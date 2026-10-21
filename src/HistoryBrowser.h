// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "EditHistory.h"
#include <juce_gui_extra/juce_gui_extra.h>
namespace pulse {
class HistoryBrowser final:public juce::Component,private juce::ListBoxModel,private juce::KeyListener {
public:
    HistoryBrowser(std::vector<HistoryRow>,int current,std::function<void(int)>);
    ~HistoryBrowser()override;
    void paint(juce::Graphics&)override;
    void resized()override;
    void focusSearch();
    void setQuery(const juce::String&);
    void setFilter(int); // 0 All, 1 Undo, 2 Redo.
    std::vector<int> matchingStates()const;
    void selectState(int);
    int selectedState()const;
    void confirm();
    void cancel();
    void inputAttemptWhenModal()override{cancel();}
private:
    int getNumRows()override{return(int)matches.size();}
    void paintListBoxItem(int,juce::Graphics&,int,int,bool)override;
    void selectedRowsChanged(int)override;
    void listBoxItemDoubleClicked(int,const juce::MouseEvent&)override{confirm();}
    bool keyPressed(const juce::KeyPress&,juce::Component*)override;
    void refresh();
    juce::TextEditor search,details;
    juce::ComboBox filter;
    juce::ListBox list;
    juce::TextButton restore{"Restore selected state"},close{"Close"};
    std::vector<HistoryRow> rows;
    std::vector<int> matches;
    int currentState=0;
    std::function<void(int)> finished;
};
}
