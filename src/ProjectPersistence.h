// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Project.h"
namespace pulse {
enum class ProjectSaveStage {written,flushed,verified,backup,beforeCommit,committed};
// Optional in-process checkpoints for deterministic failure/crash verification.
// The desktop and MCP save paths never supply a checkpoint.
struct ProjectSaveControl {std::function<juce::Result(ProjectSaveStage,const juce::File&)> checkpoint;};
juce::File projectBackupFile(const juce::File&);
juce::String projectFileToken(const juce::File&);
juce::Result saveProjectChecked(const Project&,const juce::File&,const ProjectSaveControl* = nullptr);
juce::Result loadProjectWithBackup(const juce::File&,Project&,juce::File& used);
}
