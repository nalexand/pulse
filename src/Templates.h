// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Project.h"
#include <atomic>
namespace pulse {
enum class TemplateContent { complete,channelsAndMixer };
struct ProjectTemplate {
    juce::String id,name,description,category;
    juce::File directory;
    bool builtIn=false;
    TemplateContent content=TemplateContent::complete;
    double bpm=118;
    int tracks=0,patterns=0,clips=0,notes=0;
};
std::vector<ProjectTemplate> builtInProjectTemplates();
juce::Result loadProjectTemplate(const ProjectTemplate&,Project&);
juce::Result readProjectTemplate(const juce::File& directory,ProjectTemplate&);
std::vector<ProjectTemplate> scanProjectTemplates(const juce::File& library,juce::String& summary,const std::atomic<bool>* cancel=nullptr);
juce::Result saveProjectTemplate(const Project&,const juce::File& base,const juce::File& library,
                                 const juce::String& name,const juce::String& description,TemplateContent,ProjectTemplate& saved);
}
