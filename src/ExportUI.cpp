// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <cmath>
namespace pulse {
using namespace juce;
void Studio::restoreExportSettings(){
    auto file=root.getChildFile("data/export-settings.json");if(!file.existsAsFile()||file.getSize()>4096)return;auto json=JSON::parse(file.loadFileAsString());if(!json.isObject()||(int)json["schema"]!=1)return;
    RenderSettings next;next.format=(RenderFormat)(int)json["format"];next.sampleRate=(int)json["rate"];next.bits=(int)json["bits"];next.channels=(int)json["channels"];next.tailSeconds=(double)json["tail"];next.targetDb=(double)json["targetDb"];next.normalize=(bool)json["normalize"];next.dither=(bool)json["dither"];
    RenderPlan plan;if(planRender(project,next,plan).wasOk()){if(next.bits==32)next.dither=false;renderSettings=next;}
}
void Studio::saveExportSettings(){
    auto* d=new DynamicObject();d->setProperty("schema",1);d->setProperty("format",(int)renderSettings.format);d->setProperty("rate",renderSettings.sampleRate);d->setProperty("bits",renderSettings.bits);d->setProperty("channels",renderSettings.channels);d->setProperty("tail",renderSettings.tailSeconds);d->setProperty("targetDb",renderSettings.targetDb);d->setProperty("normalize",renderSettings.normalize);d->setProperty("dither",renderSettings.dither);
    TemporaryFile temporary(root.getChildFile("data/export-settings.json"));if(!temporary.getFile().replaceWithText(JSON::toString(var(d),false))||!temporary.overwriteTargetFileWithTemporary())status="Export settings could not be saved locally.";
}
void Studio::showExport(bool isolated){
    if(!exporting.load()){renderIsolatedTracks=isolated;RenderPlan plan;if(planRender(project,renderSettings,plan).failed()){renderSettings.startBeat=0;renderSettings.endBeat=-1;}}
    showPane(exportPane);
}
void Studio::paintExport(Graphics& g,Rectangle<float> r){
    Colour text(0xffd9ddde),muted(0xffa5b0b2),accent(0xffb8c997);g.setColour(Colour(0xff394349));g.fillRect(r);float x=r.getX()+14,y=r.getY()+12;
    bool song=renderSettings.source==RenderSource::current?project.songMode:renderSettings.source==RenderSource::song;
    label(g,"Source",{x,y,80,26},12,text,true);button(g,"Song",{x+84,y,87,26},renderSource,1,song);button(g,"Pattern",{x+178,y,87,26},renderSource,2,!song);
    button(g,"Master mix",{x+333,y,131,26},renderIsolated,0,!renderIsolatedTracks);button(g,"Isolated tracks",{x+471,y,142,26},renderIsolated,1,renderIsolatedTracks);
    label(g,project.title+" / "+(song?String("Song"):project.patterns[(size_t)project.activePattern].name),{x,y+38,r.getWidth()-28,24},13,text,true);
    g.setColour(Colour(0xff59656a));g.drawHorizontalLine((int)(y+72),x,r.getRight()-14);
    label(g,"File format",{x,y+88,140,26},12,text);button(g,"WAV",{x+155,y+88,103,26},renderFormat,0,renderSettings.format==RenderFormat::wav);button(g,"FLAC",{x+266,y+88,103,26},renderFormat,1,renderSettings.format==RenderFormat::flac);
    label(g,"Sample rate",{x,y+126,140,26},12,text);const int rates[]={44100,48000,96000};const char* names[]={"44.1 kHz","48 kHz","96 kHz"};for(int i=0;i<3;++i)button(g,names[i],{x+155+i*111,y+126,103,26},renderRate,rates[i],renderSettings.sampleRate==rates[i]);
    label(g,"Bit depth",{x,y+164,140,26},12,text);const int bits[]={16,24,32};for(int i=0;i<3;++i)button(g,i==2?"32-bit float":String(bits[i])+"-bit PCM",{x+155+i*111,y+164,103,26},renderBits,bits[i],renderSettings.bits==bits[i]);
    label(g,"Channels",{x,y+202,140,26},12,text);button(g,"Stereo",{x+155,y+202,103,26},renderChannels,2,renderSettings.channels==2);button(g,"Mono",{x+266,y+202,103,26},renderChannels,1,renderSettings.channels==1);
    button(g,"Full length",{x,y+246,123,26},renderFull,0,renderSettings.startBeat==0&&renderSettings.endBeat<0);button(g,"Set range...",{x+131,y+246,123,26},renderRange);button(g,"Use punch range",{x+262,y+246,151,26},renderPunch);
    RenderPlan plan;auto result=planRender(project,renderSettings,plan);label(g,result.wasOk()?"Bar "+String(renderSettings.startBeat/4+1,3)+" to "+String(plan.endBeat/4+1,3)+" (end excluded)  /  "+String(plan.outputFrames/(double)renderSettings.sampleRate,2)+" seconds with tail":result.getErrorMessage(),{x,y+281,r.getWidth()-28,23},11,result.wasOk()?muted:Colour(0xffed7567));
    label(g,"Tail",{x,y+319,140,26},12,text);const int tails[]={0,2,5,10};for(int i=0;i<4;++i)button(g,String(tails[i])+" sec",{x+155+i*82,y+319,74,26},renderTail,tails[i],renderSettings.tailSeconds==tails[i]);
    button(g,"Peak normalize",{x,y+359,178,27},renderNormalize,0,renderSettings.normalize);button(g,"Target: "+String(renderSettings.targetDb,1)+" dBFS",{x+187,y+359,190,27},renderTarget);
    button(g,"TPDF dither",{x,y+399,178,27},renderDither,0,renderSettings.dither&&renderSettings.bits!=32);label(g,renderSettings.bits==32?"Float output: dither is off.":"Optional final quantization noise for integer output.",{x+187,y+400,r.getWidth()-215,25},11,muted);
    label(g,renderIsolatedTracks?"Isolated files include routed buses/effects and may not sum to the master.":"Monitor input, Browser preview and metronome are excluded.",{x,y+439,r.getWidth()-28,22},11,muted);
    Rectangle<float> progress{x,y+478,r.getWidth()-28,9};g.setColour(Colour(0xff242e32));g.fillRect(progress);g.setColour(accent);g.fillRect(progress.withWidth(progress.getWidth()*jlimit(0.f,1.f,exportProgress.load())));
    label(g,exporting.load()?"Rendering snapshot: "+String((int)(exportProgress.load()*100))+"%  /  You can keep editing.":"Choose a local destination. Existing output is replaced after successful completion.",{x,y+494,r.getWidth()-28,22},11,muted);
    button(g,renderIsolatedTracks?"Choose folder and export...":"Choose file and export...",{x,r.getBottom()-43,218,29},renderChoose,0,false,true);button(g,"Cancel render",{x+228,r.getBottom()-43,135,29},renderCancel);
}
bool Studio::performExportAction(int action,int data){
    if(action<renderSource||action>renderCancel)return false;
    if(action==renderCancel){if(exporting.load()){cancelExport.store(true);status="Cancelling export; completed isolated files are kept.";}return true;}
    if(recording){status="Finish the take before changing export settings.";return true;}
    if(exporting.load()){status="A render is running. Its settings are fixed; Cancel render or Escape stops it.";return true;}
    if(action==renderChoose){RenderPlan plan;auto result=planRender(project,renderSettings,plan);if(result.failed())status=result.getErrorMessage();else chooseFile(renderIsolatedTracks?exportStems:exportAudio);return true;}
    if(action==renderRange||action==renderTarget){
        bool target=action==renderTarget;auto* dialog=new AlertWindow(target?"Normalization target":"Export range",target?"Sample-peak target in dBFS (-24 to 0). This is not loudness normalization.":"Bars are 1-based. The end is excluded; source playback before the start warms up notes and effects.",MessageBoxIconType::NoIcon,this);
        RenderPlan plan;planRender(project,renderSettings,plan);dialog->addTextEditor("a",String(target?renderSettings.targetDb:renderSettings.startBeat/4+1,4),target?"Target dBFS":"Start bar");if(!target)dialog->addTextEditor("b",String(plan.endBeat/4+1,4),"End bar");dialog->addButton("Apply",1,KeyPress(KeyPress::returnKey));dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));auto safe=Component::SafePointer<Studio>(this);
        dialog->enterModalState(true,ModalCallbackFunction::create([safe,dialog,target](int r){if(!safe||r!=1||safe->recording||safe->exporting.load())return;auto text=dialog->getTextEditorContents("a").trim(),other=target?String("0"):dialog->getTextEditorContents("b").trim();if(text.isEmpty()||other.isEmpty()||!text.containsOnly("0123456789.-")||!other.containsOnly("0123456789.-")){safe->status="Enter numeric export values.";return;}auto next=safe->renderSettings;if(target)next.targetDb=text.getDoubleValue();else{next.startBeat=(text.getDoubleValue()-1)*4;next.endBeat=(other.getDoubleValue()-1)*4;}RenderPlan checked;auto result=planRender(safe->project,next,checked);if(result.failed()){safe->status=result.getErrorMessage();return;}safe->renderSettings=next;safe->saveExportSettings();safe->repaint();}),true);return true;
    }
    if(action==renderSource){renderSettings.source=data==1?RenderSource::song:RenderSource::pattern;renderSettings.startBeat=0;renderSettings.endBeat=-1;}
    else if(action==renderFormat){renderSettings.format=data==1?RenderFormat::flac:RenderFormat::wav;if(renderSettings.format==RenderFormat::flac&&renderSettings.bits==32)renderSettings.bits=24;}
    else if(action==renderRate){if(data==44100||data==48000||data==96000)renderSettings.sampleRate=data;}
    else if(action==renderBits){if(data==16||data==24||data==32){if(data==32&&renderSettings.format==RenderFormat::flac){status="FLAC supports 16- or 24-bit PCM. Choose WAV for 32-bit float.";return true;}renderSettings.bits=data;if(data==32)renderSettings.dither=false;}}
    else if(action==renderChannels)renderSettings.channels=data==1?1:2;
    else if(action==renderFull){renderSettings.startBeat=0;renderSettings.endBeat=-1;}
    else if(action==renderPunch){auto next=renderSettings;next.startBeat=project.recording.punchStart;next.endBeat=project.recording.punchEnd;RenderPlan plan;auto result=planRender(project,next,plan);if(result.failed()){status=result.getErrorMessage();return true;}renderSettings=next;}
    else if(action==renderTail)renderSettings.tailSeconds=jlimit(0,10,data);
    else if(action==renderNormalize)renderSettings.normalize=!renderSettings.normalize;
    else if(action==renderDither){if(renderSettings.bits==32){status="Dither is disabled for floating-point output.";return true;}renderSettings.dither=!renderSettings.dither;}
    else if(action==renderIsolated)renderIsolatedTracks=data!=0;
    saveExportSettings();return true;
}
}
