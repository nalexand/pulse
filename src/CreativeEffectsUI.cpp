// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <cstdlib>
#include <cmath>
namespace pulse {
using namespace juce;
void Studio::paintCreativeEffects(Graphics& g,Rectangle<float> r){
    bool echo=selectedEffect==6;const auto fx=displayedEffectMixer(effectTarget());float column=(r.getWidth()-20)/2,y=r.getY();
    if(!echo){const char* modes[]={"Soft / tube","Hard clip","Fuzz"};for(int i=0;i<3;++i)button(g,modes[i],{r.getX()+i*r.getWidth()/3,y,r.getWidth()/3-6,24},distMode,i,fx.distortion.mode==i);y+=32;}
    int count=echo?4:8;for(int i=0;i<count;++i){auto spec=echo?echoSpecs()[(size_t)i]:distortionSpecs()[(size_t)i];double value=echo?echoValue(fx.delay,i):distortionValue(fx.distortion,i);bool percent=String(spec.unit).isEmpty()&&!spec.integer;String text=String(spec.name)+"  "+String(percent?value*100:value,spec.integer||percent||i==1||i==2?0:1)+(percent?"%":spec.unit);dial(g,text,{r.getX()+(i%2)*(column+20),y+(i/2)*(echo?48.f:37.f),column,32},(echo?delayRepeats:distDrive)+i,(float)creativeNormal(spec,value),effectTarget());}
    if(echo){double bpm=project.bpm;label(g,String(fx.delay.repeats)+" echoes, "+String(fx.delay.interval*bpm/60000,2)+" beats apart at this tempo.",{r.getX(),y+110,r.getWidth(),23},12,Colour(0xffb2c98d));label(g,"Fade is the level lost after each repeat. 0% keeps equal echoes; 100% leaves only the first.",{r.getX(),y+140,r.getWidth(),31},11,Colour(0xffadb7bd));label(g,"Count or interval changes clear old echoes. For exports, choose a tail long enough for all repeats.",{r.getX(),y+178,r.getWidth(),31},11,Colour(0xffadb7bd));}
    else{const char* presets[]={"Start: Crunch","Start: Lead","Start: Fuzz"};for(int i=0;i<3;++i)button(g,presets[i],{r.getX()+i*r.getWidth()/3,y+158,r.getWidth()/3-6,24},distPreset,i);label(g,"4x waveshaping / DC removal / cabinet colour. Right-click knobs for exact values.",{r.getX(),y+190,r.getWidth(),26},10,Colour(0xffadb7bd));}
}
void Studio::editCreativeParameter(int action,int track){
    if(action<delayRepeats||action>distOutput||track< -1||track>=(int)project.tracks.size()||recording||recorder.isActive())return;
    bool echo=action<=delayMix;int i=action-(echo?delayRepeats:distDrive);auto spec=echo?echoSpecs()[(size_t)i]:distortionSpecs()[(size_t)i];bool percent=String(spec.unit).isEmpty()&&!spec.integer;double scale=percent?100:1;const auto& fx=effectMixer(track);
    auto* dialog=new AlertWindow((echo?String("Delay / "):String("Distortion / "))+spec.name,"Allowed: "+String(spec.low*scale)+" to "+String(spec.high*scale)+(percent?"%":spec.unit),MessageBoxIconType::NoIcon,this);dialog->addTextEditor("value",String((echo?echoValue(fx.delay,i):distortionValue(fx.distortion,i))*scale,3));dialog->addButton("Apply",1,KeyPress(KeyPress::returnKey));dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));auto safe=Component::SafePointer<Studio>(this);auto sourceRevision=revision;
    dialog->enterModalState(true,ModalCallbackFunction::create([safe,dialog,track,sourceRevision,echo,i,scale](int result){if(!safe||result!=1)return;if(safe->recording||safe->recorder.isActive()||safe->revision!=sourceRevision||safe->effectTarget()!=track){safe->status="Channel or effects changed. Reopen the value.";return;}auto text=dialog->getTextEditorContents("value").trim();const char* begin=text.toRawUTF8();char* end=nullptr;double value=std::strtod(begin,&end)/scale;if(text.isEmpty()||end==begin||*end!='\0'||!std::isfinite(value)){safe->status="Enter a finite numeric value.";return;}auto next=safe->effectMixer(track);auto valid=echo?setEchoValue(next.delay,i,value):setDistortionValue(next.distortion,i,value);if(valid.failed()){safe->status=valid.getErrorMessage();return;}safe->checkpoint();safe->effectMixer(track)=next;safe->changed();safe->status=echo?"Delay updated.":"Distortion updated.";}),true);
}
}
