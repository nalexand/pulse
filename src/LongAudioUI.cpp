// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <cmath>
namespace pulse {
using namespace juce;
double Studio::availableTakeSeconds(double rate,double at)const{
    if(!std::isfinite(rate)||rate<8000||rate>192000||!std::isfinite(at)||at<0||at>=4096||!std::isfinite(project.bpm)||project.bpm<30||project.bpm>300)return 0;
    // Assets decode mono and stereo to stereo floats. Reserve a playable take,
    // including mono input, within the remaining shared decoded-audio budget.
    return jmax(0.,std::min({(double)audioTakeSeconds,maxAudioSeconds,(assets.availableBytes()/8)/rate,(4096-at)*60/project.bpm}));
}
bool Studio::setAudioTakeLimit(int seconds){
    if(recording||recorder.isActive()){status="Finish the take before changing its time limit.";return false;}
    if(seconds==0){PopupMenu menu;for(int value:{120,300,600,1200})menu.addItem(value,String(value/60)+" minutes",true,audioTakeSeconds==value);auto safe=Component::SafePointer<Studio>(this);menu.showMenuAsync(menuOptions(),[safe](int value){if(safe&&value)safe->setAudioTakeLimit(value);});return true;}
    if(seconds!=120&&seconds!=300&&seconds!=600&&seconds!=1200){status="Choose a 2, 5, 10 or 20 minute audio take limit.";return false;}
    audioTakeSeconds=seconds;saveInputSettings();status="Audio take limit: "+String(seconds/60)+" min. REC/Stop can finish early. Available sample-cache space, timeline end and Punch can shorten it; pattern steps do not limit audio.";repaint();return true;
}
void Studio::samplerAudioToPlaylist(){
    if(recording||recorder.isActive()){status="Finish the current take first.";return;}const auto& track=project.tracks[(size_t)selectedTrack];
    if(track.isBus||track.instrument!=Instrument::sample||track.samplePath.isEmpty()){status="Select a sampler with an audio file first.";return;}
    auto base=currentFile==File()?root:currentFile.getParentDirectory();auto file=File::isAbsolutePath(track.samplePath)?File(track.samplePath):base.getChildFile(track.samplePath);auto count=project.clips.size();addAudioClip(file,selectedTrack,editCursorBeat);
    if(project.clips.size()>count)status="Full source placed as one Song audio clip at the Playlist cursor. Remove any old vocal-trigger pattern clips to avoid doubling; sampler notes/ranges are unchanged.";
}
}
