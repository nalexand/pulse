// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <cmath>
#include <cstdlib>
namespace pulse {
using namespace juce;
namespace {
class RestorationForm final:public Component {
public:
    explicit RestorationForm(const RestorationOptions& o){
        setComponentID("restoration-form");
        mode.addItemList({"Remove DC offset","Reduce 50/60 Hz hum","Repair isolated clicks","Spectral band edit"},1);
        mode.setSelectedItemIndex((int)o.kind);hz.addItemList({"50 Hz","60 Hz"},1);hz.setSelectedItemIndex(o.humHz==60?1:0);
        fft.addItemList({"1024 samples","2048 samples","4096 samples","8192 samples"},1);fft.setSelectedItemIndex(o.spectral.fftOrder-10);
        add("Mode",mode,"restoration-mode");add("Strength (%)",strength,"restoration-strength");
        add("Mains frequency",hz,"restoration-hz");add("Harmonics (1-6)",harmonics,"restoration-harmonics");
        add("Notch Q (8-40)",q,"restoration-q");add("Click threshold (3-20)",threshold,"restoration-threshold");add("Max click (1-8 samples)",width,"restoration-width");
        add("Low frequency (Hz)",low,"spectral-low");add("High frequency (Hz)",high,"spectral-high");
        add("Attenuation (-96 to 0 dB)",gain,"spectral-gain");add("FFT resolution",fft,"spectral-fft");
        strength.setText(String(o.strength*100,2));harmonics.setText(String(o.harmonics));q.setText(String(o.q,2));threshold.setText(String(o.clickThreshold,2));width.setText(String(o.clickWidth));
        low.setText(String(o.spectral.lowHz,2));high.setText(String(o.spectral.highHz,2));gain.setText(String(o.spectral.gainDb,2));
        mode.onChange=[this]{resized();};setSize(660,224);
    }
    Result read(RestorationOptions& o)const{
        auto number=[](const TextEditor& field,double& out){auto value=field.getText().trim();if(value.isEmpty()||!value.containsOnly("0123456789.+-eE"))return false;auto* p=value.toRawUTF8();char* end=nullptr;out=std::strtod(p,&end);return end!=p&&*end==0&&std::isfinite(out);};
        double amount=0,h=0,qual=0,t=0,w=0,lo=0,hi=0,db=0;
        if(!number(strength,amount)||!number(harmonics,h)||!number(q,qual)||!number(threshold,t)||!number(width,w)
           ||!number(low,lo)||!number(high,hi)||!number(gain,db)||h!=std::floor(h)||w!=std::floor(w)||h<1||h>6||w<1||w>8)
            return Result::fail("Enter finite values and whole harmonic/click counts.");
        RestorationOptions next;next.kind=(RestorationKind)mode.getSelectedItemIndex();next.strength=amount/100;
        next.humHz=hz.getSelectedItemIndex()==1?60:50;next.harmonics=(int)h;next.q=qual;next.clickThreshold=t;next.clickWidth=(int)w;
        next.spectral={lo,hi,db,fft.getSelectedItemIndex()+10};auto valid=validateRestoration(next);if(valid.wasOk())o=next;return valid;
    }
    void resized()override{
        int gap=10,col=(getWidth()-gap*2)/3;bool spectral=mode.getSelectedItemIndex()==3;
        for(int i=2;i<11;++i){bool visible=(i>=7)==spectral;labels[(size_t)i].setVisible(visible);fields[(size_t)i]->setVisible(visible);}
        auto place=[&](int index,int x,int y,int w){labels[(size_t)index].setBounds(x,y,w,22);fields[(size_t)index]->setBounds(x,y+25,w,28);};
        place(0,0,0,col*2+gap);place(1,(col+gap)*2,0,col);
        if(spectral){place(7,0,73,col);place(8,col+gap,73,col);place(9,(col+gap)*2,73,col);place(10,0,146,col*2+gap);}
        else{place(2,0,73,col);place(3,col+gap,73,col);place(4,(col+gap)*2,73,col);place(5,0,146,col*2+gap);place(6,(col+gap)*2,146,col);}
        bool hum=mode.getSelectedItemIndex()==1,click=mode.getSelectedItemIndex()==2;
        hz.setEnabled(hum);harmonics.setEnabled(hum);q.setEnabled(hum);threshold.setEnabled(click);width.setEnabled(click);
    }
private:
    void add(const char* title,Component& field,const char* id){int i=(int)fields.size();labels[(size_t)i].setText(title,dontSendNotification);labels[(size_t)i].setFont(Font(FontOptions(12)));addAndMakeVisible(labels[(size_t)i]);field.setComponentID(id);addAndMakeVisible(field);fields.push_back(&field);}
    ComboBox mode,hz,fft;TextEditor strength,harmonics,q,threshold,width,low,high,gain;std::array<Label,11> labels;std::vector<Component*> fields;
};
class SpectralComparison final:public Component {
public:
    SpectralComparison(const SpectralImages& data,const SpectralOptions& settings):options(settings),rate(data.sampleRate),first(data.firstSeconds),end(data.endSeconds){
        setComponentID("spectral-spectrogram");
        for(int version=0;version<2;++version){auto& image=images[(size_t)version];image=Image(Image::RGB,SpectralImages::columns,SpectralImages::rows,true);const auto& values=version?data.edited:data.original;
            for(int y=0;y<SpectralImages::rows;++y)for(int x=0;x<SpectralImages::columns;++x){float v=jlimit(0.f,1.f,(values[(size_t)y*SpectralImages::columns+x]+100)/112);image.setPixelAt(x,SpectralImages::rows-1-y,Colour::fromHSV(.7f*(1-v),.82f,.08f+.92f*v,1.f));}}
    }
    void paint(Graphics& g)override{
        g.fillAll(Colour(0xff242e32));g.setFont(Font(FontOptions(11)));
        float half=getWidth()*.5f;
        for(int version=0;version<2;++version){float x=version*half;Rectangle<float> plot{x+38,23,half-47,(float)getHeight()-66};
            g.setColour(Colour(0xffd9ddde));g.drawText(version?"Edited":"Original",(int)x+38,1,(int)half-47,20,Justification::left);
            g.drawImage(images[(size_t)version],plot);auto yAt=[&](double hz){return plot.getBottom()-(float)(std::log(jlimit(20.,rate*.5,hz)/20)/std::log(rate/40))*plot.getHeight();};
            g.setColour(Colour(0xffb8c997));g.drawRect(Rectangle<float>(plot.getX(),yAt(options.highHz),plot.getWidth(),jmax(1.f,yAt(options.lowHz)-yAt(options.highHz))),1.5f);
            for(double hz:{20.,100.,1000.,rate*.5}){float y=yAt(hz);g.setColour(Colour(0xffb6bfc0));g.drawText(hz>=1000?String(hz/1000,1)+"k":String(hz,0),(int)x+1,(int)y-7,34,14,Justification::right);}
            g.drawText(String(first,3)+" s",(int)plot.getX(),(int)plot.getBottom()+3,90,16,Justification::left);g.drawText(String(end,3)+" s",(int)plot.getRight()-90,(int)plot.getBottom()+3,90,16,Justification::right);
        }
        g.setColour(Colour(0xffb6bfc0));g.drawText("Shared scale: -100 to +12 dBFS | Log Hz | Outlined band | Sampled windows",0,getHeight()-20,getWidth(),18,Justification::centred);
    }
private:std::array<Image,2> images;SpectralOptions options;double rate,first,end;
};
class RestorationSettingsDialog final:public AlertWindow {
public:
    RestorationSettingsDialog(const RestorationOptions& o,Component* parent,bool hasPreview):AlertWindow("Audio restoration","Process the waveform selection on a separate copy. Preview creates a local audition file; review Original/Restored before Apply. Spectral mode uses FFT attenuation, not healing. Choose a band below Nyquist and a selection from one FFT window to 30 seconds. Hum filters can affect tones; click repair can affect attacks.",MessageBoxIconType::NoIcon,parent),form(o){form.setSize(jmax(230,jmin(660,(int)(parent->getParentMonitorArea().getWidth()*.54))),224);addCustomComponent(&form);addButton("Process preview",1,KeyPress(KeyPress::returnKey));if(hasPreview)addButton("Back to preview",2);addButton("Cancel",0,KeyPress(KeyPress::escapeKey));}
    RestorationForm form;
};
class RestorationReviewDialog final:public AlertWindow {
public:
    RestorationReviewDialog(const String& summary,Component* parent,const SpectralImages& images,const SpectralOptions& options)
      :AlertWindow(images.valid()?"Spectral edit preview":"Restoration preview","Original / Edited auditions the selection and stops Song playback. Apply changes only the waveform editor; assign it to a channel or Playlist explicitly afterward.",MessageBoxIconType::NoIcon,parent){
        auto area=parent->getParentMonitorArea();int w=jmax(230,jmin(660,(int)(area.getWidth()*.54)));
        text.setComponentID("restoration-report");text.setMultiLine(true,true);text.setReadOnly(true);text.setScrollbarsShown(true);text.setText(summary,false);
        if(images.valid()){spectrogram=std::make_unique<SpectralComparison>(images,options);spectrogram->setSize(w,jmax(125,jmin(215,area.getHeight()/4)));addCustomComponent(spectrogram.get());}
        text.setSize(w,jmax(90,jmin(images.valid()?145:320,area.getHeight()-(images.valid()?530:300))));addCustomComponent(&text);
        addButton("Original",2);addButton(images.valid()?"Edited":"Restored",3);addButton("Stop",6);addButton("Apply",4);addButton("Settings",5);addButton("Discard",0,KeyPress(KeyPress::escapeKey));
    }
private:TextEditor text;std::unique_ptr<SpectralComparison> spectrogram;
};

}
void Studio::restoreRestorationOptions(){auto valid=loadRestorationOptions(root.getChildFile("data/restoration-settings.json"),restorationOptions);if(valid.failed())status=valid.getErrorMessage();}
bool Studio::restorationCurrent()const{return restorationPreview&&!restorationTask&&restorationRevision==waveRevision&&waveFile==restorationSource;}
void Studio::discardRestoration(bool keep){
    restorationReveal=false;if(restorationTask){restorationTask->cancel.store(true);status="Cancelling audio restoration...";return;}engine->cancelPreview();
    if(!keep&&restorationFile!=File()&&restorationFile.isAChildOf(root.getChildFile("data/Restoration"))&&restorationFile!=waveFile){bool used=false;for(const auto& asset:projectAssets(project,currentFile==File()?root:currentFile.getParentDirectory()))used|=asset.file==restorationFile;for(const auto& h:waveHistory)used|=h.file==restorationFile;for(const auto& h:waveRedoHistory)used|=h.file==restorationFile;if(!used)restorationFile.deleteFile();}
    restorationPreview=nullptr;restorationImages={};restorationFile={};restorationSource={};restorationSourceStamp.clear();restorationOutputStamp.clear();
}
void Studio::showRestorationSettings(){
    closeCommandPalette();if(restorationTask){status="Restoration is running. Use Edit / Ctrl+P > Cancel audio restoration.";return;}if(!waveSample||waveEditBusy||recording||recorder.isActive()||exporting.load()){status="Open waveform audio and finish the take/conversion/export first.";return;}
    auto* d=new RestorationSettingsDialog(restorationOptions,this,restorationCurrent());auto safe=Component::SafePointer<Studio>(this);auto expected=waveRevision;d->enterModalState(true,ModalCallbackFunction::create([safe,d,expected](int answer){if(!safe)return;if(answer==2){safe->showRestorationReview();return;}if(answer!=1)return;RestorationOptions options;auto valid=d->form.read(options);if(valid.failed()){safe->status=valid.getErrorMessage();safe->showRestorationSettings();return;}safe->beginRestoration(options,expected,true);}),true);
}
bool Studio::beginRestoration(const RestorationOptions& options,uint64_t expected,bool reveal){
    if(expected!=waveRevision||restorationTask||!waveSample||waveEditBusy||recording||recorder.isActive()||exporting.load()){status="Audio changed or the app is busy. Reopen restoration after the take/conversion/export.";return false;}auto valid=validateRestoration(options);if(valid.failed()){status=valid.getErrorMessage();return false;}
    int first=jlimit(0,waveSample->audio.getNumSamples()-1,(int)std::llround(waveStart*waveSample->sampleRate)),end=jlimit(first+1,waveSample->audio.getNumSamples(),(int)std::llround(waveEnd*waveSample->sampleRate));if(options.kind==RestorationKind::hum&&end-first<waveSample->sampleRate*.25){status="Select at least 0.25 seconds for hum reduction.";return false;}
    if(options.kind==RestorationKind::spectral&&(end-first<(1<<options.spectral.fftOrder)||end-first>waveSample->sampleRate*30||options.spectral.highHz>waveSample->sampleRate*.5)){status="Select one FFT window to 30 seconds and a spectral band below Nyquist.";return false;}
    discardRestoration();restorationOptions=options;restorationRevision=waveRevision;restorationFirst=first;restorationEnd=end;restorationSource=waveFile;restorationReveal=reveal;
    try{AudioBuffer<float> source;source.makeCopyOf(waveSample->audio);restorationFile=root.getChildFile("data/Restoration").getChildFile(Uuid().toString()+"_restored.wav");restorationTask=std::make_unique<RestorationTask>(std::move(source),waveSample->sampleRate,first,end,options,waveFile,restorationFile);if(!restorationTask->startThread(Thread::Priority::low)){restorationTask.reset();status="Could not start restoration worker.";return false;}}catch(const std::exception&){restorationTask.reset();status="Not enough resources to preview restoration.";return false;}
    restorationMessage="Processing restoration preview in the background. Edit / Ctrl+P can cancel.";status=restorationMessage;return true;
}
void Studio::finishRestoration(){
    if(!restorationTask){if(restorationPreview&&!restorationCurrent())discardRestoration();return;}if(!restorationTask->done.load(std::memory_order_acquire))return;auto result=restorationTask->result;bool cancelled=restorationTask->cancel.load(),show=restorationReveal;restorationReveal=false;restorationStats=restorationTask->stats;restorationSourceStamp=restorationTask->sourceStamp;restorationOutputStamp=restorationTask->outputStamp;restorationImages=std::move(restorationTask->images);restorationTask.reset();
    if(cancelled||result.failed()||restorationRevision!=waveRevision||waveFile!=restorationSource||recording||recorder.isActive()||waveEditBusy||exporting.load()){restorationMessage=cancelled?"Restoration cancelled. Source and history retained.":result.failed()?result.getErrorMessage():"Audio changed or app became busy. Restoration discarded.";discardRestoration();status=restorationMessage;return;}
    if(restorationStats.changed[0]+restorationStats.changed[1]==0){discardRestoration();status="Restoration found no sample changes. No file or history entry created.";return;}
    String error;restorationPreview=assets.load(restorationFile,error);if(!restorationPreview){discardRestoration();status=error;return;}auto saved=saveRestorationOptions(root.getChildFile("data/restoration-settings.json"),restorationOptions);restorationMessage="Restoration preview ready. Compare Original / Restored, then Apply or Discard.";if(saved.failed())restorationMessage+=" "+saved.getErrorMessage();status=restorationMessage;if(show&&!Component::getCurrentlyModalComponent())showRestorationReview();
}
void Studio::showRestorationReview(){
    if(!restorationCurrent()){status="Restoration preview is stale. Process the current selection again.";return;}auto* d=new RestorationReviewDialog(restorationSummary(restorationStats,restorationOptions,waveSample->sampleRate,restorationFirst,restorationEnd),this,restorationImages,restorationOptions.spectral);auto safe=Component::SafePointer<Studio>(this);d->enterModalState(true,ModalCallbackFunction::create([safe](int answer){if(!safe)return;if(answer==2||answer==3){safe->auditionRestoration(answer==3);safe->showRestorationReview();}else if(answer==6){safe->engine->cancelPreview();safe->showRestorationReview();}else if(answer==4)safe->applyRestoration();else if(answer==5){safe->engine->cancelPreview();safe->showRestorationSettings();}else safe->discardRestoration();}),true);
}
bool Studio::auditionRestoration(bool processed){
    if(!restorationCurrent()||recording||recorder.isActive()||exporting.load()||inputMonitor.load()||restorationFileStamp(restorationSource)!=restorationSourceStamp||restorationFileStamp(restorationFile)!=restorationOutputStamp){status="Preview changed or recording/export is active. Audition cancelled.";return false;}if(!audioReady())return false;double peak=0;for(int c=0;c<2;++c)peak=jmax(peak,restorationStats.peakBefore[c],restorationStats.peakAfter[c]);float gain=(float)jmin(.7,peak>0?.98/peak:.7);perform(stop);return engine->preview(processed?restorationPreview:waveSample,restorationFirst/waveSample->sampleRate,restorationEnd/waveSample->sampleRate,gain);
}
bool Studio::applyRestoration(){
    if(!restorationCurrent()||recording||recorder.isActive()||waveEditBusy||exporting.load()||restorationFileStamp(restorationSource)!=restorationSourceStamp||restorationFileStamp(restorationFile)!=restorationOutputStamp){status="Audio changed or app is busy. Process a fresh restoration preview.";return false;}
    engine->cancelPreview();waveHistory.push_back({waveFile,waveStart,waveEnd,waveViewStart,waveSpan});if(waveHistory.size()>20)waveHistory.erase(waveHistory.begin());waveRedoHistory.clear();waveMarkers.clear();waveFile=restorationFile;waveSample=restorationPreview;waveStart=restorationFirst/waveSample->sampleRate;waveEnd=restorationEnd/waveSample->sampleRate;++waveRevision;discardRestoration(true);status="Restored copy opened. One waveform Undo restores the source; To sampler / To Playlist assigns the new audio.";repaint();return true;
}
}
