// SPDX-License-Identifier: AGPL-3.0-only
#include "Recorder.h"
#include "Project.h"
#include <cmath>
#include <chrono>
namespace pulse {
using namespace juce;
Recorder::~Recorder(){finish();}
Result Recorder::start(const File& file,double rate,int inputChannels,double maximumSeconds){
    if(writer)return Result::fail("A take is already open.");
    if(file.existsAsFile())return Result::fail("Recording destination already exists.");
    if(!std::isfinite(rate)||rate<8000||rate>192000||inputChannels<1||!std::isfinite(maximumSeconds)||maximumSeconds<=0||maximumSeconds>maxAudioSeconds||maximumSeconds*rate<1)return Result::fail("Select an audio input in F10 first.");
    auto result=file.getParentDirectory().createDirectory();if(result.failed())return result;
    sampleRate=rate;frameLimit=(uint64_t)std::floor(rate*maximumSeconds);channels=jlimit(1,2,inputChannels);ring.setSize(channels,(int)rate*2);fifo.setTotalSize(ring.getNumSamples());
    std::unique_ptr<OutputStream> output=file.createOutputStream();
    writer=WavAudioFormat().createWriterFor(output,AudioFormatWriterOptions{}.withSampleRate(rate).withNumChannels(channels).withBitsPerSample(24));
    if(!writer)return Result::fail("Cannot create recording WAV.");
    failed.store(false);done.store(false);limitReached.store(false);captured.store(0);inputPeak.store(0);
    worker=std::thread([this]{
        uint64_t written=0,nextFlush=(uint64_t)sampleRate;
        while(!done.load()||fifo.getNumReady()>0){
            int a=0,an=0,b=0,bn=0;fifo.prepareToRead(fifo.getNumReady(),a,an,b,bn);
            if(an>0&&!writer->writeFromAudioSampleBuffer(ring,a,an))failed.store(true);
            if(bn>0&&!writer->writeFromAudioSampleBuffer(ring,b,bn))failed.store(true);
            fifo.finishedRead(an+bn);written+=(uint64_t)(an+bn);
            if(written>=nextFlush){if(!writer->flush())failed.store(true);nextFlush=written+(uint64_t)sampleRate;}
            if(an+bn==0)std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        if(!writer->flush())failed.store(true);
    });
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
    done.store(true);if(worker.joinable())worker.join();writer.reset();
    return failed.load()?Result::fail("Recording stopped after a disk error or input-buffer overflow. The partial WAV is kept locally."):Result::ok();
}
}
