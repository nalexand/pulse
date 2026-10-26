// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "ProjectPersistence.h"
#include <atomic>
namespace pulse {
// Managed take directories contain a uniquely named Take WAV, session.pulse and checkpoint.json.
// Only the disk writer commits checkpoints, after a successful OS stream flush.
struct TakeCheckpoint {
    juce::File directory;
    juce::String sessionHash,tailHash;
    juce::MemoryBlock header;
    double rate=48000,beat=0,bpm=120;
    int channels=1,track=0;
    uint64_t frames=0,limit=0;
    int64_t dataOffset=0,created=0;
    juce::File audio()const{return directory.getChildFile("Take_"+directory.getFileName().substring(0,8)+".wav");}
    juce::File journal()const{return directory.getChildFile("checkpoint.json");}
};
juce::Result prepareTakeCheckpoint(const Project&,const juce::File& base,const juce::File& recordings,int track,double beat,double rate,int channels,uint64_t frameLimit,TakeCheckpoint&);
juce::Result commitTakeCheckpoint(TakeCheckpoint&,uint64_t frames,int64_t dataOffset);
juce::Result readTakeCheckpoint(const juce::File& directory,TakeCheckpoint&);
juce::Result completeTakeCheckpoint(const TakeCheckpoint&,const Project&,const juce::File& base);
struct TakeRecoveryScan {std::vector<TakeCheckpoint> takes;int invalid=0,completed=0;bool limited=false;};
juce::Result scanTakeCheckpoints(const juce::File& recordings,TakeRecoveryScan&);
// Copies only a verified flushed PCM prefix. Never patches/deletes the original.
// Repeated recovery of the same checkpoint returns the verified existing copy.
juce::Result recoverTakeCheckpoint(const TakeCheckpoint&,juce::File& project,const std::atomic<bool>* cancel=nullptr);
}
