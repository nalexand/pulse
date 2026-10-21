// SPDX-License-Identifier: AGPL-3.0-only
#include "SpectralEditing.h"
static double spectralTone(const AudioBuffer<float>& a,int ch,double hz,double rate,int first,int end){double s=0,c=0;for(int i=first;i<end;++i){double phase=2*MathConstants<double>::pi*hz*i/rate;s+=a.getSample(ch,i)*std::sin(phase);c+=a.getSample(ch,i)*std::cos(phase);}return 2*std::hypot(s,c)/(end-first);}
static bool spectralExact(const AudioBuffer<float>& a,const AudioBuffer<float>& b){if(a.getNumChannels()!=b.getNumChannels()||a.getNumSamples()!=b.getNumSamples())return false;for(int ch=0;ch<a.getNumChannels();++ch)for(int i=0;i<a.getNumSamples();++i)if(a.getSample(ch,i)!=b.getSample(ch,i))return false;return true;}
static void testSpectral(const File& root){
    auto* corpus=new DynamicObject();Array<var> cases;bool passed=true;
    AudioBuffer<float> out;
    for(int rate:{8000,44100,48000,96000,192000})for(int order:{10,11,12,13}){
        int n=1<<order,count=n*8;double target=48.*rate/n,other=12.*rate/n;
        AudioBuffer<float> source(2,count);for(int i=0;i<count;++i){float v=(float)(.2*std::sin(2*MathConstants<double>::pi*target*i/rate)+.1*std::sin(2*MathConstants<double>::pi*other*i/rate));source.setSample(0,i,v);source.setSample(1,i,-v);}
        SpectralOptions options{40.*rate/n,56.*rate/n,-24,order};
        bool ok=editSpectral(source,rate,0,count,options,1,out).wasOk();
        expect(ok,"STFT spectral attenuation runs at each supported rate/resolution family");
        if(!ok){passed=false;continue;}
        double actual=spectralTone(out,0,target,rate,n,count-n),retained=spectralTone(out,0,other,rate,n,count-n),db=20*std::log10(actual/.2);
        bool measured=std::abs(db+24)<.03&&std::abs(retained-.1)<1e-5;
        expect(measured,"configured spectral tone attenuates within 0.03 dB while distant tone stays within 1e-5");passed&=measured;
        bool phase=true;for(int i=0;i<count;++i)phase&=out.getSample(0,i)==-out.getSample(1,i);
        expect(phase,"spectral processing retains antiphase stereo instead of collapsing channels");
        auto* row=new DynamicObject();row->setProperty("rate",rate);row->setProperty("fft",n);row->setProperty("targetHz",target);row->setProperty("attenuationDb",db);row->setProperty("retainedAmplitude",retained);row->setProperty("passed",measured&&phase);cases.add(var(row));
        expect(editSpectral(source,rate,n,count-n,options,1,out).wasOk(),"interior time-frequency selection processes successfully");
        bool exact=true;for(int ch=0;ch<2;++ch)for(int i=0;i<count;++i)if(i<=n||i>=count-n-1)exact&=source.getSample(ch,i)==out.getSample(ch,i);
        expect(exact,"all samples outside the time selection and both blended endpoints remain bit-exact");
        options.gainDb=0;expect(editSpectral(source,rate,0,count,options,1,out).wasOk()&&spectralExact(source,out),"zero-dB spectral mask is an exact neutral bypass");
        options.gainDb=-24;expect(editSpectral(source,rate,0,count,options,0,out).wasOk()&&spectralExact(source,out),"zero-strength spectral processing is an exact neutral bypass");
        options.lowHz=0;options.highHz=rate*.5;options.gainDb=-6;
        expect(editSpectral(source,rate,0,count,options,1,out).wasOk(),"full-spectrum attenuation reconstructs with no added frame delay");
        double error=0,gain=std::pow(10.,-.3);for(int i=0;i<count;++i)error=jmax(error,std::abs(out.getSample(0,i)-source.getSample(0,i)*gain));
        expect(error<2e-6,"normalized overlap-add retains expected whole-file gain including edges");
    }
    AudioBuffer<float> source(2,96000);for(int i=0;i<source.getNumSamples();++i){source.setSample(0,i,(float)(.2*std::sin(2*MathConstants<double>::pi*1000*i/48000)));source.setSample(1,i,0);}
    SpectralOptions options;options.lowHz=800;options.highHz=1200;
    expect(editSpectral(source,48000,0,96000,options,.5,out).wasOk()&&std::abs(spectralTone(out,0,1000,48000,12000,84000)-.2*(.5+.5*std::pow(10.,-1.2)))<1e-5,"strength interpolates dry and processed audio, rather than decibels");
    bool isolated=true;for(int i=0;i<96000;++i)isolated&=out.getSample(1,i)==0;expect(isolated,"left-channel spectral edit never introduces right-channel signal");
    SpectralImages images;expect(spectralImages(source,out,48000,0,96000,11,images).wasOk()&&images.valid(),"fixed-size original/edited spectrograms build off the audio thread");
    int y=(int)(std::log(1000./20)/std::log(48000./40)*SpectralImages::rows);auto index=(size_t)y*SpectralImages::columns+96;
    expect(std::abs(images.original[index]+17)<1&&images.edited[index]<images.original[index]-5,"spectrogram shows calibrated stereo RMS tone and actual attenuation on a shared scale");
    AudioBuffer<float> impulses(2,16384);for(int i=0;i<16384;++i){impulses.setSample(0,i,i==0||i==4096||i==16383?.5f:0);impulses.setSample(1,i,.1f+(i%2?.2f:-.2f));}
    SpectralOptions full{0,24000,-6,11};expect(editSpectral(impulses,48000,0,16384,full,1,out).wasOk(),"full-band resynthesis accepts DC, Nyquist and impulses at boundaries");double impulseError=0;for(int ch=0;ch<2;++ch)for(int i=0;i<16384;++i)impulseError=jmax(impulseError,std::abs(out.getSample(ch,i)-impulses.getSample(ch,i)*std::pow(10.,-.3)));
    expect(impulseError<2e-6,"DC/Nyquist and impulses retain timing/gain through normalized inverse FFT");
    auto oldImage=images;std::atomic<bool> cancel{true};AudioBuffer<float> sentinel(2,1);sentinel.clear();sentinel.setSample(0,0,.123f);out.makeCopyOf(sentinel);
    expect(editSpectral(source,48000,0,96000,options,1,out,&cancel).failed()&&spectralExact(out,sentinel),"cancelled spectral work never replaces caller output");
    expect(spectralImages(source,source,48000,0,96000,11,images,&cancel).failed()&&images.original==oldImage.original&&images.edited==oldImage.edited,"cancelled spectrogram generation preserves previous images");
    for(int test=0;test<8;++test){auto bad=options;if(test==0)bad.lowHz=-1;if(test==1)bad.highHz=bad.lowHz;if(test==2)bad.highHz=96001;if(test==3)bad.gainDb=1;if(test==4)bad.gainDb=-97;if(test==5)bad.fftOrder=9;if(test==6)bad.fftOrder=14;if(test==7)bad.lowHz=std::numeric_limits<double>::quiet_NaN();expect(editSpectral(source,48000,0,96000,bad,1,out).failed()&&spectralExact(out,sentinel),"invalid spectral masks reject atomically");}
    auto bad=source;bad.setSample(0,20,std::numeric_limits<float>::infinity());expect(editSpectral(bad,48000,0,96000,options,1,out).failed(),"non-finite source cannot reach spectral output");
    expect(editSpectral(source,48000,-1,96000,options,1,out).failed()&&editSpectral(source,48000,0,2047,options,1,out).failed(),"invalid/short spectral time selections reject");
    auto nyquist=options;nyquist.highHz=25000;expect(editSpectral(source,48000,0,96000,nyquist,1,out).failed(),"band above actual Nyquist rejects instead of silently clamping");
    AudioBuffer<float> longSelection(2,48000*31);longSelection.clear();expect(editSpectral(longSelection,48000,0,longSelection.getNumSamples(),options,1,out).failed(),"spectral edit selection has an explicit 30-second work/memory bound");
    source.clear();expect(editSpectral(source,48000,0,96000,options,1,out).wasOk()&&spectralExact(source,out),"spectral attenuation of silence stays an exact no-op");
    RestorationOptions prefs;prefs.kind=RestorationKind::spectral;prefs.spectral=options;auto file=root.getChildFile("spectral-settings.json");RestorationOptions read;
    expect(saveRestorationOptions(file,prefs).wasOk()&&loadRestorationOptions(file,read).wasOk()&&read==prefs,"spectral settings round-trip through atomic native preferences");
    auto json=restorationToJson(prefs);json.getDynamicObject()->setProperty("fftOrder",1e100);expect(restorationFromJson(json,read).failed()&&read==prefs,"oversized FFT preference rejects without integer overflow or output mutation");
    json=restorationToJson(RestorationOptions{});json.getDynamicObject()->setProperty("schema",1);for(const char* key:{"lowHz","highHz","gainDb","fftOrder"})json.getDynamicObject()->removeProperty(key);
    expect(restorationFromJson(json,read).wasOk()&&read==RestorationOptions{},"schema-1 restoration preferences retain historical defaults");
    corpus->setProperty("version","0.73.0");corpus->setProperty("passed",passed);corpus->setProperty("cases",cases);root.getChildFile("spectral-evaluation.json").replaceWithText(JSON::toString(var(corpus),false));
}
