// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <juce_core/juce_core.h>
#include <map>
#include <vector>
namespace pulse {
struct ShortcutCommand {juce::String id,name,group,defaultKey;bool typingOff=false,locked=false;};
struct ShortcutSettings {std::map<juce::String,juce::String> overrides;bool operator==(const ShortcutSettings&)const=default;};
const std::vector<ShortcutCommand>& shortcutCommands();
const ShortcutCommand* shortcutCommand(const juce::String&);
juce::String shortcutBinding(const ShortcutSettings&,const juce::String&);
juce::Result canonicalShortcut(const juce::String&,juce::String&);
juce::Result validateShortcuts(const ShortcutSettings&);
juce::Result setShortcut(ShortcutSettings&,const juce::String& id,const juce::String& chord);
juce::String findShortcut(const ShortcutSettings&,const juce::String& canonical,bool typingEnabled);
juce::String shortcutFileToken(const juce::File&);
juce::Result loadShortcuts(const juce::File&,ShortcutSettings&);
juce::Result saveShortcuts(const juce::File&,const ShortcutSettings&);
}
