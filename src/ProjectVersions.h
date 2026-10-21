// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Project.h"
#include <atomic>
namespace pulse {
constexpr int maxVersionRows=256,maxVersionScan=5000;
struct ProjectVersion {juce::File file;juce::String name,stamp;juce::Time modified;juce::int64 bytes=0;};
juce::String projectVersionToken(const juce::File&);
std::vector<ProjectVersion> scanProjectVersions(const juce::File&,const juce::String& query,juce::String& message,const std::atomic<bool>* cancel=nullptr);
juce::Result readProjectVersion(const ProjectVersion&,Project&,juce::String& token);
juce::Result saveProjectVersion(const Project&,const juce::File& base,const juce::File& library,const juce::String& label,juce::File& saved,const std::atomic<bool>* cancel=nullptr);
juce::String compareProjectVersion(const Project& current,const juce::File& currentBase,const Project& saved,const juce::File& savedBase);
}
