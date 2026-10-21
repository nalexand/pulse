// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <juce_core/juce_core.h>
namespace pulse {
constexpr int maxSampleFavorites=256,maxSampleFavoritesBytes=1048576;
bool sampleNameMatches(const juce::String& query,const juce::String& name,const juce::String& keywords={});
bool isSampleFavorite(const juce::StringArray&,const juce::File&);
juce::String sampleFavoritesToken(const juce::File&);
juce::Result loadSampleFavorites(const juce::File&,juce::StringArray& output);
juce::Result saveSampleFavorites(const juce::File&,const juce::StringArray&,const juce::String& expectedToken={});
juce::Result editSampleFavorite(const juce::File& catalog,const juce::File& audio,bool add,const juce::String& expectedToken={});
}
