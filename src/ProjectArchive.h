// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Project.h"
#include <atomic>
namespace pulse {
constexpr juce::int64 maxProjectArchiveBytes=512LL*1024*1024;
constexpr int maxProjectArchiveEntries=maxTracks+maxClips+2;
// Import publishes a new directory only; existing folders and caller output survive failure.
juce::Result exportProjectArchive(const Project&,const juce::File& base,const juce::File& target,const std::atomic<bool>* cancel=nullptr);
juce::Result importProjectArchive(const juce::File& archive,const juce::File& destination,juce::File& projectFile,const std::atomic<bool>* cancel=nullptr);
}
