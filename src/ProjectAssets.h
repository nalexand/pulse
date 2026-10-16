// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Project.h"
#include <atomic>
namespace pulse {
enum class AssetIssue { ready,missing,unreadable,unsupported,emptySampler,invalidRange };
struct AssetUse { bool clip=false; int index=0; };
struct ProjectAsset {
    juce::String key;
    juce::File file;
    std::vector<AssetUse> uses;
    AssetIssue issue=AssetIssue::missing;
    juce::String detail;
    double rate=0,duration=0;
    unsigned channels=0;
    std::vector<juce::File> candidates;
};
struct AssetReplacement { juce::String key; juce::File file; };
std::vector<ProjectAsset> projectAssets(const Project&,const juce::File& base);
void inspectProjectAssets(std::vector<ProjectAsset>&,const Project&,const std::atomic<bool>* cancel=nullptr);
juce::String findProjectAssets(std::vector<ProjectAsset>&,const juce::File& folder,const std::atomic<bool>* cancel=nullptr);
juce::Result relinkProjectAssets(Project&,const juce::File& base,const std::vector<AssetReplacement>&,int* references=nullptr);
juce::String assetIssueName(AssetIssue);
}
