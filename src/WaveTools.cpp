// SPDX-License-Identifier: AGPL-3.0-only
#include "WaveTools.h"
#include <cmath>
namespace pulse {
using namespace juce;
Result validateWaveAudio(const AudioBuffer<float>& audio,double rate){
    if(!std::isfinite(rate)||rate<8000||rate>192000||audio.getNumChannels()!=2||audio.getNumSamples()<1||audio.getNumSamples()>rate*120)
        return Result::fail("Audio must be stereo, 8-192 kHz and 1 frame to 120 seconds long.");
    for(int ch=0;ch<2;++ch)for(int i=0;i<audio.getNumSamples();++i)if(!std::isfinite(audio.getSample(ch,i)))return Result::fail("Audio contains a non-finite sample.");
    return Result::ok();
}
Result copyWaveRange(const AudioBuffer<float>& audio,double rate,int first,int end,AudioBuffer<float>& output){
    auto valid=validateWaveAudio(audio,rate);if(valid.failed())return valid;
    if(first<0||end>audio.getNumSamples()||end<=first)return Result::fail("Select at least one audio frame.");
    AudioBuffer<float> next(2,end-first);for(int ch=0;ch<2;++ch)next.copyFrom(ch,0,audio,ch,first,end-first);output=std::move(next);return Result::ok();
}
Result resampleWave(const AudioBuffer<float>& audio,double rate,double targetRate,AudioBuffer<float>& output,const std::function<bool()>& cancelled){
    auto valid=validateWaveAudio(audio,rate);if(valid.failed())return valid;
    if(!std::isfinite(targetRate)||targetRate<8000||targetRate>192000)return Result::fail("Use a sample rate between 8000 and 192000 Hz.");
    auto stopped=[&]{return cancelled&&cancelled();};if(stopped())return Result::fail("Audio conversion cancelled.");
    if(rate==targetRate){AudioBuffer<float> next;next.makeCopyOf(audio);output=std::move(next);return Result::ok();}
    int frames=jmax(1,(int)std::llround(audio.getNumSamples()*targetRate/rate));
    if(frames>targetRate*120)return Result::fail("Converted audio exceeds the 120-second limit.");
    // 64-zero-crossing Blackman-windowed sinc, scaled for downsampling. A bounded
    // polyphase table avoids trig in the sample loop. Endpoint extension preserves DC.
    constexpr int phases=512;constexpr double pi=3.14159265358979323846;
    double cutoff=.95*jmin(1.,targetRate/rate);int radius=(int)std::ceil(32/cutoff),taps=radius*2+1;
    std::vector<double> kernels((size_t)(phases+1)*taps);
    for(int phase=0;phase<=phases;++phase){if(stopped())return Result::fail("Audio conversion cancelled.");double fraction=phase/(double)phases,sum=0;auto* kernel=kernels.data()+(size_t)phase*taps;
        for(int k=0;k<taps;++k){double x=k-radius-fraction,z=x*cutoff,window=std::abs(z)>=32?0:.42+.5*std::cos(pi*z/32)+.08*std::cos(2*pi*z/32);double w=window*(std::abs(z)<1.e-12?cutoff:std::sin(pi*z)/(pi*x));kernel[k]=w;sum+=w;}
        for(int k=0;k<taps;++k)kernel[k]/=sum;
    }
    AudioBuffer<float> next(2,frames);int count=audio.getNumSamples();double stride=rate/targetRate;
    for(int i=0;i<frames;++i){if((i&255)==0&&stopped())return Result::fail("Audio conversion cancelled.");double position=i*stride;int centre=(int)std::floor(position);double fraction=(position-centre)*phases;int phase=jmin(phases-1,(int)fraction);double blend=fraction-phase;auto* kernel=kernels.data()+(size_t)phase*taps;
        double left=0,right=0;for(int k=0;k<taps;++k){int at=jlimit(0,count-1,centre+k-radius);double w=kernel[k]+blend*(kernel[k+taps]-kernel[k]);left+=w*audio.getSample(0,at);right+=w*audio.getSample(1,at);}
        float l=(float)left,r=(float)right;if(!std::isfinite(l)||!std::isfinite(r))return Result::fail("Converted audio exceeds the finite sample range.");next.setSample(0,i,l);next.setSample(1,i,r);
    }
    if(stopped())return Result::fail("Audio conversion cancelled.");output=std::move(next);return Result::ok();
}
Result spliceWave(const AudioBuffer<float>& audio,double rate,int first,int end,WaveOperation operation,const AudioBuffer<float>& clipboard,double clipboardRate,AudioBuffer<float>& output,int& selectedFirst,int& selectedEnd,const std::function<bool()>& cancelled){
    auto valid=validateWaveAudio(audio,rate);if(valid.failed())return valid;int n=audio.getNumSamples();
    if(first<0||end>n||end<=first)return Result::fail("Select at least one audio frame.");
    bool paste=operation==WaveOperation::insert||operation==WaveOperation::replace;
    AudioBuffer<float> converted;const auto* pasted=&clipboard;
    if(paste){valid=validateWaveAudio(clipboard,clipboardRate);if(valid.failed())return Result::fail("Copy valid audio in the waveform editor first.");
        // Reject oversized edits before allocating a conversion buffer.
        int added=jmax(1,(int)std::llround(clipboard.getNumSamples()*rate/clipboardRate));int removed=operation==WaveOperation::replace?end-first:0;
        if(n-removed+added>rate*120)return Result::fail("This edit exceeds the 120-second sample limit.");
        if(rate!=clipboardRate){valid=resampleWave(clipboard,clipboardRate,rate,converted,cancelled);if(valid.failed())return valid;pasted=&converted;}}
    if(cancelled&&cancelled())return Result::fail("Audio conversion cancelled.");
    int count=end-first,added=paste?pasted->getNumSamples():operation==WaveOperation::duplicate?count:0;
    int destination=operation==WaveOperation::duplicate?end:first;
    int removed=operation==WaveOperation::erase||operation==WaveOperation::replace?count:0;
    int length=operation==WaveOperation::trim?count:n-removed+added;
    if(length<1)return Result::fail("Cannot remove the entire file. Use Silence to keep its duration.");
    if(length>rate*120)return Result::fail("This edit exceeds the 120-second sample limit.");
    AudioBuffer<float> next(2,length);
    for(int ch=0;ch<2;++ch){
        if(operation==WaveOperation::trim)next.copyFrom(ch,0,audio,ch,first,count);
        else if(operation==WaveOperation::silence){next.copyFrom(ch,0,audio,ch,0,n);next.clear(ch,first,count);}
        else{
            if(destination>0)next.copyFrom(ch,0,audio,ch,0,destination);
            if(added>0)next.copyFrom(ch,destination,paste?*pasted:audio,ch,paste?0:first,added);
            int suffix=destination+removed;if(n>suffix)next.copyFrom(ch,destination+added,audio,ch,suffix,n-suffix);
        }
    }
    int a=operation==WaveOperation::trim?0:destination;
    int b=operation==WaveOperation::trim?length:added>0?a+added:operation==WaveOperation::silence?end:jmin(length,a+1);
    a=jlimit(0,length-1,a);b=jlimit(a+1,length,b);
    output=std::move(next);selectedFirst=a;selectedEnd=b;return Result::ok();
}
Result sliceWaveToPlaylist(const Project& project,int track,const String& path,double rate,int frames,int first,int end,double step,double at,Project& output){
    if(track<0||track>=(int)project.tracks.size()||project.tracks[(size_t)track].isBus)return Result::fail("Select a source channel before slicing.");
    if(path.isEmpty()||!File::isAbsolutePath(path)||!std::isfinite(rate)||rate<8000||rate>192000||frames<1||frames>rate*maxAudioSeconds||first<0||end>frames||end<=first)return Result::fail("Open a valid local audio selection first.");
    if(!std::isfinite(step)||step<.0625||step>64||!std::isfinite(project.bpm)||project.bpm<30||project.bpm>300||!std::isfinite(at)||at<0||at+(end-first)/rate*project.bpm/60>4096)return Result::fail("Slices must fit inside 0-4096 beats with a valid tempo and grid.");
    double stride=rate*60/project.bpm*step;int count=(int)std::ceil((end-first)/stride);
    // Rounding can leave the last boundary at end: never create a zero-frame tail.
    if(count>0&&first+(int64)std::llround((count-1)*stride)>=end)--count;
    if(count<1||project.clips.size()+count>maxClips)return Result::fail("Slicing exceeds the 512-clip Playlist limit. Choose a larger grid.");
    std::vector<int> boundaries;boundaries.push_back(first);for(int i=1;i<count;++i)boundaries.push_back(first+(int)std::llround(i*stride));boundaries.push_back(end);
    return sliceWaveFramesToPlaylist(project,track,path,rate,frames,boundaries,at,output);
}
Result sliceWaveFramesToPlaylist(const Project& project,int track,const String& path,double rate,int frames,const std::vector<int>& boundaries,double at,Project& output){
    if(track<0||track>=(int)project.tracks.size()||project.tracks[(size_t)track].isBus)return Result::fail("Select a source channel before slicing.");
    if(path.isEmpty()||!File::isAbsolutePath(path)||!std::isfinite(rate)||rate<8000||rate>192000||frames<1||frames>rate*maxAudioSeconds||boundaries.size()<2||boundaries.size()>maxClips+1)return Result::fail("Use between one and 512 valid audio slices.");
    for(size_t i=0;i<boundaries.size();++i)if(boundaries[i]<0||boundaries[i]>frames||(i>0&&boundaries[i]<=boundaries[i-1]))return Result::fail("Slice boundaries must be ordered, unique audio frames.");
    if(!std::isfinite(project.bpm)||project.bpm<30||project.bpm>300||!std::isfinite(at)||at<0||at+(boundaries.back()-boundaries.front())/rate*project.bpm/60>4096)return Result::fail("Slices must fit inside 0-4096 beats at a valid tempo.");
    if(project.clips.size()+boundaries.size()-1>maxClips)return Result::fail("Slicing exceeds the 512-clip Playlist limit.");
    Project next=project;for(size_t i=1;i<boundaries.size();++i){int a=boundaries[i-1],b=boundaries[i];Clip c;c.track=track;c.audioPath=path;c.sourceOffset=a/rate;c.beat=at+(a-boundaries.front())/rate*project.bpm/60;c.length=(b-a)/rate*project.bpm/60;next.clips.push_back(c);}
    next.songMode=true;output=std::move(next);return Result::ok();
}
Result findWaveTransients(const AudioBuffer<float>& audio,double rate,int first,int end,const TransientOptions& options,std::vector<int>& output){
    auto valid=validateWaveAudio(audio,rate);if(valid.failed())return valid;
    if(first<0||end>audio.getNumSamples()||end<=first)return Result::fail("Select audio before detecting transients.");
    if(!std::isfinite(options.thresholdDb)||options.thresholdDb<-80||options.thresholdDb>-3||!std::isfinite(options.ratio)||options.ratio<1.25||options.ratio>10||!std::isfinite(options.minGapMs)||options.minGapMs<10||options.minGapMs>1000)return Result::fail("Use a -80 to -3 dBFS floor, 1.25-10 rise ratio and 10-1000 ms minimum gap.");
    // Non-overlapping 5 ms peak windows, compared with a decaying 40 ms envelope.
    // This is an amplitude-rise detector; manual marks correct quiet/soft attacks.
    int window=jmax(1,(int)std::llround(rate*.005)),gap=jmax(1,(int)std::llround(rate*options.minGapMs*.001));double floor=std::pow(10.,options.thresholdDb/20),envelope=0,decay=std::exp(-.005/.040);
    std::vector<int> next;int last=first;
    for(int at=first;at<end;at+=window){int count=jmin(window,end-at);float peak=audio.getMagnitude(at,count);
        if(peak>=floor&&peak>=envelope*options.ratio){int onset=at;double trigger=jmax(floor,peak*.15);for(int i=at;i<at+count;++i)if(jmax(std::abs(audio.getSample(0,i)),std::abs(audio.getSample(1,i)))>=trigger){onset=i;break;}
            if(onset>first&&onset<end&&onset-last>=gap){if(next.size()>=256)return Result::fail("More than 256 transients found. Raise the floor/ratio or minimum gap, or select a shorter range.");next.push_back(onset);last=onset;}}
        envelope=jmax((double)peak,envelope*decay);
    }
    output=std::move(next);return Result::ok();
}
}
