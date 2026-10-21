// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Project.h"
namespace pulse {
// UI positions in source-pattern beats. Overlapping instances may have different phases.
std::vector<double> patternPlaybackPositions(const Project&,int track,double transportBeat);
}
