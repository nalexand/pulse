// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Project.h"
namespace pulse {
juce::var auxSendsToJson(const AuxSends&);
juce::Result auxSendsFromJson(const juce::var&,AuxSends&);
struct RoutingAudibility {uint32_t inputs=0;std::array<uint8_t,maxTracks> edges{};};
// Bit 0 is the main output; bits 1-4 are auxiliary paths. Invalid graphs return silence.
RoutingAudibility routingAudibility(const Project&);
bool auxTopologyChanged(const Project&,const Project&);
}
