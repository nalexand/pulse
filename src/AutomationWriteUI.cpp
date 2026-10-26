// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <cmath>
namespace pulse {
using namespace juce;
namespace {
bool usable(const AutomationWrite::Take& take){return take.end==AutomationWrite::End::complete||take.end==AutomationWrite::End::stopped;}
String writeEnd(AutomationWrite::End end){
    switch(end){case AutomationWrite::End::complete:return "Range complete";case AutomationWrite::End::stopped:return "Stopped";case AutomationWrite::End::cancelled:return "Discarded";case AutomationWrite::End::discontinuity:return "Seek/loop interrupted the pass";case AutomationWrite::End::changed:return "Project changed during the pass";case AutomationWrite::End::capacity:return "Take exceeded 16 gestures / 256 points";}return {};
}
}
bool Studio::beginAutomationWrite(){
    if(automationWriteContext){status="Apply or Discard the previous automation pass first.";return false;}
    auto context=automationToolContext();auto* c=automationClip();
    if(!context||!c||!project.songMode||automationWriteModeValue<1||automationWriteModeValue>4||exporting.load()){
        status="Select an automation clip, switch to SONG and choose Touch, Latch, Write or Trim before Record pass.";return false;
    }
    double end=c->beat+c->length;
    const auto* section=project.loop&&project.loopSection.isNotEmpty()?findSongMarker(project,project.loopSection):nullptr;
    if(end>4096||c->length>600||(section&&(c->beat<section->beat||end>section->endBeat))){status="The complete automation clip must fit inside the Song loop range and 600 beats.";return false;}
    if(audioEnabled&&!audioReady())return false;
    const int target=c->automationTarget,destination=automationIsMaster(target,c->automationTrack)?-1:c->automationTrack;
    if(automationWriteModeValue==4&&automationParameters()[(size_t)target].discrete){status="Trim requires a continuous parameter. Use Touch, Latch or Write for switches, counts and selectors.";return false;}
    float initial=automationWriteModeValue==4?.5f:projectAutomationAt(project,target,destination,c->beat);
    AutomationWrite::Plan plan{target,destination,c->beat,end,revision,automationWriteModeValue==1?AutomationWrite::Mode::touch:automationWriteModeValue==2?AutomationWrite::Mode::latch:automationWriteModeValue==3?AutomationWrite::Mode::write:AutomationWrite::Mode::trim,initial};
    // Reset and seek are consumed before the newly armed pass starts in render().
    engine->stop();engine->seekRequested.store(c->beat);
    if(!engine->automationWrite.begin(plan,true)){status="The previous audio callback has not released automation capture yet. Restart the audio device if callbacks stopped.";return false;}
    releaseKeys();automationWriteContext=*context;automationWriteNotified=false;automationWriteNativeAction=0;automationWriteValue=initial;engine->start();engine->automationWrite.activate();
    lastPlaybackFrames=engine->processedFrames.load();stalledPlaybackTicks=0;
    status=automationWriteModeValue==4?"Trim offsets existing curves while touched; centre is zero. Release returns to the original. Review before Apply.":automationWriteModeValue==3?"Write records the full selected range, starting at its current value. Drag Write or the linked knob/fader. Review before Apply.":"Drag Write or the linked knob/fader. Touch returns on release; Latch holds after release. Finish, then review and Apply.";repaint();return true;
}
void Studio::tickAutomationWrite(){
    if(!automationWriteContext)return;
    if(auto* take=engine->automationWrite.result()){
        if(!automationWriteNotified){automationWriteNotified=true;engine->playing.store(false);status=writeEnd(take->end)+": "+String(take->segmentCount)+" gestures, "+String(take->pointCount)+" points. "+(usable(*take)&&take->segmentCount>0?"Review the orange take, then Apply or Discard.":"No changes applied. Discard and retry.");repaint();}
    }else if(revision!=automationWriteContext->revision)engine->automationWrite.cancel();
}
void Studio::discardAutomationWrite(){
    engine->automationWrite.cancel();engine->playing.store(false);automationWriteContext.reset();automationWriteNotified=false;
    if(dragAction==automationWriteControl)dragAction=0;
    automationWriteNativeAction=0;
    status="Automation pass discarded. Existing curves and static controls are retained.";repaint();
}
bool Studio::applyAutomationWrite(){
    const auto* take=engine->automationWrite.result();
    if(!automationWriteContext||!take||!usable(*take)||take->segmentCount==0){status="Finish a nonempty automation pass before Apply.";return false;}
    if(!automationToolCurrent(*automationWriteContext)){status="Project or selected curve changed. Discard the stale automation pass.";return false;}
    auto next=project;int first=(int)next.clips.size();int total=0,count=0;
    for(const auto& clip:next.clips)if(clip.isAutomation()){++count;total+=(int)clip.points.size();}
    if(first+take->segmentCount>maxClips||count+take->segmentCount>maxAutomationClips||total+take->pointCount>maxTotalAutomationPoints){status="The pass will exceed 512 clips, 64 automation clips or 4096 points. Discard it and free space first.";return false;}
    const auto& source=project.clips[(size_t)automationWriteContext->clip];
    for(int i=0;i<take->segmentCount;++i){const auto& segment=take->segments[(size_t)i];
        Clip clip;clip.track=source.track;clip.beat=segment.start;clip.length=segment.end-segment.start;clip.automationTarget=take->plan.target;clip.automationTrack=take->plan.track;clip.automationTrim=take->plan.mode==AutomationWrite::Mode::trim;
        for(int n=0;n<segment.count;++n){const auto& point=take->points[(size_t)(segment.first+n)];clip.points.push_back({point.beat-segment.start,point.value,true});}
        next.clips.push_back(std::move(clip));
    }
    // Compile both modes and roundtrip the real schema before any Undo/project edit.
    Project validated;auto valid=projectFromJson(projectToJson(next),validated);
    auto playback=std::make_unique<Playback>();String warnings;auto base=currentFile==File()?root:currentFile.getParentDirectory();
    if(valid.wasOk())valid=compilePlayback(next,assets,base,*playback,warnings);
    if(valid.wasOk()){next.songMode=false;valid=compilePlayback(next,assets,base,*playback,warnings);next.songMode=true;}
    if(valid.failed()){status="Automation pass could not apply: "+valid.getErrorMessage();return false;}
    checkpoint();project=std::move(next);selectedClip=first;selectedPoint=-1;automationWriteContext.reset();automationWriteNotified=false;engine->playing.store(false);changed();
    status="Automation pass applied as "+String(take->segmentCount)+" overlay clips. Trim adds signed offsets; originals remain underneath. One Undo removes this entire pass.";return true;
}
bool Studio::automationWriteAction(int action,int data){
    if(action==automationWriteMode){
        if(automationWriteContext){status="Apply or Discard before changing write mode.";return true;}
        if(data>=1&&data<=5){automationWriteModeValue=data-1;return true;}
        PopupMenu menu;menu.addItem(1,"Read — existing curves",true,automationWriteModeValue==0);menu.addItem(2,"Touch — return on release",true,automationWriteModeValue==1);menu.addItem(3,"Latch — hold after release",true,automationWriteModeValue==2);
        menu.addItem(4,"Write — record the entire range",true,automationWriteModeValue==3);
        menu.addItem(5,"Trim — offset while touched",true,automationWriteModeValue==4);
        auto safe=Component::SafePointer<Studio>(this);menu.showMenuAsync(menuOptions(automationWriteMode),[safe](int result){if(safe&&result)safe->perform(automationWriteMode,result);});return true;
    }
    if(action==automationWriteStart){if(automationWriteContext){engine->playing.store(false);status="Finishing automation pass at the next audio callback.";}else beginAutomationWrite();return true;}
    if(action==automationWriteApply){applyAutomationWrite();return true;}
    if(action==automationWriteDiscard){discardAutomationWrite();return true;}
    return false;
}
bool Studio::automationWriteMouseDown(const Hit& hit,const MouseEvent& e){
    const bool dedicated=hit.action==automationWriteControl;
    if(!dedicated&&(!automationWriteContext||(!isParameter(hit.action)&&hit.action!=synthValue)))return false;
    auto phase=engine->automationWrite.phase();
    if(!automationWriteContext||(phase!=AutomationWrite::Phase::armed&&phase!=AutomationWrite::Phase::running)){status="Choose Touch, Latch, Write or Trim, then Record pass before dragging a linked control.";return true;}
    if(!e.mods.isLeftButtonDown())return true;
    automationWriteNativeAction=0;
    if(dedicated)automationWriteValue=jlimit(0.f,1.f,(e.position.x-hit.rect.getX())/jmax(1.f,hit.rect.getWidth()));
    else {
        const auto& source=project.clips[(size_t)automationWriteContext->clip];
        const int target=hit.action==synthValue?automationSynthFirst+hit.data:automationTargetFor(hit.action,hit.data);
        const int destination=hit.action==synthValue?selectedTrack:hit.data;
        const bool master=automationIsMaster(source.automationTarget,source.automationTrack);
        if(target!=source.automationTarget||automationIsMaster(target,destination)!=master||(!master&&destination!=source.automationTrack)){
            status="Only the selected automation parameter/destination records in this pass. Finish and select another link to record it.";return true;
        }
        automationWriteNativeAction=hit.action;automationWriteNativeRect=hit.rect;dragOrigin=e.position;
        const int row=master?maxTracks:destination;
        float live=engine->automationReadout[(size_t)row][(size_t)target].load();
        dragValue=automationWriteModeValue==4?.5f:live<0?automationWriteValue:live;automationWriteValue=dragValue;
        if(hit.action==mixFader&&automationWriteModeValue!=4)automationWriteValue=jlimit(0.f,1.f,(hit.rect.getBottom()-8-e.position.y)/jmax(1.f,hit.rect.getHeight()-16));
    }
    dragAction=automationWriteControl;engine->automationWrite.control(automationWriteValue,true);repaint();return true;
}
void Studio::automationWriteMouseDrag(const MouseEvent& e){
    if(!automationWriteContext)return;
    auto phase=engine->automationWrite.phase();if(phase!=AutomationWrite::Phase::armed&&phase!=AutomationWrite::Phase::running)return;
    float value;
    if(!automationWriteNativeAction)value=(e.position.x-automationWriteRect.getX())/jmax(1.f,automationWriteRect.getWidth());
    else if(automationWriteNativeAction==mixFader&&automationWriteModeValue!=4)value=(automationWriteNativeRect.getBottom()-8-e.position.y)/jmax(1.f,automationWriteNativeRect.getHeight()-16);
    else {float factor=e.mods.isCtrlDown()?.1f:1.f;value=dragValue+((e.position.x-dragOrigin.x)/300.f-(e.position.y-dragOrigin.y)/180.f)*factor;}
    automationWriteValue=jlimit(0.f,1.f,value);engine->automationWrite.control(automationWriteValue,true);repaint();
}
bool Studio::automationWriteNavigation(int action,int data)const{
    switch(action){
        case paneMove:case paneResize:case paneMaximize:case paneClose:return data>=0&&data<paneCount;
        case trackSelect:case channelOpen:return data>=0&&data<(int)project.tracks.size();
        case auxLevelsOpen:case composeTab:case arrangeTab:case mixTab:case automationOpen:case synthOpen:case synthChannel:case synthTab:case effectsOpen:case effectsTab:case masterEffectsOpen:case masterInsertsOpen:case masterStereoOpen:case stereoOpen:return true;
        default:return false;
    }
}
void Studio::paintAutomationWrite(Graphics& g,Rectangle<float> r){
    const char* modes[]={"Read","Touch","Latch","Write","Trim"};button(g,modes[jlimit(0,4,automationWriteModeValue)],r.removeFromLeft(64),automationWriteMode);r.removeFromLeft(5);
    bool ready=automationWriteContext&&engine->automationWrite.result();
    button(g,automationWriteContext?(ready?"Finished":"Finish pass"):"Record pass",r.removeFromLeft(92),automationWriteStart,0,automationWriteContext.has_value());r.removeFromLeft(5);
    if(automationWriteContext){button(g,"Discard",r.removeFromRight(65),automationWriteDiscard);r.removeFromRight(5);if(ready){button(g,"Apply",r.removeFromRight(55),automationWriteApply);r.removeFromRight(5);}}
    automationWriteRect=r;
    g.setColour(Colour(0xff263238));g.fillRect(r);g.setColour(Colour(0xffd3af71));g.fillRect(r.withWidth(r.getWidth()*automationWriteValue).reduced(0,3));
    label(g,automationWriteModeValue==4?"Trim: "+String((automationWriteValue-.5f)*200,1)+"% range":"Write: "+automationParameterText(automationClip()->automationTarget,automationWriteValue),r.reduced(5,0),11,Colours::white);
    hits.push_back({r,automationWriteControl});
}
}
