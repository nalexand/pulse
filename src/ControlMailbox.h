// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <juce_core/juce_core.h>
#include <deque>
#include <set>
namespace pulse {
class ControlMailbox {
public:
    ~ControlMailbox(){stop();}
    juce::Result start(const juce::File& workspace);
    void stop();
    bool enabled()const{return sessionId.isNotEmpty();}
    void poll(const std::function<juce::Result(const juce::String&,const juce::var&,juce::var&)>&);
    const juce::File& directory()const{return folder;}
private:
    juce::File root,folder;
    juce::String sessionId,token;
    std::set<juce::String> seen;
    std::deque<juce::String> order;
};
}
