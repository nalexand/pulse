// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Project.h"
namespace pulse {
struct ProjectCommandDraft { Project project; juce::var operations; juce::StringArray resolved; };
// A bounded, offline grammar. Produces ordinary validated domain edits only.
juce::Result buildProjectCommands(const Project&,const juce::String&,ProjectCommandDraft&);
juce::String projectCommandHelp();
}
