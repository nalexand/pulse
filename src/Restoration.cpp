// SPDX-License-Identifier: AGPL-3.0-only
#include "Restoration.h"
#include <cmath>
namespace pulse {
using namespace juce;
namespace {
bool stopped(const std::atomic<bool>* cancel){return cancel&&cancel->load();}
double wetAt(int i,int first,int end,int frames,double rate,double strength){double wet=strength;int fade=jmax(1,(int)std::llround(rate*.005));if(first>0)wet*=jlimit(0.,1.,(i-first)/(double)fade);if(end<frames)wet*=jlimit(0.,1.,(end-1-i)/(double)fade);return wet;}
double median(std::array<double,32> values){std::nth_element(values.begin(),values.begin()+16,values.end());double upper=values[16],lower=*std::max_element(values.begin(),values.begin()+16);return (lower+upper)*.5;}
void notch(std::vector<double>& audio,double rate,double hz,double q,bool reverse,const std::atomic<bool>* cancel){
    auto c=dsp::IIR::ArrayCoefficients<double>::makeNotch(rate,hz,q);double a0=c[3],b0=c[0]/a0,b1=c[1]/a0,b2=c[2]/a0,a1=c[4]/a0,a2=c[5]/a0;
    double initial=reverse?audio.back():audio.front(),z1=initial*(1-b0),z2=initial*(b2-a2);
    for(size_t j=0;j<audio.size();++j){if((j&1023)==0&&stopped(cancel))return;size_t i=reverse?audio.size()-1-j:j;double x=audio[i],y=b0*x+z1;z1=b1*x-a1*y+z2;z2=b2*x-a2*y;audio[i]=y;}
}
Result matchesDecodedSource(const File& file,const AudioBuffer<float>& input,double rate,const std::atomic<bool>* cancel){
    AudioFormatManager fm;fm.registerBasicFormats();std::unique_ptr<AudioFormatReader> reader(fm.createReaderFor(file));if(!reader||reader->sampleRate!=rate||reader->lengthInSamples!=input.getNumSamples())return Result::fail("Source audio changed. Reopen it in the waveform editor.");AudioBuffer<float> block(2,4096);
    for(int pos=0;pos<input.getNumSamples();pos+=4096){if(stopped(cancel))return Result::fail("Restoration cancelled.");int n=jmin(4096,input.getNumSamples()-pos);if(!reader->read(&block,0,n,pos,true,true))return Result::fail("Source audio could not be verified.");for(int c=0;c<2;++c)for(int i=0;i<n;++i)if(block.getSample(reader->numChannels==1?0:c,i)!=input.getSample(c,pos+i))return Result::fail("Source audio changed since it was opened. Reopen it before restoration.");}return Result::ok();
}
}
Result validateRestoration(const RestorationOptions& o){
    if((int)o.kind<0||(int)o.kind>3||!std::isfinite(o.strength)||o.strength<0||o.strength>1||(o.humHz!=50&&o.humHz!=60)||!std::isfinite(o.q)||o.q<8||o.q>40||o.harmonics<1||o.harmonics>6||!std::isfinite(o.clickThreshold)||o.clickThreshold<3||o.clickThreshold>20||o.clickWidth<1||o.clickWidth>8)return Result::fail("Use strength 0-100%, hum 50/60 Hz, 1-6 harmonics, Q 8-40, click threshold 3-20 and click width 1-8 samples.");return validateSpectral(o.spectral);
}
var restorationToJson(const RestorationOptions& o){
    auto* v=new DynamicObject();v->setProperty("schema",2);v->setProperty("kind",(int)o.kind);
    v->setProperty("strength",o.strength);v->setProperty("humHz",o.humHz);v->setProperty("q",o.q);
    v->setProperty("harmonics",o.harmonics);v->setProperty("clickThreshold",o.clickThreshold);v->setProperty("clickWidth",o.clickWidth);
    v->setProperty("lowHz",o.spectral.lowHz);v->setProperty("highHz",o.spectral.highHz);
    v->setProperty("gainDb",o.spectral.gainDb);v->setProperty("fftOrder",o.spectral.fftOrder);return var(v);
}
Result restorationFromJson(const var& v,RestorationOptions& out){
    if(!v.isObject()||!v["schema"].isInt()||((int)v["schema"]!=1&&(int)v["schema"]!=2))return Result::fail("Unsupported restoration settings.");
    auto number=[&](const char* key){return (v[key].isInt()||v[key].isInt64()||v[key].isDouble())&&std::isfinite((double)v[key]);};
    for(const char* key:{"kind","strength","humHz","q","harmonics","clickThreshold","clickWidth"})if(!number(key))return Result::fail("Missing or invalid restoration settings.");
    for(const char* key:{"kind","harmonics","clickWidth"})if((double)v[key]!=std::floor((double)v[key]))return Result::fail("Restoration mode/counts must be whole numbers.");
    int maxKind=(int)v["schema"]==1?2:3;
    if((double)v["kind"]<0||(double)v["kind"]>maxKind||(double)v["harmonics"]<1||(double)v["harmonics"]>6||(double)v["clickWidth"]<1||(double)v["clickWidth"]>8)return Result::fail("Invalid restoration mode/counts.");
    RestorationOptions next;next.kind=(RestorationKind)(int)v["kind"];next.strength=(double)v["strength"];next.humHz=(double)v["humHz"];next.q=(double)v["q"];next.harmonics=(int)v["harmonics"];next.clickThreshold=(double)v["clickThreshold"];next.clickWidth=(int)v["clickWidth"];
    if((int)v["schema"]==2){
        for(const char* key:{"lowHz","highHz","gainDb","fftOrder"})if(!number(key))return Result::fail("Missing or invalid spectral settings.");
        double order=(double)v["fftOrder"];if(order!=std::floor(order)||order<10||order>13)return Result::fail("Invalid spectral FFT size.");
        next.spectral={(double)v["lowHz"],(double)v["highHz"],(double)v["gainDb"],(int)order};
    }
    auto valid=validateRestoration(next);if(valid.wasOk())out=next;return valid;
}
Result saveRestorationOptions(const File& file,const RestorationOptions& o){auto valid=validateRestoration(o);if(valid.failed())return valid;valid=file.getParentDirectory().createDirectory();if(valid.failed())return valid;auto text=JSON::toString(restorationToJson(o),false,22);TemporaryFile temp(file);if(!temp.getFile().replaceWithText(text)||temp.getFile().loadFileAsString()!=text||!temp.overwriteTargetFileWithTemporary())return Result::fail("Could not save restoration preferences. Previous settings retained.");return Result::ok();}
Result loadRestorationOptions(const File& file,RestorationOptions& o){if(!file.exists())return Result::ok();if(!file.existsAsFile()||file.getSize()>65536)return Result::fail("Restoration settings exceed 64 KiB or are unavailable.");return restorationFromJson(JSON::parse(file),o);}
String restorationName(RestorationKind kind){return kind==RestorationKind::spectral?"Spectral band edit":kind==RestorationKind::dc?"Remove DC offset":kind==RestorationKind::hum?"Reduce mains hum":"Repair isolated clicks";}
String restorationSummary(const RestorationStats& s,const RestorationOptions& o,double rate,int first,int end){
    String text=restorationName(o.kind)+" / "+String(o.strength*100,1)+"%\nSelection "+String(first/rate,4)+" - "+String(end/rate,4)+" s / "+String(end-first)+" frames at "+String(rate,0)+" Hz\n\nChanged samples L / R: "+String((int64)s.changed[0])+" / "+String((int64)s.changed[1])+"\nDC before L / R (linear): "+String(s.dcBefore[0],7)+" / "+String(s.dcBefore[1],7)+"\nDC after L / R (linear): "+String(s.dcAfter[0],7)+" / "+String(s.dcAfter[1],7)+"\nSample peak before L / R (linear): "+String(s.peakBefore[0],5)+" / "+String(s.peakBefore[1],5)+"\nSample peak after L / R (linear): "+String(s.peakAfter[0],5)+" / "+String(s.peakAfter[1],5)+"\n";
    if(o.kind==RestorationKind::hum)text+="\n"+String(o.humHz,0)+" Hz fundamental / "+String(o.harmonics)+" harmonics / Q "+String(o.q,1)+". Two-pass notches with reflected edges. Tones near these frequencies can also be reduced.";
    if(o.kind==RestorationKind::clicks)text+="\nRepaired click runs L / R: "+String((int64)s.clicks[0])+" / "+String((int64)s.clicks[1])+". Only detected runs of at most "+String(o.clickWidth)+" samples are interpolated. Attacks may be affected; compare the original.";
    if(o.kind==RestorationKind::spectral)text+="\nBand "+String(o.spectral.lowHz,1)+" - "+String(o.spectral.highHz,1)+" Hz / "+String(o.spectral.gainDb,1)+" dB at 100% strength. FFT "+String(1<<o.spectral.fftOrder)+" / "+String(rate/(1<<o.spectral.fftOrder),2)+" Hz bins / "+String((1<<o.spectral.fftOrder)/rate*1000,2)+" ms windows. Hann / 75% overlap, phase retained. Two-bin cosine frequency edges and 5 ms interior time edges. Close tones and attacks can smear; this is attenuation, not source separation or healing. Spectrogram: stereo RMS amplitude, shared -100 to +12 dBFS scale, log 20 Hz to Nyquist; columns sample windows and may miss short transients.";
    text+="\n\nAudition plays only this selection and stops Song playback. Both versions share the same preview attenuation to avoid clipping; WAV gain is unchanged. Apply opens the edited copy with one waveform Undo. To sampler / To Playlist explicitly assigns it to the song. Source file, frame count, sample rate and audio outside the selection are retained. DC/hum interior selection edges blend over 5 ms. No normalization, clipping, denoising model or automatic song replacement.";return text;
}
Result restoreWave(const AudioBuffer<float>& input,double rate,int first,int end,const RestorationOptions& o,AudioBuffer<float>& output,RestorationStats& stats,std::atomic<bool>* cancel,std::atomic<float>* progress){
    if(progress)progress->store(0);if(stopped(cancel))return Result::fail("Restoration cancelled.");auto valid=validateWaveAudio(input,rate);if(valid.failed())return valid;valid=validateRestoration(o);if(valid.failed())return valid;if(first<0||end>input.getNumSamples()||end<=first)return Result::fail("Select valid audio before restoration.");if(o.kind==RestorationKind::hum&&end-first<rate*.25)return Result::fail("Select at least 0.25 seconds for hum reduction.");
    AudioBuffer<float> next;
    if(o.kind==RestorationKind::spectral){valid=editSpectral(input,rate,first,end,o.spectral,o.strength,next,cancel,progress);if(valid.failed())return valid;}
    else next.makeCopyOf(input);RestorationStats report;int length=end-first;
    for(int ch=0;ch<2;++ch){
        double mean=0;for(int i=first;i<end;++i){if((i&4095)==0&&stopped(cancel))return Result::fail("Restoration cancelled.");mean+=(input.getSample(ch,i)-mean)/(i-first+1);report.peakBefore[ch]=jmax(report.peakBefore[ch],std::abs((double)input.getSample(ch,i)));}report.dcBefore[ch]=mean;
        if(o.strength>0&&o.kind==RestorationKind::dc){for(int i=first;i<end;++i){if((i&4095)==0&&stopped(cancel))return Result::fail("Restoration cancelled.");next.setSample(ch,i,(float)(input.getSample(ch,i)-mean*wetAt(i,first,end,input.getNumSamples(),rate,o.strength)));}}
        if(o.strength>0&&o.kind==RestorationKind::hum){
            int pad=jmin(length-1,(int)std::llround(rate*.5));std::vector<double> temp((size_t)length+2*pad);for(int i=-pad;i<length+pad;++i){int pos=i<0?-i:i>=length?2*length-2-i:i;temp[(size_t)(i+pad)]=input.getSample(ch,first+pos);}
            for(int harmonic=1;harmonic<=o.harmonics;++harmonic){notch(temp,rate,o.humHz*harmonic,o.q,false,cancel);notch(temp,rate,o.humHz*harmonic,o.q,true,cancel);if(stopped(cancel))return Result::fail("Restoration cancelled.");if(progress)progress->store((ch+(harmonic/(float)o.harmonics))*.5f);}
            for(int i=first;i<end;++i){double original=input.getSample(ch,i),wet=wetAt(i,first,end,input.getNumSamples(),rate,o.strength);next.setSample(ch,i,(float)(original+(temp[(size_t)(i-first+pad)]-original)*wet));}
        }
        if(o.strength>0&&o.kind==RestorationKind::clicks&&length>33){
            std::vector<uint8_t> mask((size_t)length,0);for(int i=first+16;i<end-16;++i){if((i&255)==0&&stopped(cancel))return Result::fail("Restoration cancelled.");std::array<double,32> neighbors{};int j=0;for(int k=-16;k<=16;++k)if(k)neighbors[(size_t)j++]=input.getSample(ch,i+k);double centre=median(neighbors);for(auto& value:neighbors)value=std::abs(value-centre);double deviation=1.4826*median(neighbors);if(std::abs(input.getSample(ch,i)-centre)>jmax(.02,o.clickThreshold*deviation))mask[(size_t)(i-first)]=1;if(progress&&(i&4095)==0)progress->store((ch+(i-first)/(float)length)*.5f);}
            for(int i=16;i<length-16;){if(!mask[(size_t)i]){++i;continue;}int a=i;while(i<length-16&&mask[(size_t)i])++i;int b=i;if(b-a>o.clickWidth)continue;double left=input.getSample(ch,first+a-1),right=input.getSample(ch,first+b);for(int k=a;k<b;++k){double original=input.getSample(ch,first+k),replacement=left+(right-left)*(k-a+1)/(b-a+1);next.setSample(ch,first+k,(float)(original+(replacement-original)*o.strength));}++report.clicks[ch];}
        }
        mean=0;for(int i=first;i<end;++i){if((i&4095)==0&&stopped(cancel))return Result::fail("Restoration cancelled.");double value=next.getSample(ch,i);if(!std::isfinite(value))return Result::fail("Restoration exceeds finite sample range. Source retained.");mean+=(value-mean)/(i-first+1);report.peakAfter[ch]=jmax(report.peakAfter[ch],std::abs(value));report.changed[ch]+=value!=input.getSample(ch,i);}report.dcAfter[ch]=mean;if(progress)progress->store((ch+1)*.5f);
    }
    if(stopped(cancel))return Result::fail("Restoration cancelled.");output=std::move(next);stats=report;if(progress)progress->store(1);return Result::ok();
}
String restorationFileStamp(const File& file){return file.existsAsFile()?file.getFullPathName()+"|"+String(file.getSize())+"|"+String(file.getLastModificationTime().toMilliseconds()):String("missing");}
void RestorationTask::run(){
    try{result=[&]()->Result{
        AssetFingerprint before,after;auto valid=fingerprintAssetFile(sourceFile,before,&cancel);if(valid.failed())return valid;valid=matchesDecodedSource(sourceFile,input,rate,&cancel);if(valid.failed())return valid;
        AudioBuffer<float> restored;valid=restoreWave(input,rate,first,end,options,restored,stats,&cancel,&progress);if(valid.failed())return valid;if(stats.changed[0]+stats.changed[1]==0)return Result::ok();if(options.kind==RestorationKind::spectral){valid=spectralImages(input,restored,rate,first,end,options.spectral.fftOrder,images,&cancel);if(valid.failed())return valid;}if(stopped(&cancel))return Result::fail("Restoration cancelled.");
        valid=fingerprintAssetFile(sourceFile,after,&cancel);if(valid.failed())return valid;if(before.hash!=after.hash||before.bytes!=after.bytes)return Result::fail("Source audio changed while processing. Preview discarded.");sourceStamp=restorationFileStamp(sourceFile);
        valid=outputFile.getParentDirectory().createDirectory();if(valid.failed())return valid;TemporaryFile temp(outputFile);std::unique_ptr<OutputStream> stream(temp.getFile().createOutputStream());WavAudioFormat wav;auto writer=stream?wav.createWriterFor(stream,AudioFormatWriterOptions{}.withSampleRate(rate).withNumChannels(2).withBitsPerSample(32).withSampleFormat(AudioFormatWriterOptions::SampleFormat::floatingPoint)):nullptr;if(!writer)return Result::fail("Cannot create restoration preview.");
        for(int i=0;i<restored.getNumSamples();i+=4096){if(stopped(&cancel))return Result::fail("Restoration cancelled.");if(!writer->writeFromAudioSampleBuffer(restored,i,jmin(4096,restored.getNumSamples()-i)))return Result::fail("Cannot write restoration preview.");}writer.reset();stream.reset();
        if(stopped(&cancel))return Result::fail("Restoration cancelled.");if(restorationFileStamp(sourceFile)!=sourceStamp)return Result::fail("Source audio changed while saving the preview.");if(!temp.overwriteTargetFileWithTemporary())return Result::fail("Cannot publish restoration preview.");outputStamp=restorationFileStamp(outputFile);return Result::ok();
    }();}catch(const std::exception&){result=Result::fail("Not enough resources to restore audio. Source retained.");}done.store(true,std::memory_order_release);
}
}
