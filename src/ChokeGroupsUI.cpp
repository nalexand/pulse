// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
namespace pulse {
using namespace juce;
bool Studio::setChokeGroup(int track,int group,uint64_t expected){
    if(track<0||track>=(int)project.tracks.size()||project.tracks[(size_t)track].isBus||group<0||group>16||recording||recorder.isActive()||automationWriteContext||pendingNoteTool()||revision!=expected){status="Finish the take/preview and reopen Choke group after changes. Choose a source channel and group 0-16.";return false;}
    auto& channel=project.tracks[(size_t)track];if(channel.chokeGroup==group){status="Choke group is unchanged.";return true;}
    checkpoint();channel.chokeGroup=group;changed();status=group?"Choke group "+String(group)+": a new accepted note closes older group voices with a 5 ms fade. Effect tails remain.":"Choke group off. Other channel settings are retained.";return true;
}
void Studio::showChokeGroup(){
    int track=selectedTrack;if(track<0||track>=(int)project.tracks.size()||project.tracks[(size_t)track].isBus||recording||recorder.isActive()||automationWriteContext||pendingNoteTool()){status="Choose a source and finish the take/preview before changing its choke group.";return;}
    const auto expected=revision;PopupMenu menu;menu.addSectionHeader(project.tracks[(size_t)track].name+" / Choke group");menu.addItem(1,"Off — independent voices",true,project.tracks[(size_t)track].chokeGroup==0);
    for(int group=1;group<=16;++group){StringArray names;for(int i=0;i<(int)project.tracks.size();++i)if(i!=track&&!project.tracks[(size_t)i].isBus&&project.tracks[(size_t)i].chokeGroup==group)names.add(project.tracks[(size_t)i].name);menu.addItem(group+1,"Group "+String(group)+(names.isEmpty()?String():" — "+names.joinIntoString(", ").substring(0,60)),true,project.tracks[(size_t)track].chokeGroup==group);}
    auto safe=Component::SafePointer<Studio>(this);menu.showMenuAsync(menuOptions(chokeGroupOpen),[safe,track,expected](int result){if(safe&&result>0)safe->setChokeGroup(track,result-1,expected);});
}
}
