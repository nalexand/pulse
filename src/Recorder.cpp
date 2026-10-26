// SPDX-License-Identifier: AGPL-3.0-only
#include "Recorder.h"
#include "Project.h"
#include <cmath>
#include <chrono>
namespace pulse {
using namespace juce;
Recorder::~Recorder(){finish();}
Result Recorder::start(const File& file,double rate,int inputChannels,double maximumSeconds,const TakeCheckpoint* recovery){
    if(writer)return Result::fail("A take is already open.");
    if(file.existsAsFile())return Result::fail("Recording destination already exists.");
    if(!std::isfinite(rate)||rate<8000||rate>192000||inputChannels<1||!std::isfinite(maximumSeconds)||maximumSeconds<=0||maximumSeconds>maxAudioSeconds||maximumSeconds*rate<1)return Result::fail("Select an audio input in F10 first.");
    auto result=file.getParentDirectory().createDirectory();if(result.failed())return result;
    sampleRate=rate;frameLimit=(uint64_t)std::floor(rate*maximumSeconds);channels=jlimit(1,2,inputChannels);ring.setSize(channels,(int)rate*2);fifo.setTotalSize(ring.getNumSamples());
    checkpoint.reset();flushed.store(0);if(recovery){TakeCheckpoint reviewed;auto checked=readTakeCheckpoint(recovery->directory,reviewed);if(checked.failed()||file!=recovery->audio()||reviewed.frames!=0||reviewed.rate!=rate||reviewed.channels!=channels||reviewed.limit!=frameLimit||reviewed.track!=recovery->track||reviewed.beat!=recovery->beat||reviewed.bpm!=recovery->bpm||reviewed.created!=recovery->created||reviewed.sessionHash!=recovery->sessionHash||projectFileToken(recovery->directory.getChildFile("session.pulse"))!=recovery->sessionHash)return Result::fail("Recording recovery metadata changed. Try REC again.");checkpoint=reviewed;}
    auto fileStream=file.createOutputStream();outputFile=fileStream.get();std::unique_ptr<OutputStream> output(std::move(fileStream));
    writer=WavAudioFormat().createWriterFor(output,AudioFormatWriterOptions{}.withSampleRate(rate).withNumChannels(channels).withBitsPerSample(24));
    if(!writer){outputFile=nullptr;return Result::fail("Cannot create recording WAV.");}
    dataOffset=outputFile->getPosition();if(!writer->flush()){writer.reset();outputFile=nullptr;return Result::fail("Cannot initialize recording header.");}outputFile->flush();if(outputFile->getStatus().failed()){writer.reset();outputFile=nullptr;return Result::fail("Cannot flush recording header.");}
    if(checkpoint){auto committed=commitTakeCheckpoint(*checkpoint,0,dataOffset);if(committed.failed()){writer.reset();outputFile=nullptr;return committed;}}
    failed.store(false);done.store(false);limitReached.store(false);captured.store(0);inputPeak.store(0);
    try{worker=std::thread([this]{try{
        uint64_t written=0,nextFlush=(uint64_t)sampleRate;bool diskFailed=false;
        auto flush=[&]{bool ok=writer->flush();outputFile->flush();ok=ok&&outputFile->getStatus().wasOk();if(!ok){failed.store(true);diskFailed=true;return;}flushed.store(written,std::memory_order_release);if(checkpoint&&commitTakeCheckpoint(*checkpoint,written,dataOffset).failed())failed.store(true);};
        while(!done.load()||fifo.getNumReady()>0){
            int a=0,an=0,b=0,bn=0;fifo.prepareToRead(fifo.getNumReady(),a,an,b,bn);
            auto write=[&](int start,int count){if(count>0&&!diskFailed){if(writer->writeFromAudioSampleBuffer(ring,start,count))written+=(uint64_t)count;else{diskFailed=true;failed.store(true);}}};
            write(a,an);write(b,bn);fifo.finishedRead(an+bn);
            if(written>=nextFlush){flush();nextFlush=written+(uint64_t)sampleRate;}
            if(an+bn==0)std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        flush();
    }catch(...){failed.store(true);}});}catch(const std::exception&){writer.reset();outputFile=nullptr;return Result::fail("Cannot start recording worker; local take files retained.");}
    enabled.store(true,std::memory_order_release);return Result::ok();
}
void Recorder::capture(const float* left,const float* right,int frames){
    readers.fetch_add(1,std::memory_order_seq_cst);
    if(enabled.load(std::memory_order_seq_cst)){
        auto total=captured.load();auto remaining=total<frameLimit?frameLimit-total:0;int count=(int)jmin((uint64_t)jmax(0,frames),remaining);
        int a=0,an=0,b=0,bn=0;fifo.prepareToWrite(count,a,an,b,bn);
        if(an+bn<count)failed.store(true);
        float peak=0;const float* inputs[]={left,right?right:left};
        for(int ch=0;ch<channels;++ch){auto* source=inputs[ch];
            for(int i=0;i<an+bn;++i){float value=source&&std::isfinite(source[i])?source[i]:0;ring.setSample(ch,i<an?a+i:b+i-an,value);peak=jmax(peak,std::abs(value));}}
        fifo.finishedWrite(an+bn);captured.fetch_add((uint64_t)(an+bn));inputPeak.store(peak);
        if((uint64_t)count==remaining)limitReached.store(true);
    }
    readers.fetch_sub(1,std::memory_order_seq_cst);
}
Result Recorder::finish(){
    enabled.store(false,std::memory_order_seq_cst);
    while(readers.load(std::memory_order_seq_cst)>0)std::this_thread::yield();
    done.store(true);if(worker.joinable())worker.join();writer.reset();outputFile=nullptr;
    return failed.load()?Result::fail("Recording stopped after a disk/checkpoint error or input-buffer overflow. The WAV and latest acknowledged checkpoint are kept locally."):Result::ok();
}
}
