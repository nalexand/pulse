// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <cmath>
namespace pulse {
using namespace juce;
void Studio::restoreAnalyzerSettings(){auto file=root.getChildFile("data/analyzer-settings.json");if(!file.existsAsFile()||file.getSize()>2048)return;auto value=JSON::parse(file.loadFileAsString());if(!value.isObject()||(int)value["schema"]!=1||!value["view"].isBool()||!value["followTrack"].isBool())return;analyzerView=(bool)value["view"];analyzerFollowTrack=(bool)value["followTrack"];analyzerFrozen=false;analyzerWasActive=false;}
void Studio::saveAnalyzerSettings(){auto* value=new DynamicObject();value->setProperty("schema",1);value->setProperty("view",analyzerView);value->setProperty("followTrack",analyzerFollowTrack);TemporaryFile temp(root.getChildFile("data/analyzer-settings.json"));if(!temp.getFile().replaceWithText(JSON::toString(var(value),false))||!temp.overwriteTargetFileWithTemporary())status="Analyzer preferences could not be saved locally.";}
void Studio::updateAnalyzer(){
    bool active=analyzerView&&panes[effectsPane].visible&&!analyzerFrozen;
    if(!active){engine->analyzer.enabled.store(false,std::memory_order_relaxed);engine->analyzer.discard();analyzerWasActive=false;return;}
    int source=analyzerFollowTrack?selectedTrack:-1;
    if(!analyzerWasActive||source!=analyzerLastSource){engine->analyzer.enabled.store(false,std::memory_order_relaxed);analyzerToken=engine->analyzer.selectSource(source);engine->analyzer.discard();analyzerModel.reset();analyzerLastSource=source;analyzerDropBaseline=engine->analyzer.dropped();analyzerLastFresh=Time::getMillisecondCounterHiRes();analyzerWasActive=true;}
    engine->analyzer.enabled.store(true,std::memory_order_relaxed);analyzerModel.beginBatch();AnalyzerCapture::Frame frame;bool fresh=false;
    for(unsigned i=0;i<AnalyzerCapture::capacity&&engine->analyzer.pop(frame);++i)if(frame.token==analyzerToken){analyzerModel.feed(frame.left,frame.right,frame.rate);fresh=true;}
    analyzerModel.finishBatch();if(fresh)analyzerLastFresh=Time::getMillisecondCounterHiRes();
}
bool Studio::performAnalyzerAction(int action,int){
    if(action<analyzerOpen||action>analyzerBack)return false;
    if(action==analyzerBack){analyzerView=false;saveAnalyzerSettings();updateAnalyzer();return true;}
    if(action==analyzerOpen){analyzerView=true;showPane(effectsPane);}
    if(action==analyzerMaster||action==analyzerChannel){analyzerView=true;analyzerFollowTrack=action==analyzerChannel;analyzerFrozen=false;analyzerWasActive=false;showPane(effectsPane);}
    if(action==analyzerFreeze){analyzerView=true;analyzerFrozen=!analyzerFrozen;showPane(effectsPane);}
    if(action==analyzerReset){analyzerModel.reset(analyzerModel.sampleRate());analyzerWasActive=false;analyzerDropBaseline=engine->analyzer.dropped();}
    saveAnalyzerSettings();updateAnalyzer();return true;
}
void Studio::paintAnalyzer(Graphics& g,Rectangle<float> r){
    Colour text(0xffd9ddde),muted(0xffa5b0b2),accent(0xffb8c997),orange(0xffd3af71),red(0xffed7567);g.setColour(Colour(0xff394349));g.fillRect(r);float x=r.getX()+12,y=r.getY()+8,w=r.getWidth()-24;
    button(g,"Master",{x,y,78,25},analyzerMaster,0,!analyzerFollowTrack);button(g,"Selected channel",{x+84,y,132,25},analyzerChannel,0,analyzerFollowTrack);button(g,analyzerFrozen?"Resume":"Freeze",{x+228,y,78,25},analyzerFreeze,0,analyzerFrozen);button(g,"Reset",{x+312,y,64,25},analyzerReset);button(g,"Effects",{r.getRight()-86,y,74,25},analyzerBack);
    String source=analyzerLastSource<0?String("Master output"):analyzerLastSource<(int)project.tracks.size()?project.tracks[(size_t)analyzerLastSource].name:String("Switching channel");
    String detail=analyzerFrozen?"Frozen":Time::getMillisecondCounterHiRes()-analyzerLastFresh>700?"Waiting for audio":"Live";
    label(g,source+"  /  "+detail+"  /  "+String(analyzerModel.sampleRate()/1000,1)+" kHz",{x,y+31,w,23},12,text,true);
    auto db=[](double amplitude){double level=AnalyzerModel::decibels(amplitude);return level<=-120?String("-inf"):String(level,1);};
    float column=w/2;for(int ch=0;ch<2;++ch){float cx=x+ch*column;String channel=ch==0?"L":"R";label(g,channel+"  Peak "+db(analyzerModel.peak(ch))+"   RMS "+db(analyzerModel.rms(ch))+" dBFS",{cx,y+58,column-8,21},11,text);label(g,"Hold "+db(analyzerModel.peakHold(ch))+" dBFS  /  Clips "+String((int64)analyzerModel.clips(ch)),{cx,y+80,column-8,18},10,analyzerModel.clips(ch)>0?red:muted);Rectangle<float> bar{cx,y+101,column-10,8};g.setColour(Colour(0xff263136));g.fillRect(bar);float length=(float)jlimit(0.,1.,(AnalyzerModel::decibels(analyzerModel.peak(ch))+72)/78);g.setColour(analyzerModel.peak(ch)>=1?red:accent);g.fillRect(bar.withWidth(bar.getWidth()*length));}
    Rectangle<float> graph{x+34,y+125,w-42,r.getHeight()-250};g.setColour(Colour(0xff263136));g.fillRect(graph);double highest=jmin(20000.,analyzerModel.sampleRate()*.5);auto px=[&](double hz){return graph.getX()+(float)(std::log(jlimit(20.,highest,hz)/20)/std::log(highest/20))*graph.getWidth();};auto py=[&](double level){return graph.getY()+(float)((12-jlimit(-120.,12.,level))/132)*graph.getHeight();};
    for(int level:{12,0,-24,-48,-72,-96,-120}){float yy=py(level);g.setColour(muted.withAlpha(.18f));g.drawHorizontalLine((int)yy,graph.getX(),graph.getRight());label(g,String(level),{x,yy-7,30,15},9,muted,false,Justification::centredRight);}
    for(double hz:{20.,100.,1000.,10000.,20000.})if(hz<=highest){float xx=px(hz);g.setColour(muted.withAlpha(.15f));g.drawVerticalLine((int)xx,graph.getY(),graph.getBottom());label(g,hz>=1000?String(hz/1000,0)+"k":String(hz,0),{xx-17,graph.getBottom()+2,34,15},9,muted,false,Justification::centred);}
    if(analyzerModel.spectrumReady()){Path curve;bool started=false;for(int bin=1;bin<AnalyzerModel::bins;++bin){double frequency=analyzerModel.frequency(bin);if(frequency<20||frequency>highest)continue;float xx=px(frequency),yy=py(AnalyzerModel::decibels(analyzerModel.magnitude(bin)));if(!started){curve.startNewSubPath(xx,yy);started=true;}else curve.lineTo(xx,yy);}g.setColour(accent);g.strokePath(curve,PathStrokeType(1.4f));}else label(g,analyzerFrozen?"Frozen measurement":"Collecting 2048 samples...",graph.reduced(12),12,muted,false,Justification::centred);
    float bottom=r.getBottom()-75;label(g,"L/R correlation: "+(analyzerModel.correlationReady()?String(analyzerModel.correlation(),2):String("-- / silence or one channel")),{x,bottom,w-210,22},11,analyzerModel.correlationReady()&&analyzerModel.correlation()<0?orange:text);Rectangle<float> correlation{r.getRight()-202,bottom+6,188,8};g.setColour(Colour(0xff263136));g.fillRect(correlation);g.setColour(muted);g.drawVerticalLine((int)correlation.getCentreX(),correlation.getY()-2,correlation.getBottom()+2);if(analyzerModel.correlationReady()){float point=correlation.getX()+(float)(analyzerModel.correlation()+1)*.5f*correlation.getWidth();g.setColour(analyzerModel.correlation()<0?orange:accent);g.fillEllipse(point-3,correlation.getY()-1,6,10);}
    label(g,"Stereo spectrum / Hann 2048  /  "+String(analyzerModel.sampleRate()/AnalyzerModel::size,1)+" Hz bins  /  300 ms RMS",{x,bottom+23,w,18},9,muted);auto dropped=engine->analyzer.dropped()-analyzerDropBaseline;label(g,"Sample peaks, not true-peak or LUFS. Dropped analysis frames: "+String((int64)dropped)+"  /  Invalid samples: "+String((int64)analyzerModel.invalidSamples()),{x,bottom+43,w,18},9,muted);
}
}
