// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <juce_core/juce_core.h>
#include <vector>

namespace pulse {
constexpr int workspacePaneCount=15, maxWorkspacePresets=32;
constexpr juce::int64 maxWorkspaceLibraryBytes=256*1024;
struct WorkspaceWindow {
    float x=0,y=0,w=720,h=400;
    bool visible=false,maximized=false;
    bool operator==(const WorkspaceWindow&) const = default;
};
struct WorkspaceLayout {
    int width=1440,height=940,active=1;
    bool browser=true;
    float browserWidth=216;
    std::vector<WorkspaceWindow> windows;
    std::vector<int> order;
    bool operator==(const WorkspaceLayout&) const = default;
};
struct WorkspacePreset {
    juce::String id,name;
    WorkspaceLayout layout;
    bool operator==(const WorkspacePreset&) const = default;
};
using WorkspaceLibrary=std::vector<WorkspacePreset>;
juce::Result validateWorkspace(const WorkspaceLayout&);
juce::var workspaceToJson(const WorkspaceLayout&);
juce::Result workspaceFromJson(const juce::var&,WorkspaceLayout&);
juce::Result validateWorkspaceLibrary(const WorkspaceLibrary&);
juce::String workspaceLibraryToken(const juce::File&);
juce::Result loadWorkspaceLibrary(const juce::File&,WorkspaceLibrary&);
juce::Result putWorkspacePreset(const juce::File&,const WorkspacePreset&,bool replace,const juce::String& expected);
juce::Result removeWorkspacePreset(const juce::File&,const juce::String& id,const juce::String& expected);
juce::Result saveWorkspacePreset(const juce::File&,const WorkspacePreset&);
juce::Result loadWorkspacePreset(const juce::File&,WorkspacePreset&);
inline constexpr int interfaceScales[]={75,90,100,110,125,150,175,200};
bool validInterfaceScale(int);
juce::Result loadInterfaceScale(const juce::File&,int&);
juce::Result saveInterfaceScale(const juce::File&,int);
}
