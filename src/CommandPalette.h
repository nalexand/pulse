// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <vector>
namespace pulse {
struct PaletteEntry {
    juce::String id,name,detail,keywords,shortcut;
    int action=0,data=0;
    bool enabled=true;
};
std::vector<int> searchCommands(const std::vector<PaletteEntry>&,const juce::String&);
class CommandPalette final : public juce::Component,private juce::ListBoxModel,private juce::TextEditor::Listener,private juce::KeyListener {
public:
    CommandPalette(std::vector<PaletteEntry>,std::function<void(juce::String)>);
    void paint(juce::Graphics&)override;
    void resized()override;
    void focusSearch();
    void setQuery(const juce::String&);
    std::vector<juce::String> matchingIds()const;
    void moveSelection(int);
    void confirm();
    void cancel();
    void inputAttemptWhenModal()override{cancel();}
private:
    int getNumRows()override{return(int)matches.size();}
    void paintListBoxItem(int,juce::Graphics&,int,int,bool)override;
    void listBoxItemDoubleClicked(int,const juce::MouseEvent&)override{confirm();}
    void textEditorTextChanged(juce::TextEditor&)override;
    bool keyPressed(const juce::KeyPress&,juce::Component*)override;
    void textEditorReturnKeyPressed(juce::TextEditor&)override{confirm();}
    void textEditorEscapeKeyPressed(juce::TextEditor&)override{cancel();}
    juce::TextEditor search;
    juce::ListBox list;
    std::vector<PaletteEntry> entries;
    std::vector<int> matches;
    std::function<void(juce::String)> finished;
};
}
