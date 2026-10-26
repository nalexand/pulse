// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <cmath>
namespace pulse {
using namespace juce;
double Studio::availableTakeSeconds(double rate,double at)const{
    if(!std::isfinite(rate)||rate<8000||rate>192000||!std::isfinite(at)||at<0||at>=4096||!std::isfinite(project.bpm)||project.bpm<30||project.bpm>300)return 0;
    double seconds=jmax(0.,std::min({(double)audioTakeSeconds,maxAudioSeconds,(4096-at)*60/project.bpm}));
    // Reserve the bounded cursor/pages needed after finalization, rather than
    // an entire decoded stereo copy. Editing/extra destinations stay disarmed
    // during this take. If page capacity is exhausted, only a short decode fits.
    uint64_t reserve=sizeof(StreamedAudio)+StreamedAudio::pageCount*(2ull*(StreamedAudio::pageFrames+1)*sizeof(float)+64)
        +(uint64_t)std::ceil(seconds*rate/StreamedAudio::pageFrames)*sizeof(std::atomic<float>);
    auto streams=assets.streamingStatistics();if(reserve<=StreamingBudget::limit-streams.bytes)return seconds;
    return jmax(0.,std::min({seconds,(assets.availableBytes()/8)/rate,(16.*1024*1024/8)/rate}));
}
bool Studio::setAudioTakeLimit(int seconds){
    if(recording||recorder.isActive()){status="Finish the take before changing its time limit.";return false;}
    if(seconds==0){PopupMenu menu;for(int value:{120,300,600,1200})menu.addItem(value,String(value/60)+" minutes",true,audioTakeSeconds==value);auto safe=Component::SafePointer<Studio>(this);menu.showMenuAsync(menuOptions(),[safe](int value){if(safe&&value)safe->setAudioTakeLimit(value);});return true;}
    if(seconds!=120&&seconds!=300&&seconds!=600&&seconds!=1200){status="Choose a 2, 5, 10 or 20 minute take limit.";return false;}
    audioTakeSeconds=seconds;saveInputSettings();status="Take limit: "+String(seconds/60)+" min. REC/Stop can finish early. Streaming-buffer capacity, timeline end and Punch can shorten it; audio and Long NOTES ignore the active pattern steps.";repaint();return true;
}
void Studio::samplerAudioToPlaylist(){
    if(recording||recorder.isActive()){status="Finish the current take first.";return;}const auto& track=project.tracks[(size_t)selectedTrack];
    if(track.isBus||track.instrument!=Instrument::sample||track.samplePath.isEmpty()){status="Select a sampler with an audio file first.";return;}
    auto base=currentFile==File()?root:currentFile.getParentDirectory();auto file=File::isAbsolutePath(track.samplePath)?File(track.samplePath):base.getChildFile(track.samplePath);auto count=project.clips.size();addAudioClip(file,selectedTrack,editCursorBeat);
    if(project.clips.size()>count)status="Full source placed as one Song audio clip at the Playlist cursor. Remove any old vocal-trigger pattern clips to avoid doubling; sampler notes/ranges are unchanged.";
}
}
