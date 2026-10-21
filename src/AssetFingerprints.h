// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <juce_cryptography/juce_cryptography.h>
#include <atomic>
#include <vector>
namespace pulse {
constexpr juce::int64 maxFingerprintFileBytes=512LL*1024*1024;
constexpr juce::int64 maxFingerprintPassBytes=2LL*1024*1024*1024;
constexpr int maxAssetFingerprints=1024;
struct AssetFingerprint {
    juce::String path,hash;
    juce::int64 bytes=0,modified=0,seen=0;
    bool operator==(const AssetFingerprint&)const=default;
};
using AssetFingerprints=std::vector<AssetFingerprint>;
juce::String assetPathKey(const juce::File&);
juce::String collectedAssetHash(const juce::File&);
juce::Result fingerprintAssetFile(const juce::File&,AssetFingerprint&,const std::atomic<bool>* cancel=nullptr);
juce::String assetFingerprintToken(const juce::File&);
juce::Result loadAssetFingerprints(const juce::File&,AssetFingerprints&);
juce::Result saveAssetFingerprints(const juce::File&,const AssetFingerprints&,const juce::String& expectedToken,const std::atomic<bool>* cancel=nullptr);
}
