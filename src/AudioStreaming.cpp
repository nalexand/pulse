// SPDX-License-Identifier: AGPL-3.0-only
#include "AudioStreaming.h"
#include <algorithm>
#include <cmath>
namespace pulse {
using namespace juce;
StreamedAudio::StreamedAudio(std::unique_ptr<AudioFormatReader> r,double offset,bool isOffline,std::shared_ptr<StreamingBudget> b,uint64_t bytes)
    :sampleRate(r->sampleRate),frames(r->lengthInSamples),offline(isOffline),reader(std::move(r)),budget(std::move(b)),allocationBytes(bytes),
     seedPage(jlimit(int64_t(0),frames-1,(int64_t)(offset*sampleRate))/pageFrames),peakCount((int)((frames+pageFrames-1)/pageFrames)){
    if(auto* input=dynamic_cast<FileInputStream*>(reader->input)){sourceFile=input->getFile();sourceBytes=sourceFile.getSize();sourceModified=sourceFile.getLastModificationTime().toMilliseconds();}
    peaks=std::make_unique<std::atomic<float>[]>((size_t)peakCount);for(int i=0;i<peakCount;++i)peaks[(size_t)i].store(0);
    for(auto& page:pages)page.data.setSize(2,pageFrames+1);
    wanted.store(seedPage*pageFrames);loopFrame.store(seedPage*pageFrames);budget->sources.fetch_add(1);
}
StreamedAudio::~StreamedAudio(){budget->sources.fetch_sub(1);budget->bytes.fetch_sub(allocationBytes);}
void StreamedAudio::fail(){if(!readFailed.exchange(true))budget->readErrors.fetch_add(1);}
bool StreamedAudio::readSource(AudioBuffer<float>& data,int count,int64_t start){
    auto stable=[&]{return sourceFile==File()||(sourceFile.existsAsFile()&&sourceFile.getSize()==sourceBytes&&sourceFile.getLastModificationTime().toMilliseconds()==sourceModified);};
    if(failed()||!stable()||!reader->read(&data,0,count,start,true,true)||!stable()){fail();return false;}return true;
}
bool StreamedAudio::readPage(Page& page,int64_t index){
    const int64_t base=index*pageFrames;const int count=(int)jmin(int64_t(pageFrames+1),frames-base);
    if(count<=0)return false;
    if(!readSource(page.data,count,base))return false;
    if(reader->numChannels==1)page.data.copyFrom(1,0,page.data,0,0,count);
    if(count<pageFrames+1)for(int ch=0;ch<2;++ch)page.data.clear(ch,count,pageFrames+1-count);
    page.base=base;float value=jmax(page.data.getMagnitude(0,0,jmin(count,pageFrames)),page.data.getMagnitude(1,0,jmin(count,pageFrames)));
    peaks[(size_t)index].store(value,std::memory_order_relaxed);return true;
}
bool StreamedAudio::prime(){
    // Header/open and initial read-ahead happen on the control thread, before
    // publication. Offline cursors have no worker and fill on demand instead.
    if(offline)return true;
    for(int n=0;n<4&&seedPage+n<peakCount;++n){auto& page=pages[(size_t)n];if(!readPage(page,seedPage+n))return false;page.state.store(1,std::memory_order_release);}return true;
}
bool StreamedAudio::copy(Page& page,double position,float& left,float& right)const{
    int64_t index=(int64_t)position,base=page.base.load(std::memory_order_relaxed);if(base<0||index<base||index>=base+pageFrames||index>=frames)return false;
    int at=(int)(index-base),next=index+1<frames?at+1:at;float alpha=(float)(position-index);
    auto* l=page.data.getReadPointer(0);auto* r=page.data.getReadPointer(1);
    left=l[at]+alpha*(l[next]-l[at]);right=r[at]+alpha*(r[next]-r[at]);return true;
}
bool StreamedAudio::sample(double position,float& left,float& right){
    left=right=0;if(!std::isfinite(position)||position<0||position>=frames)return true;
    if(failed()){if(!offline)budget->missedFrames.fetch_add(1,std::memory_order_relaxed);return false;}
    const int64_t index=(int64_t)position;
    if(offline){auto& page=pages[0];if(page.base.load()<0||index<page.base.load()||index>=page.base.load()+pageFrames)if(!readPage(page,index/pageFrames))return false;return copy(page,position,left,right);}
    wanted.store(index,std::memory_order_relaxed);
    // A bounded scan; a page can never be freed/overwritten while state is 2.
    // No waits, locks, refcount changes, allocations, disk calls or OS events.
    for(auto& page:pages){int ready=1;if(!page.state.compare_exchange_strong(ready,2,std::memory_order_acquire,std::memory_order_relaxed))continue;
        bool found=copy(page,position,left,right);page.state.store(1,std::memory_order_release);if(found)return true;}
    budget->missedFrames.fetch_add(1,std::memory_order_relaxed);return false;
}
bool StreamedAudio::service(AudioBuffer<float>& scratch){
    if(offline||failed())return false;
    const int64_t current=jlimit(int64_t(0),frames-1,wanted.load(std::memory_order_relaxed))/pageFrames;
    const int64_t loop=jlimit(int64_t(0),frames-1,loopFrame.load(std::memory_order_relaxed))/pageFrames;
    // Reserve the next pages at BOTH heads. A loop start near the end of
    // a page may cross into its successor during one hardware callback, before
    // the worker can observe the wrap. A single reserved loop page is too late.
    std::array<int64_t,pageCount> desired{current,current+1,current+2,loop,loop+1,loop+2};
    for(auto index:desired){if(index<0||index>=peakCount)continue;bool present=false;
        // Only this worker changes base. Observing ready or callback-pinned
        // metadata needs no page lock and cannot contend with audio reads.
        for(auto& page:pages){int state=page.state.load(std::memory_order_acquire);if((state==1||state==2)&&page.base.load(std::memory_order_relaxed)==index*pageFrames)present=true;}
        if(present)continue;
        for(auto& page:pages){int state=page.state.load(std::memory_order_relaxed);if(state!=0&&state!=1)continue;
            // Do not briefly claim a retained page merely to inspect it: that
            // could cause an avoidable one-frame miss in the callback. This is
            // the only writer of base, so metadata can be checked before CAS.
            if(state==1&&std::find(desired.begin(),desired.end(),page.base.load(std::memory_order_relaxed)/pageFrames)!=desired.end())continue;
            if(!page.state.compare_exchange_strong(state,3,std::memory_order_acquire,std::memory_order_relaxed))continue;
            bool ok=readPage(page,index);page.state.store(ok?1:0,std::memory_order_release);return true;
        }
        // The callback may briefly pin a desired page. Retry next worker pass.
        return false;
    }
    // Thumbnail scan uses separate storage and runs only after read-ahead is
    // satisfied. One page per worker pass bounds interference with playback.
    if(scratch.getNumSamples()>0&&scanPage<peakCount){int64_t base=int64_t(scanPage)*pageFrames;int count=(int)jmin(int64_t(pageFrames),frames-base);
        if(!readSource(scratch,count,base))return false;
        float value=scratch.getMagnitude(0,0,count);if(reader->numChannels>1)value=jmax(value,scratch.getMagnitude(1,0,count));
        peaks[(size_t)scanPage].store(value,std::memory_order_relaxed);if(++scanPage==peakCount)peaksComplete.store(true,std::memory_order_release);return true;}
    return false;
}
float StreamedAudio::peak(int64_t start,int count)const{
    if(count<=0||start>=frames||start+count<=0)return 0;int a=(int)(jmax(int64_t(0),start)/pageFrames),b=(int)(jmin(frames-1,start+count-1)/pageFrames);float value=0;
    for(int i=a;i<=b;++i)value=jmax(value,peaks[(size_t)i].load(std::memory_order_relaxed));return value;
}
AudioStreaming::AudioStreaming():Thread("Playlist disk streaming"){}
AudioStreaming::~AudioStreaming(){signalThreadShouldExit();notify();stopThread(-1);}
std::shared_ptr<StreamedAudio> AudioStreaming::open(std::unique_ptr<AudioFormatReader> reader,double offset,bool offline,String& error){
    error.clear();if(!reader||!std::isfinite(reader->sampleRate)||reader->sampleRate<8000||reader->sampleRate>192000||reader->numChannels<1||reader->lengthInSamples<=0||reader->lengthInSamples>reader->sampleRate*1800||!std::isfinite(offset)||offset<0||offset>1800){error="Invalid Playlist stream metadata or offset (30 minutes, 8-192 kHz).";return {};}
    const uint64_t bytes=sizeof(StreamedAudio)+StreamedAudio::pageCount*(2ull*(StreamedAudio::pageFrames+1)*sizeof(float)+64)
        +(uint64_t)((reader->lengthInSamples+StreamedAudio::pageFrames-1)/StreamedAudio::pageFrames)*sizeof(std::atomic<float>);
    auto used=budget->bytes.load();do{if(bytes>StreamingBudget::limit-used){error="Playlist streaming buffers are full (128 MiB). Remove unused long clips or close their previews, then retry.";return {};}}while(!budget->bytes.compare_exchange_weak(used,used+bytes));
    std::shared_ptr<StreamedAudio> cursor;StreamedAudio* constructed=nullptr;
    try{constructed=new StreamedAudio(std::move(reader),offset,offline,budget,bytes);cursor=std::shared_ptr<StreamedAudio>(constructed);}
    // shared_ptr deletes a constructed object if its control-block allocation
    // fails; a constructor failure has no destructor and needs explicit refund.
    catch(const std::exception&){if(!constructed)budget->bytes.fetch_sub(bytes);error="Not enough memory for Playlist streaming buffers.";return {};}
    if(!cursor->prime()){error="Could not read initial Playlist audio data.";return {};}
    if(!offline){const ScopedLock lock(listLock);cursors.erase(std::remove_if(cursors.begin(),cursors.end(),[](const auto& weak){return weak.expired();}),cursors.end());cursors.push_back(cursor);
        if(!isThreadRunning()&&!startThread()){error="Cannot start the Playlist streaming worker.";cursors.pop_back();return {};}}
    notify();return cursor;
}
AudioStreaming::Statistics AudioStreaming::statistics()const{return {budget->bytes.load(),budget->sources.load(),budget->missedFrames.load(),budget->readErrors.load()};}
void AudioStreaming::run(){
    AudioBuffer<float> scratch(2,StreamedAudio::pageFrames),noThumbnail;std::vector<std::shared_ptr<StreamedAudio>> live;size_t thumbnail=0;
    while(!threadShouldExit()){
        {const ScopedLock lock(listLock);live.clear();for(auto it=cursors.begin();it!=cursors.end();)if(auto cursor=it->lock()){live.push_back(std::move(cursor));++it;}else it=cursors.erase(it);}
        // Serve every cursor's read-ahead before doing one thumbnail page.
        // Thumbnail work cannot scale into hundreds of reads per worker pass.
        bool filled=false;for(auto& cursor:live){if(threadShouldExit())break;filled|=cursor->service(noThumbnail);}
        if(!filled&&!live.empty()&&!threadShouldExit())live[thumbnail++%live.size()]->service(scratch);
        live.clear();wait(2);
    }
}
}
