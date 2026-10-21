// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <cmath>
#include <complex>
#include <juce_dsp/juce_dsp.h>
namespace pulse {
using namespace juce;
bool Studio::routeTrack(int source,int destination){
    if(recorder.isActive()||source<0||source>=(int)project.tracks.size())return false;
    auto candidate=project;candidate.tracks[(size_t)source].mixer.output=destination;auto result=validateRouting(candidate);
    if(result.failed()){status=result.getErrorMessage();repaint();return false;}
    checkpoint();releaseKeys();engine->stop();project.tracks[(size_t)source].mixer.output=destination;changed();status="Mixer output routed. Space resumes playback.";return true;
}
void Studio::chooseRouting(bool sidechain){
    if(editingMasterEffects()){status="Master output and compressor detector routing are fixed. Select a source or bus first.";return;}
    int source=selectedTrack;PopupMenu menu;const auto& fx=project.tracks[(size_t)source].mixer;
    menu.addItem(1,sidechain?"Internal detector":"Master",true,(sidechain?fx.sidechain:fx.output)<0);
    for(int i=0;i<(int)project.tracks.size();++i){if(i==source)continue;bool enabled=true;
        if(!sidechain){auto candidate=project;candidate.tracks[(size_t)source].mixer.output=i;enabled=validateRouting(candidate).wasOk();}
        menu.addItem(i+2,String(i+1)+" - "+project.tracks[(size_t)i].name,enabled,(sidechain?fx.sidechain:fx.output)==i);}
    auto safe=Component::SafePointer<Studio>(this);menu.showMenuAsync(menuOptions(),[safe,source,sidechain](int r){
        if(!safe||r==0||safe->recorder.isActive()||source>=(int)safe->project.tracks.size())return;
        int target=r==1?-1:r-2;if(sidechain){safe->checkpoint();safe->project.tracks[(size_t)source].mixer.sidechain=target;safe->changed();}else safe->routeTrack(source,target);
    });
}
void Studio::paintEffects(Graphics& g,Rectangle<float> r){
    if(analyzerView){paintAnalyzer(g,r);return;}
    if(masterEffectsView){paintMasterEffects(g,r);return;}
    if(selectedEffect==6){paintStereo(g,r);return;}
    const Colour text(0xffd9ddde),muted(0xffadb7bd),accent(0xffb2c98d);const auto& tr=project.tracks[(size_t)selectedTrack];const auto& fx=effectMixer(effectTarget());const bool master=editingMasterEffects();
    g.setColour(Colour(0xff3b464c));g.fillRect(r);label(g,master?String("Master - inserts"):tr.name+(tr.isBus?" - bus":" - insert"),{r.getX()+12,r.getY()+4,r.getWidth()-440,25},13,text,true);
    const char* tabs[]={"EQ","Dynamics","Reverb","Chorus","Flanger","Phaser"};const float widths[]={48,90,60,60,64,60};float tabX=r.getRight()-415;for(int i=0;i<6;++i){button(g,tabs[i],{tabX,r.getY()+5,widths[i],24},effectsTab,i,selectedEffect==i);tabX+=widths[i]+5;}
    bool enabled=selectedEffect==0?fx.eq:selectedEffect==1?fx.compressor:selectedEffect==2?fx.reverb:selectedEffect==3?fx.chorus.enabled:selectedEffect==4?fx.flanger.enabled:fx.phaser.enabled;
    button(g,enabled?"ENABLED":"BYPASSED",{r.getX()+13,r.getY()+40,106,25},effectToggle,selectedEffect,enabled);button(g,"Chain...",{r.getX()+131,r.getY()+40,84,25},effectsChainMenu);button(g,"Stereo...",{r.getX()+221,r.getY()+40,78,25},master?masterStereoOpen:stereoOpen);StringArray chain;for(int effect:fx.order)chain.add(effectName(effect,true));label(g,chain.joinIntoString(" > "),{r.getX()+310,r.getY()+40,r.getWidth()-323,25},9,muted);
    float x=r.getX()+18,y=r.getY()+90,w=(r.getWidth()-58)/2;
    auto control=[&](const String& title,int action,float value,int row,int column){dial(g,title,{x+column*(w+20),y+row*48,w,38},action,value,effectTarget());};
    if(selectedEffect==0){control("Low shelf 120 Hz  "+String(fx.low,1)+" dB",eqLow,(fx.low+18)/36,0,0);control("Mid 1 kHz  "+String(fx.mid,1)+" dB",eqMid,(fx.mid+18)/36,1,0);control("High shelf 5 kHz  "+String(fx.high,1)+" dB",eqHigh,(fx.high+18)/36,2,0);
        Rectangle<float> graph{x+w+20,y,w,152};panel(g,graph,Colour(0xff293439));g.setColour(muted.withAlpha(.3f));g.drawHorizontalLine((int)graph.getCentreY(),graph.getX(),graph.getRight());
        auto* device=deviceManager.getCurrentAudioDevice();double rate=device?device->getCurrentSampleRate():48000;using Coeff=dsp::IIR::ArrayCoefficients<double>;
        std::array<std::array<double,6>,3> coefficients{Coeff::makeLowShelf(rate,120.,.707,Decibels::decibelsToGain((double)fx.low)),Coeff::makePeakFilter(rate,jmin(1000.,rate*.4),.8,Decibels::decibelsToGain((double)fx.mid)),Coeff::makeHighShelf(rate,jmin(5000.,rate*.4),.707,Decibels::decibelsToGain((double)fx.high))};
        label(g,"EQ response",graph.reduced(8).withHeight(18),10,muted);Path response;auto plot=graph.reduced(8).withTrimmedTop(20).withTrimmedBottom(17);double highest=jmin(20000.,rate*.45);
        for(int i=0;i<=(int)plot.getWidth();++i){double hz=20*std::pow(highest/20,i/plot.getWidth());std::complex<double> z=std::polar(1.,-2*MathConstants<double>::pi*hz/rate),value=1.;
            if(fx.eq)for(const auto& c:coefficients)value*=(c[0]+c[1]*z+c[2]*z*z)/(c[3]+c[4]*z+c[5]*z*z);
            float db=(float)(20*std::log10(jmax(.00000001,std::abs(value))));float py=plot.getCentreY()-jlimit(-24.f,24.f,db)/48*plot.getHeight();if(i==0)response.startNewSubPath(plot.getX(),py);else response.lineTo(plot.getX()+i,py);}
        g.setColour(fx.eq?accent:muted);g.strokePath(response,PathStrokeType(1.7f));label(g,"20 Hz",{plot.getX(),graph.getBottom()-22,40,18},9,muted);float oneK=plot.getX()+(float)(std::log(1000./20)/std::log(highest/20))*plot.getWidth();label(g,"1 kHz",{oneK-19,graph.getBottom()-22,40,18},9,muted,false,Justification::centred);label(g,String(highest/1000,0)+" kHz",{plot.getRight()-43,graph.getBottom()-22,43,18},9,muted,false,Justification::centredRight);
    }else if(selectedEffect==1){
        auto mode=fx.dynamics.mode;bool limiter=mode==DynamicsMode::limiter,gate=mode==DynamicsMode::gate,expander=mode==DynamicsMode::expander;
        button(g,"Mode: "+dynamicsName(mode)+"...",{x,y-12,r.getWidth()-38,26},dynamicsModeMenu);y+=27;auto dynControl=[&](const String& title,int action,float value,int row,int column){dial(g,title,{x+column*(w+20),y+row*40,w,32},action,value,effectTarget());};
        dynControl((limiter?String("Ceiling  "):String("Threshold  "))+String(fx.threshold,1)+" dBFS",compThreshold,(fx.threshold+60)/60,0,0);
        if(!limiter&&!gate)dynControl("Ratio  "+String(fx.ratio,1)+":1",compRatio,(fx.ratio-1)/19,0,1);
        if(!limiter)dynControl("Attack  "+String(fx.compAttack,1)+" ms",compAttack,(fx.compAttack-.1f)/99.9f,1,0);
        dynControl("Release  "+String(fx.compRelease,0)+" ms",compRelease,(fx.compRelease-5)/995,1,1);
        dynControl((limiter?String("Input trim  "):String("Makeup  "))+String(fx.makeup,1)+" dB",compMakeup,(fx.makeup+12)/36,2,0);
        if(gate||expander)dynControl("Range  "+String(fx.dynamics.range,1)+" dB",dynamicsRange,fx.dynamics.range/96,2,1);
        if(gate){dynControl("Hysteresis  "+String(fx.dynamics.hysteresis,1)+" dB",dynamicsHysteresis,fx.dynamics.hysteresis/24,3,0);dynControl("Hold  "+String(fx.dynamics.hold,0)+" ms",dynamicsHold,fx.dynamics.hold/1000,3,1);}
        if(mode==DynamicsMode::compressor)dynControl("Knee  "+String(fx.dynamics.knee,1)+" dB",dynamicsKnee,fx.dynamics.knee/24,2,1);
        float bottom=y+(gate?164:124),reduction=master?engine->masterGainReduction.load():engine->gainReduction[(size_t)selectedTrack].load();
        label(g,"Gain reduction  "+String(reduction,1)+" dB",{x,bottom,r.getWidth()-38,22},12,accent);g.setColour(accent);g.fillRect(Rectangle<float>(x,bottom+25,(r.getWidth()-38)*jlimit(0.f,1.f,reduction/60),5));
        if(limiter)label(g,"Internal stereo peak / instant attack / zero lookahead. Downstream gain can raise the ceiling.",{x,bottom+36,r.getWidth()-38,32},10,muted);
        else if(master)label(g,"Detector: internal stereo peak",{x,bottom+36,r.getWidth()-38,28},11,muted);
        else button(g,"Detector: "+(fx.sidechain<0?String("internal"):project.tracks[(size_t)fx.sidechain].name),{x,bottom+36,r.getWidth()-38,26},sidechainRoute);
    }else if(selectedEffect==2){control("Wet mix  "+String(fx.reverbMix*100,0)+"%",reverbMix,fx.reverbMix,0,0);control("Room size  "+String(fx.room*100,0)+"%",reverbRoom,fx.room,1,0);control("Damping  "+String(fx.damping*100,0)+"%",reverbDamping,fx.damping,2,0);label(g,"Stereo algorithmic reverb",{x+w+20,y,w,30},13,text);label(g,"Choose the export tail in Export audio",{x+w+20,y+40,w,42},11,muted);}
    else if(selectedEffect==3){
        const auto& chorus=fx.chorus;
        for(int i=0;i<6;++i){auto& spec=chorusParameterSpecs()[(size_t)i];double value=chorusValue(chorus,i);String display=(i==0||i==2||i==4||i==5)?String(value*100,0)+"%":String(value,i==1?2:1)+spec.unit;control(String(spec.name)+"  "+display,chorusMix+i,(float)chorusNormal(i,value),i/2,i%2);}
        label(g,"Stereo modulation with a dry signal at zero latency. Right-click a knob to type a value.",{x,y+155,r.getWidth()-38,30},11,muted);
    }
    else{
        auto kind=selectedEffect==4?ModulationKind::flanger:ModulationKind::phaser;const auto& effect=selectedEffect==4?fx.flanger:fx.phaser;int first=selectedEffect==4?flangerMix:phaserMix;
        for(int i=0;i<6;++i){auto spec=modulationSpecs(kind)[(size_t)i];double v=modulationValue(effect,i);bool percentage=i==0||i==2||i==4||i==5;String display=percentage?String(v*100,0)+"%":String(v,i==1||selectedEffect==4?2:0)+spec.unit;control(String(spec.name)+"  "+display,first+i,(float)modulationNormal(kind,i,v),i/2,i%2);}
        label(g,selectedEffect==4?"Short stereo delay sweep / signed feedback / dry signal at zero latency":"Six all-pass stages / bounded feedback / independent stereo sweeps",{x,y+155,r.getWidth()-38,30},11,muted);
    }
    if(master){float bottom=r.getBottom()-72;button(g,"Master output / dry bypass",{x,bottom,r.getWidth()-154,27},masterEffectsOpen);button(g,"Stereo...",{r.getRight()-125,bottom,108,27},masterStereoOpen);label(g,"After channel mix, delay return and click; before Master level and soft saturation. Preview stays dry.",{x,bottom+33,r.getWidth()-36,25},10,muted);return;}
    float bottom=r.getBottom()-72;button(g,"Output: "+(fx.output<0?String("Master"):project.tracks[(size_t)fx.output].name),{x,bottom,r.getWidth()-154,27},outputRoute);button(g,"Add bus",{r.getRight()-125,bottom,108,27},addBus);
    label(g,"Routing processes sources before destinations. Feedback routes are disabled.",{x,bottom+33,r.getWidth()-36,25},10,muted);
}
void Studio::paintMasterEffects(Graphics& g,Rectangle<float> r){
    const Colour text(0xffd9ddde),muted(0xffadb7bd);g.setColour(Colour(0xff3b464c));g.fillRect(r);
    float x=r.getX()+16,y=r.getY()+9,w=r.getWidth()-32,column=(w-18)/2;
    label(g,"Master / Output processing",{x,y,w-200,25},13,text,true);
    button(g,"Channel effects",{r.getRight()-190,y,174,25},effectsOpen,selectedEffect==6?0:selectedEffect);
    button(g,project.masterSoftClip?"Soft saturation: ON":"Soft saturation: BYPASS",{x,y+39,column,28},masterSoftClipToggle,0,project.masterSoftClip);
    button(g,"Dry master",{x+column+18,y+39,column,28},masterDry);
    dial(g,"Shared delay return  "+String(project.delayReturn*100,0)+"%",{x,y+89,column,42},masterDelayReturn,project.delayReturn,-1);
    dial(g,"Delay feedback  "+String(project.delayFeedback*100,0)+"%",{x+column+18,y+89,column,42},masterDelayFeedback,project.delayFeedback/.8f,-1);
    dial(g,"Master volume  "+String(project.master*100,0)+"%",{x,y+148,column,42},masterDial,project.master,-1);
    button(g,"Analyze Master",{x+column+18,y+157,column,27},analyzerMaster);
    button(g,"Insert effects...",{x,y+201,column,27},masterInsertsOpen,selectedEffect==6?0:selectedEffect);button(g,"Stereo tools...",{x+column+18,y+201,column,27},masterStereoOpen);
    label(g,"Delay receives each channel's Delay send, including routed buses.",{x,y+241,w,23},11,muted);
    label(g,"Return 0% mutes echoes; feedback 0% still gives one echo when return is up.",{x,y+266,w,23},11,muted);
    label(g,"Dry master bypasses Master inserts, stereo and saturation, and mutes the shared delay return.",{x,y+291,w,30},10,muted);
    label(g,"New projects bypass saturation. Older projects retain their original master sound.",{x,r.getBottom()-42,w,26},10,muted);
}
}
