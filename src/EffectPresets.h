// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Project.h"
namespace pulse {
struct EffectChainPreset {juce::String name;Track::Mixer settings;};
EffectChainPreset captureEffectChain(const juce::String&,const Track::Mixer&);
const std::vector<EffectChainPreset>& factoryEffectChains();
juce::Result validateEffectChain(const EffectChainPreset&);
juce::var effectChainToJson(const EffectChainPreset&);
juce::Result effectChainFromJson(const juce::var&,EffectChainPreset&);
juce::Result saveEffectChain(const EffectChainPreset&,const juce::File&);
juce::Result loadEffectChain(const juce::File&,EffectChainPreset&);
}
