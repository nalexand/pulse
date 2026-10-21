// SPDX-License-Identifier: AGPL-3.0-only
#include "AudioHistory.h"
#include <cmath>
#include <thread>
namespace pulse {
using namespace juce;
Result validateHistoryOptions(const HistoryOptions& o){if((o.seconds!=15&&o.seconds!=30&&o.seconds!=60)||(o.memoryMiB!=4&&o.memoryMiB!=16&&o.memoryMiB!=32&&o.memoryMiB!=64)||(int)o.mode<0||(int)o.mode>2)return Result::fail("Choose 15/30/60 seconds, 4/16/32/64 MiB and a supported audio input.");return Result::ok();}
Result saveHistoryOptions(const File& file,const HistoryOptions& o){auto valid=validateHistoryOptions(o);if(valid.failed())return valid;auto* d=new DynamicObject();d->setProperty("schema",1);d->setProperty("seconds",o.seconds);d->setProperty("memoryMiB",o.memoryMiB);d->setProperty("mode",(int)o.mode);auto data=JSON::toString(var(d),true);valid=file.getParentDirectory().createDirectory();if(valid.failed())return valid;TemporaryFile t(file);if(!t.getFile().replaceWithData(data.toRawUTF8(),data.getNumBytesAsUTF8())||t.getFile().loadFileAsString()!=data||!t.overwriteTargetFileWithTemporary())return Result::fail("Cannot save audio history preferences; previous file retained.");return Result::ok();}
Result loadHistoryOptions(const File& file,HistoryOptions& out){if(!file.exists())return Result::ok();if(!file.existsAsFile()||file.getSize()>4096)return Result::fail("Invalid audio history preferences.");auto v=JSON::parse(file);if(!v.isObject()||!v["schema"].isInt()||(int)v["schema"]!=1)return Result::fail("Unsupported audio history preferences.");for(auto* key:{"seconds","memoryMiB","mode"})if(!v[key].isInt())return Result::fail("Invalid audio history settings.");HistoryOptions next{(int)v["seconds"],(int)v["memoryMiB"],(InputMode)(int)v["mode"]};auto valid=validateHistoryOptions(next);if(valid.wasOk())out=next;return valid;}
String historySourceName(InputMode mode){return mode==InputMode::stereo?"Stereo inputs 1 + 2":mode==InputMode::first?"Input 1 (mono)":"Input 2 (mono)";}
void AudioHistory::pause(){enabled.store(false,std::memory_order_seq_cst);while(readers.load(std::memory_order_seq_cst)>0)std::this_thread::yield();}
void AudioHistory::disable(){pause();buffer.setSize(0,0);count=write=0;revision=invalidSamples=0;gap=true;published.store(0);fault.store(false);}
Result AudioHistory::start(double sampleRate,const HistoryOptions& settings){
    auto valid=validateHistoryOptions(settings);if(valid.failed())return valid;if(!std::isfinite(sampleRate)||sampleRate<8000||sampleRate>192000)return Result::fail("Audio history requires an 8-192 kHz input device.");
    pause();int channels=settings.mode==InputMode::stereo?2:1;int frames=(int)jmin(sampleRate*settings.seconds,(double)settings.memoryMiB*1024*1024/(channels*sizeof(float)));
    try{buffer.setSize(channels,frames);buffer.clear();}catch(const std::exception&){disable();return Result::fail("Not enough memory for audio history.");}
    options=settings;rate=sampleRate;count=write=0;revision=invalidSamples=0;endBeat=0;gap=true;published.store(0);fault.store(false);enabled.store(true,std::memory_order_seq_cst);return Result::ok();
}
void AudioHistory::capture(const AudioBuffer<float>& raw,int available,const HistorySpan& span){
    readers.fetch_add(1,std::memory_order_seq_cst);
    if(enabled.load(std::memory_order_seq_cst)){
        auto signal=selectInput(raw,available,options.mode);
        if(span.rate!=rate){fault.store(true);enabled.store(false);}
        else if(!span.valid||span.frames<1||span.frames>raw.getNumSamples()||!signal.left||signal.channels==0||(options.mode==InputMode::stereo&&signal.channels!=2)
            ||!std::isfinite(span.bpm)||span.bpm<30||span.bpm>300||!std::isfinite(span.firstBeat)||span.firstBeat<0){gap=true;}
        else{
            if(gap||span.revision!=revision||span.bpm!=bpm||std::abs(span.firstBeat-endBeat)>1e-7){count=write=0;invalidSamples=0;}
            bpm=span.bpm;revision=span.revision;gap=false;int capacity=buffer.getNumSamples(),skip=jmax(0,span.frames-capacity);
            for(int i=skip;i<span.frames;++i){const float* channels[]={signal.left,signal.right};for(int ch=0;ch<buffer.getNumChannels();++ch){float value=channels[ch]?channels[ch][i]:0;if(!std::isfinite(value)){value=0;++invalidSamples;}buffer.setSample(ch,write,value);}write=(write+1)%capacity;}
            count=jmin(capacity,count+span.frames);endBeat=span.firstBeat+span.frames*bpm/(60*rate);published.store((uint64_t)count,std::memory_order_release);
        }
    }readers.fetch_sub(1,std::memory_order_seq_cst);
}
Result AudioHistory::copyFrozen(AudioHistorySnapshot& out,std::atomic<bool>* cancel)const{
    if(active())return Result::fail("Freeze audio history before copying it.");if(fault.load()||count<1)return Result::fail("No valid Song input history. Enable an input and play Song, then Freeze.");
    if(cancel&&cancel->load())return Result::fail("Audio history cancelled.");AudioHistorySnapshot next;next.audio.setSize(buffer.getNumChannels(),count);next.options=options;next.rate=rate;next.bpm=bpm;next.revision=revision;next.invalidSamples=invalidSamples;next.endBeat=endBeat;next.firstBeat=endBeat-count*bpm/(60*rate);
    int first=(write-count+buffer.getNumSamples())%buffer.getNumSamples();
    for(int pos=0;pos<count;){if(cancel&&cancel->load())return Result::fail("Audio history cancelled.");int source=(first+pos)%buffer.getNumSamples(),n=jmin(count-pos,buffer.getNumSamples()-source,4096);for(int ch=0;ch<buffer.getNumChannels();++ch)next.audio.copyFrom(ch,pos,buffer,ch,source,n);pos+=n;}
    out=std::move(next);return Result::ok();
}
namespace {
Result validateSnapshot(const AudioHistorySnapshot& s){double seconds=s.audio.getNumSamples()/s.rate;if(!std::isfinite(s.rate)||s.rate<8000||s.rate>192000||s.audio.getNumChannels()<1||s.audio.getNumChannels()>2||s.audio.getNumChannels()!=(s.options.mode==InputMode::stereo?2:1)||s.audio.getNumSamples()<1||seconds>60||!std::isfinite(s.bpm)||s.bpm<30||s.bpm>300||!std::isfinite(s.firstBeat)||s.firstBeat< -1e-7||!std::isfinite(s.endBeat)||s.endBeat>s.firstBeat+600||s.endBeat>4096||std::abs(s.endBeat-s.firstBeat-seconds*s.bpm/60)>1e-7)return Result::fail("Invalid captured Song range.");return validateHistoryOptions(s.options);}
}
Result appendAudioHistory(const Project& source,const AudioHistorySnapshot& snapshot,const File& file,int track,Project& out){
    auto valid=validateSnapshot(snapshot);if(valid.failed())return valid;if(track<0||track>=(int)source.tracks.size()||source.tracks[(size_t)track].isBus||source.clips.size()>=maxClips||source.bpm!=snapshot.bpm||!file.existsAsFile())return Result::fail("Choose an instrument lane, retain captured tempo and free one Playlist slot.");
    auto next=source;Clip c;c.track=track;c.beat=jmax(0.,snapshot.firstBeat);c.length=snapshot.endBeat-snapshot.firstBeat;c.audioPath=file.getFullPathName();next.clips.push_back(c);next.songMode=true;out=std::move(next);return Result::ok();
}
String historySummary(const AudioHistorySnapshot& s){return historySourceName(s.options.mode)+" / dry input / "+String(s.audio.getNumChannels())+" channels\n"+String(s.audio.getNumSamples())+" frames at "+String(s.rate,0)+" Hz / "+String(s.audio.getNumSamples()/s.rate,4)+" s\nSong beats "+String(s.firstBeat,6)+" - "+String(s.endBeat,6)+" / "+String(s.bpm,3)+" BPM / revision "+String((int64)s.revision)+"\nBudget "+String(s.options.memoryMiB)+" MiB, requested "+String(s.options.seconds)+" s; actual duration also follows available memory/input.\nNon-finite input samples sanitized in this segment (including overwritten history): "+String((int64)s.invalidSamples)+"\n\nOnly complete, contiguous Song playback blocks are retained. Seeks, wraps, preview/take/PAT and source/revision discontinuities start a new segment. Stopped input and partial end/loop blocks are not retained. Freeze pauses retention; Start begins a new history. Off clears memory and discards an unapplied owned copy.\nAudition is raw, with headroom attenuation; input monitoring must be off. Apply inserts a dry audio clip at its captured Song beat as one Undo. Lane volume/effects and Master process it normally. Source audio is not quantized or normalized. Device/input latency is not compensated. Bus/output history, stopped-transport capture and multi-lap reconstruction remain planned.";}
String historyFileStamp(const File& f){return f.existsAsFile()?f.getFullPathName()+"|"+String(f.getSize())+"|"+String(f.getLastModificationTime().toMilliseconds())+"|"+SHA256(f).toHexString():String("missing");}
void HistorySaveTask::run(){
    try{result=[&]()->Result{
        auto valid=history.copyFrozen(snapshot,&cancel);if(valid.failed())return valid;valid=validateSnapshot(snapshot);if(valid.failed())return valid;
        if(folder.exists())return Result::fail("History preview destination already exists.");valid=folder.createDirectory();if(valid.failed())return valid;auto file=folder.getChildFile("input.wav");TemporaryFile temporary(file);std::unique_ptr<OutputStream> stream(temporary.getFile().createOutputStream());WavAudioFormat wav;
        auto writer=stream?wav.createWriterFor(stream,AudioFormatWriterOptions{}.withSampleRate(snapshot.rate).withNumChannels(snapshot.audio.getNumChannels()).withBitsPerSample(32).withSampleFormat(AudioFormatWriterOptions::SampleFormat::floatingPoint)):nullptr;if(!writer)return Result::fail("Cannot create frozen input copy.");
        for(int pos=0;pos<snapshot.audio.getNumSamples();pos+=4096){if(cancel.load())return Result::fail("Audio history cancelled.");if(!writer->writeFromAudioSampleBuffer(snapshot.audio,pos,jmin(4096,snapshot.audio.getNumSamples()-pos)))return Result::fail("Cannot write frozen input.");}
        writer.reset();stream.reset();AudioFormatManager fm;fm.registerBasicFormats();std::unique_ptr<AudioFormatReader> reader(fm.createReaderFor(temporary.getFile()));if(!reader||reader->sampleRate!=snapshot.rate||reader->lengthInSamples!=snapshot.audio.getNumSamples()||reader->numChannels!=(unsigned)snapshot.audio.getNumChannels()||!reader->usesFloatingPointData)return Result::fail("Frozen input header verification failed.");
        AudioBuffer<float> block(snapshot.audio.getNumChannels(),4096);for(int pos=0;pos<snapshot.audio.getNumSamples();pos+=4096){if(cancel.load())return Result::fail("Audio history cancelled.");int n=jmin(4096,snapshot.audio.getNumSamples()-pos);if(!reader->read(&block,0,n,pos,true,true))return Result::fail("Cannot verify frozen input.");for(int ch=0;ch<block.getNumChannels();++ch)for(int i=0;i<n;++i)if(block.getSample(ch,i)!=snapshot.audio.getSample(ch,pos+i))return Result::fail("Frozen input read-back differs.");}reader.reset();
        if(cancel.load())return Result::fail("Audio history cancelled.");if(!temporary.overwriteTargetFileWithTemporary())return Result::fail("Cannot publish frozen input.");hash=SHA256(file).toHexString();stamp=historyFileStamp(file);
        auto* d=new DynamicObject();d->setProperty("schema",1);d->setProperty("application","Pulse Studio retrospective audio");d->setProperty("sha256",hash);d->setProperty("frames",snapshot.audio.getNumSamples());d->setProperty("channels",snapshot.audio.getNumChannels());d->setProperty("rate",snapshot.rate);d->setProperty("bpm",snapshot.bpm);d->setProperty("firstBeat",snapshot.firstBeat);d->setProperty("endBeat",snapshot.endBeat);d->setProperty("revision",(int64)snapshot.revision);d->setProperty("source",historySourceName(snapshot.options.mode));d->setProperty("summary",historySummary(snapshot));auto data=JSON::toString(var(d),true);auto meta=folder.getChildFile("capture.json");if(!meta.replaceWithData(data.toRawUTF8(),data.getNumBytesAsUTF8())||meta.loadFileAsString()!=data)return Result::fail("Cannot verify frozen input metadata.");if(cancel.load())return Result::fail("Audio history cancelled.");return Result::ok();
    }();}catch(const std::exception&){result=Result::fail("Not enough resources to freeze audio history. Memory and music retained.");}done.store(true,std::memory_order_release);
}
}
