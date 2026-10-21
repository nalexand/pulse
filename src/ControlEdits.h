// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Project.h"
namespace pulse {
// Atomic model edits shared by the live MCP bridge and regression checks.
juce::Result applyControlEdits(const Project&,const juce::var& operations,Project& output,juce::StringArray* resolvedCommands=nullptr);
}
