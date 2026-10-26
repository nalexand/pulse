// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
namespace pulse {
using namespace juce;
Track::Mixer Studio::displayedEffectMixer(int track)const{
    auto mixer=effectMixer(track);
    if(engine->playing.load()&&project.songMode){int row=track<0?maxTracks:track;for(int id=6;id<automationTargetCount;++id)if(!isSynthAutomation(id)){float value=engine->automationReadout[(size_t)row][(size_t)id].load();if(value>=0)applyMixerAutomation(mixer,id,value);}}
    return mixer;
}
int Studio::automationTargetFor(int action,int track)const{
    if(action>=auxLevel1&&action<=auxLevel4)return automationAuxFirst+action-auxLevel1;
    if(action==masterDelayReturn)return 118;if(action==masterDelayFeedback)return 119;if(action==masterSoftClipToggle)return 120;if(action==mute&&track>=0)return 121;
    if(action==masterDial||(action==mixFader&&track<0))return 5;
    if(action>=delayRepeats&&action<=delayMix)return 6+action-delayRepeats;
    if(action>=distDrive&&action<=distOutput)return 10+action-distDrive;
    if(action>=eqLow&&action<=reverbDamping)return 18+action-eqLow;
    if(action>=chorusMix&&action<=chorusWidth)return 29+action-chorusMix;
    if(action>=flangerMix&&action<=phaserWidth)return 35+action-flangerMix;
    if(action==stereoWidth)return 47;if(action==stereoTrim)return 48;
    if(action>=dynamicsRange&&action<=dynamicsKnee)return 49+action-dynamicsRange;
    if(action==attackDial)return automationSynthFirst+12;if(action==releaseDial)return automationSynthFirst+15;
    if(action==stereoEnable)return 109;if(action==stereoMono)return 110;if(action==stereoSwap)return 111;if(action==stereoInvertLeft)return 112;if(action==stereoInvertRight)return 113;
    if(action==dynamicsModeMenu)return 114;if(action==distMode)return 115;
    switch(action){case volumeDial:case mixFader:return 0;case panDial:return 1;case cutoffDial:return 2;case driveDial:return 3;case sendDial:return 4;default:return -1;}
}
float Studio::liveParameter(int action,int track,float fallback)const{
    if(!engine->playing.load()||!project.songMode)return fallback;
    int target=action==synthValue?automationSynthFirst+track:automationTargetFor(action,track);if(action==synthValue)track=selectedTrack;
    if(target<0||target>=automationTargetCount)return fallback;int row=automationIsMaster(target,track)?maxTracks:track;if(row<0||row>maxTracks)return fallback;float value=target==5?engine->masterAutomation.load():engine->automationReadout[(size_t)row][(size_t)target].load();return value<0?fallback:value;
}
Clip* Studio::automationClip(){return selectedClip>=0&&selectedClip<(int)project.clips.size()&&project.clips[(size_t)selectedClip].isAutomation()?&project.clips[(size_t)selectedClip]:nullptr;}
bool Studio::createParameterAutomation(int target,int track,uint64_t expected){
    if(recording||recorder.isActive()||pendingNoteTool()||revision!=expected){status="Finish the take/preview and reopen automation after changes.";return false;}
    if(!validAutomationLink(target,track,(int)project.tracks.size())){status="This parameter cannot be automated on that destination.";return false;}
    if(isSynthAutomation(target)&&track>=0&&project.tracks[(size_t)track].isBus){status="Synth automation needs a source channel.";return false;}
    int count=0,points=0;for(const auto& c:project.clips)if(c.isAutomation()){++count;points+=(int)c.points.size();}
    if(project.clips.size()>=maxClips||count>=maxAutomationClips||points+2>maxTotalAutomationPoints){status="Automation limit: 64 clips and 4096 points.";return false;}
    bool master=automationIsMaster(target,track);float value=isMasterOnlyAutomation(target)?projectAutomationNormal(project,target):master?mixerAutomationNormal(project.masterMixer,target):trackAutomationNormal(project.tracks[(size_t)track],target);
    Clip c;c.track=master?selectedTrack:track;c.beat=jlimit(0.,4096.-.0625,snapped(editCursorBeat));c.length=jmin(8.,4096.-c.beat);c.automationTarget=target;c.automationTrack=master?-1:track;bool discrete=automationParameters()[(size_t)target].discrete;c.points={{0,value,discrete},{c.length,value,discrete}};
    checkpoint();project.clips.push_back(c);selectedClip=(int)project.clips.size()-1;selectedPoint=-1;project.songMode=true;changed();showPane(automationPane);status="Automation linked to "+(master?String("Master"):project.tracks[(size_t)track].name)+" / "+automationName((AutomationTarget)target)+". Static settings are retained.";return true;
}
void Studio::createAutomation(int action,int track){int target=automationTargetFor(action,track);if(target>=0)createParameterAutomation(target,track,revision);}
void Studio::showAutomationParameters(){
    if(recording||recorder.isActive()||pendingNoteTool()){status="Finish the take/preview before linking automation.";return;}int track=selectedTrack;auto sourceRevision=revision;PopupMenu menu;
    auto descriptors=automationParametersToJson();auto applicable=[&](int id){if(!isSynthAutomation(id))return true;auto allowed=descriptors[id]["instruments"].getArray();return allowed&&allowed->contains((int)project.tracks[(size_t)track].instrument)&&!project.tracks[(size_t)track].isBus;};
    auto addDestination=[&](bool master){PopupMenu destination;StringArray groups;const auto& parameters=automationParameters();for(int id=0;id<automationTargetCount;++id){const auto& p=parameters[(size_t)id];if((master?p.master:!isMasterOnlyAutomation(id))&&(master||applicable(id)))groups.addIfNotAlreadyThere(p.group);}for(const auto& group:groups){PopupMenu fields;for(int id=0;id<automationTargetCount;++id){const auto& p=parameters[(size_t)id];if(p.group==group&&(master?p.master:!isMasterOnlyAutomation(id))&&(master||applicable(id)))fields.addItem((master?1001:1)+id,p.name);}destination.addSubMenu(group,fields);}menu.addSubMenu(master?String("Master"):project.tracks[(size_t)track].name,destination);};addDestination(false);addDestination(true);auto safe=Component::SafePointer<Studio>(this);menu.showMenuAsync(menuOptions(),[safe,track,sourceRevision](int result){if(!safe||!result)return;if(safe->selectedTrack!=track){safe->status="Selected channel changed. Reopen automation.";return;}safe->createParameterAutomation(result>=1001?result-1001:result-1,result>=1001?-1:track,sourceRevision);});
}
bool Studio::setAutomationActualValue(const AutomationContext& context,int point,double value){
    if(!automationToolCurrent(context)){status="Automation clip or project changed. Reopen the value editor.";return false;}
    auto* c=automationClip();if(!c||point<0||point>=(int)c->points.size()||point!=selectedPoint){status="Selected point changed. Reopen the value editor.";return false;}
    const auto& p=automationParameters()[(size_t)c->automationTarget];
    if(c->automationTrim){if(!std::isfinite(value)||value< -100||value>100){status="Trim offset must be -100 to +100 percent of the normalized parameter range.";return false;}auto points=context.points;points[(size_t)point].value=(float)(.5+value/200);bool applied=applyAutomationPoints(context,points,false);if(applied)selectedPoint=point;return applied;}
    if(!std::isfinite(value)||value<p.low||value>p.high||(p.discrete&&value!=std::floor(value))){status="Enter a value within the parameter range; discrete values must be integers.";return false;}
    auto points=context.points;points[(size_t)point].value=automationParameterNormal(c->automationTarget,value);
    bool applied=applyAutomationPoints(context,points,false);if(applied)selectedPoint=point;return applied;
}
bool Studio::setAutomationChoice(const AutomationContext& context,int point,int choice){
    if(!automationToolCurrent(context)){status="Automation clip or project changed. Reopen the value menu.";return false;}
    auto* c=automationClip();if(!c||point<0||point>=(int)c->points.size()||point!=selectedPoint)return false;
    auto choices=automationParameterChoices(c->automationTarget);if(choice<0||choice>=choices.size())return false;
    return setAutomationActualValue(context,point,choice);
}
void Studio::showAutomationValueMenu(){
    auto context=automationToolContext();auto* c=automationClip();
    if(!context||!c||selectedPoint<0||selectedPoint>=(int)c->points.size()){status="Select a curve point first.";return;}
    auto choices=automationParameterChoices(c->automationTarget);if(c->automationTrim||choices.isEmpty()){
        const auto& p=automationParameters()[(size_t)c->automationTarget];String unit=automationParametersToJson()[c->automationTarget]["unit"].toString();auto* dialog=new AlertWindow(p.group+" / "+p.name,c->automationTrim?String("Trim offset: -100 to +100 percent of normalized range. Zero retains the underlying curve. Final values clamp to the parameter range."):"Enter the actual parameter value (not a normalized fraction). Range: "+String(p.low,5)+" to "+String(p.high,5)+(unit.isEmpty()?String():" "+unit)+(p.discrete?"; integer values.":"."),MessageBoxIconType::NoIcon,this);
        dialog->addTextEditor("automationValue",String(c->automationTrim?(c->points[(size_t)selectedPoint].value-.5f)*200:automationParameterValue(c->automationTarget,c->points[(size_t)selectedPoint].value),8));dialog->addButton("Apply",1,KeyPress(KeyPress::returnKey));dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));auto safe=Component::SafePointer<Studio>(this);int point=selectedPoint;
        dialog->enterModalState(true,ModalCallbackFunction::create([safe,dialog,context=*context,point](int result){if(!safe||result!=1)return;auto text=dialog->getTextEditorContents("automationValue").trim();const char* begin=text.toRawUTF8();char* end=nullptr;double value=std::strtod(begin,&end);if(text.isEmpty()||end==begin||*end!='\0'||!std::isfinite(value)){safe->status="Enter a finite numeric parameter value.";return;}safe->setAutomationActualValue(context,point,value);}),true);return;
    }
    int point=selectedPoint,current=(int)automationParameterValue(c->automationTarget,c->points[(size_t)point].value);PopupMenu menu;
    for(int i=0;i<choices.size();++i)menu.addItem(i+1,choices[i],true,i==current);
    auto safe=Component::SafePointer<Studio>(this);menu.showMenuAsync(menuOptions(automationValueMenu),[safe,context=*context,point](int result){if(safe&&result>0)safe->setAutomationChoice(context,point,result-1);});
}
void Studio::deleteAutomationPoint(){
    auto* c=automationClip();if(!c||selectedPoint<=0||selectedPoint>=(int)c->points.size()-1)return;
    checkpoint();c->points.erase(c->points.begin()+selectedPoint);selectedPoint=-1;changed();
}
void Studio::editAutomationClipboard(int action){
    auto* c=automationClip();if(!c)return;
    bool copying=action==copySelection||action==cutSelection,duplicate=action==duplicateSelection;
    if(copying||duplicate){
        if(selectedPoint<0||selectedPoint>=(int)c->points.size()){status="Select a curve point first.";return;}
        automationClipboard={c->points[(size_t)selectedPoint]};
        if(copying){status="Automation point copied. Paste at the curve cursor.";if(action==cutSelection)deleteAutomationPoint();return;}
    }
    if(automationClipboard.empty())return;
    auto point=automationClipboard.front();double at=duplicate?point.beat+jmax(.25,snap):automationCursor;
    if(at<0||at>c->length){status="Paste position must be inside the automation clip.";return;}
    at=jlimit(0.,c->length,snapped(at));int index=0;while(index<(int)c->points.size()&&c->points[(size_t)index].beat<at)++index;
    bool replace=index<(int)c->points.size()&&std::abs(c->points[(size_t)index].beat-at)<.00001;
    if(!replace){int total=0;for(const auto& clip:project.clips)total+=(int)clip.points.size();if(c->points.size()>=maxAutomationPoints||total>=maxTotalAutomationPoints){status="Automation point limit reached.";return;}}
    checkpoint();point.beat=at;if(replace)c->points[(size_t)index]=point;else c->points.insert(c->points.begin()+index,point);
    selectedPoint=index;changed();
}
bool Studio::automationMouseDown(const Hit& hit,const MouseEvent& e){
    if(hit.action!=automationPoint&&hit.action!=automationGrid)return false;
    auto* c=automationClip();if(!c)return true;
    if(hit.action==automationPoint){selectedPoint=hit.data;
        if(e.mods.isRightButtonDown()||editorTool==2){deleteAutomationPoint();return true;}
    }else{
        if(e.mods.isRightButtonDown()||editorTool==2)return true;
        double at=jlimit(0.,c->length,snapped((e.position.x-automationRect.getX())/automationRect.getWidth()*c->length,e.mods.isAltDown()));
        int index=0;while(index<(int)c->points.size()&&c->points[(size_t)index].beat<at)++index;
        if(index<(int)c->points.size()&&std::abs(c->points[(size_t)index].beat-at)<.00001)selectedPoint=index;
        else{
            if(c->points.size()>=maxAutomationPoints){status="This automation clip supports up to 256 points.";return true;}
            int total=0;for(const auto& clip:project.clips)total+=(int)clip.points.size();if(total>=maxTotalAutomationPoints){status="The project supports up to 4096 automation points.";return true;}
            checkpoint();c->points.insert(c->points.begin()+index,{at,jlimit(0.f,1.f,(automationRect.getBottom()-e.position.y)/automationRect.getHeight()),false});selectedPoint=index;changed();
            dragAction=automationPoint;dragData=selectedClip;dragExtra=selectedPoint;return true;
        }
    }
    checkpoint();dragAction=automationPoint;dragData=selectedClip;dragExtra=selectedPoint;return true;
}
void Studio::automationMouseDrag(const MouseEvent& e){
    if(dragData!=selectedClip)return;auto* c=automationClip();if(!c||dragExtra<0||dragExtra>=(int)c->points.size())return;
    auto& point=c->points[(size_t)dragExtra];float v=jlimit(0.f,1.f,(automationRect.getBottom()-e.position.y)/automationRect.getHeight());
    if(dragExtra>0&&dragExtra<(int)c->points.size()-1){double at=snapped((e.position.x-automationRect.getX())/automationRect.getWidth()*c->length,e.mods.isAltDown());
        double previous=c->points[(size_t)dragExtra-1].beat,next=c->points[(size_t)dragExtra+1].beat,epsilon=jmin(.00001,(next-previous)*.25);point.beat=jlimit(previous+epsilon,next-epsilon,at);}
    point.value=v;changed();
}
void Studio::paintAutomation(Graphics& g,Rectangle<float> r){
    const Colour text(0xffd9ddde),accent(0xffd3af71),muted(0xffadb7bd);
    g.setColour(Colour(0xff3b464c));g.fillRect(r);auto* c=automationClip();
    if(!c){label(g,"Select an automation clip, or link a synth/effect parameter.",r.reduced(15).withHeight(30),12,text);button(g,"New / link parameter...",{r.getX()+15,r.getY()+44,190,27},automationLink);return;}
    const bool trimCapture=automationWriteContext&&automationWriteModeValue==4,trimView=c->automationTrim||trimCapture;
    label(g,(automationIsMaster(c->automationTarget,c->automationTrack)?"Master":project.tracks[(size_t)c->automationTrack].name)+" - "+(trimView?String("Trim / "):String())+automationName((AutomationTarget)c->automationTarget),{r.getX()+11,r.getY()+3,r.getWidth()-400,25},12,text,true);
    button(g,"Shape...",{r.getRight()-387,r.getY()+4,87,23},automationShapeOpen);
    button(g,"Generate...",{r.getX()+11,r.getY()+33,125,23},automationGenerateOpen);button(g,"New / link...",{r.getX()+145,r.getY()+33,110,23},automationLink);
    if(selectedPoint>=0&&selectedPoint<(int)c->points.size())button(g,"Value: "+automationClipValueText(*c,c->points[(size_t)selectedPoint].value),{r.getX()+265,r.getY()+33,160,23},automationValueMenu);
    bool hold=selectedPoint>=0&&selectedPoint<(int)c->points.size()&&c->points[(size_t)selectedPoint].hold;
    button(g,hold?"STEP":selectedPoint>=0&&selectedPoint<(int)c->points.size()?String(automationCurveName(c->points[(size_t)selectedPoint].curve)):"LINEAR",{r.getRight()-294,r.getY()+4,69,23},automationHold,0,hold);
    button(g,"Reset curve",{r.getRight()-220,r.getY()+4,87,23},automationReset);button(g,"Playlist",{r.getRight()-128,r.getY()+4,70,23},arrangeTab);
    button(g,snap>0?"Snap":"Free",{r.getRight()-53,r.getY()+4,47,23},snapMenu);
    paintAutomationWrite(g,{r.getX()+11,r.getY()+62,r.getWidth()-22,26});
    automationRect={r.getX()+48,r.getY()+112,r.getWidth()-63,r.getHeight()-141};
    g.setColour(Colour(0xff293439));g.fillRect(automationRect);
    for(int n=0;n<=4;++n){float y=automationRect.getBottom()-n*.25f*automationRect.getHeight();g.setColour(Colour(0xff58656b));g.drawHorizontalLine((int)y,automationRect.getX(),automationRect.getRight());label(g,String(trimView?n*50-100:n*25)+"%",{r.getX()+3,y-8,40,16},10,muted,false,Justification::centredRight);}
    double step=jmax(1.,std::ceil(c->length/16));for(double beat=0;beat<=c->length;beat+=step){float x=automationRect.getX()+(float)(beat/c->length)*automationRect.getWidth();g.setColour(Colour(0xff46535a));g.drawVerticalLine((int)x,automationRect.getY(),automationRect.getBottom());label(g,String(beat,0),{x+3,automationRect.getY()-21,45,18},10,muted);}
    hits.push_back({automationRect,automationGrid});
    Path curve;int pixels=(int)automationRect.getWidth();for(int x=0;x<=pixels;++x){float y=automationRect.getBottom()-([&]{float v=trimCapture&&!c->automationTrim?.5f:automationValue(c->points.data(),(int)c->points.size(),x/(double)pixels*c->length);return !c->automationTrim&&automationParameters()[(size_t)c->automationTarget].discrete?automationParameterNormal(c->automationTarget,automationParameterValue(c->automationTarget,v)):v;})()*automationRect.getHeight();if(x==0)curve.startNewSubPath(automationRect.getX(),y);else curve.lineTo(automationRect.getX()+x,y);}
    auto area=curve;area.lineTo(automationRect.getRight(),automationRect.getBottom());area.lineTo(automationRect.getX(),automationRect.getBottom());area.closeSubPath();g.setColour(accent.withAlpha(.12f));g.fillPath(area);g.setColour(accent);g.strokePath(curve,PathStrokeType(1.8f));
    if(!trimCapture||c->automationTrim)for(int i=0;i<(int)c->points.size();++i){const auto& p=c->points[(size_t)i];if(p.beat>c->length)continue;float x=automationRect.getX()+(float)(p.beat/c->length)*automationRect.getWidth(),y=automationRect.getBottom()-p.value*automationRect.getHeight();g.setColour(i==selectedPoint?Colours::white:accent);g.fillEllipse(x-4,y-4,8,8);hits.push_back({{x-7,y-7,14,14},automationPoint,i});}
    if(automationWriteContext)if(const auto* take=engine->automationWrite.result()){
        g.setColour(Colour(0xffff963f));
        for(int n=0;n<take->segmentCount;++n){const auto& segment=take->segments[(size_t)n];Path recorded;
            for(int i=0;i<segment.count;++i){const auto& point=take->points[(size_t)(segment.first+i)];float x=automationRect.getX()+(float)((point.beat-c->beat)/c->length)*automationRect.getWidth(),y=automationRect.getBottom()-point.value*automationRect.getHeight();if(i==0)recorded.startNewSubPath(x,y);else{recorded.lineTo(x,automationRect.getBottom()-take->points[(size_t)(segment.first+i-1)].value*automationRect.getHeight());recorded.lineTo(x,y);}}
            g.strokePath(recorded,PathStrokeType(2.5f));
        }
    }
    if(engine->playing.load()&&project.songMode){double at=engine->position.load()-c->beat;if(at>=0&&at<=c->length){g.setColour(Colour(0xffb2c98d));g.drawVerticalLine((int)(automationRect.getX()+at/c->length*automationRect.getWidth()),automationRect.getY(),automationRect.getBottom());}}
    String value=trimCapture?String("Trim offsets / centre = zero. Original curves are retained; orange shows the relative take."):selectedPoint>=0&&selectedPoint<(int)c->points.size()?"Point "+String(selectedPoint+1)+"  |  "+String(c->points[(size_t)selectedPoint].beat,3)+" beats  |  "+automationClipValueText(*c,c->points[(size_t)selectedPoint].value)+" ("+String(c->points[(size_t)selectedPoint].value*100,1)+"%)  |  "+String(c->points[(size_t)selectedPoint].hold?"Hold":automationCurveName(c->points[(size_t)selectedPoint].curve))+" / tension "+String(c->points[(size_t)selectedPoint].tension,2):"Click to add points; drag to edit; right-click deletes interior points.";
    label(g,value,{automationRect.getX(),r.getBottom()-24,automationRect.getWidth(),20},10,muted);
}
}
