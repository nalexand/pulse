// SPDX-License-Identifier: AGPL-3.0-only
#include "HistoryBrowser.h"
namespace pulse {
using namespace juce;
HistoryBrowser::HistoryBrowser(std::vector<HistoryRow> entries,int current,std::function<void(int)> done):rows(std::move(entries)),currentState(current),finished(std::move(done)){
    setOpaque(true);setWantsKeyboardFocus(true);
    search.setComponentID("history-search");search.setTextToShowWhenEmpty("Find notes, pattern, channel, sound, mixer, date...",Colour(0xffa6b3b6));search.setMultiLine(false);search.setSelectAllWhenFocused(true);search.onTextChange=[this]{refresh();};
    filter.setComponentID("history-filter");filter.addItem("All states",1);filter.addItem("Undo states",2);filter.addItem("Redo states",3);filter.setSelectedId(1,dontSendNotification);filter.onChange=[this]{refresh();};
    list.setModel(this);list.setRowHeight(52);list.setColour(ListBox::backgroundColourId,Colour(0xff2e383d));
    details.setComponentID("history-details");details.setReadOnly(true);details.setMultiLine(true);details.setScrollbarsShown(true);details.setCaretVisible(false);details.setFont(FontOptions(12));
    restore.setComponentID("history-restore");restore.onClick=[this]{confirm();};close.onClick=[this]{cancel();};
    for(auto* component:std::initializer_list<Component*>{&search,&filter,&list,&details,&restore,&close}){addAndMakeVisible(component);component->addKeyListener(this);}
    search.setColour(TextEditor::backgroundColourId,Colour(0xff253035));search.setColour(TextEditor::textColourId,Colour(0xffe0e5df));details.setColour(TextEditor::backgroundColourId,Colour(0xff253035));details.setColour(TextEditor::textColourId,Colour(0xffd9dfd5));refresh();
}
HistoryBrowser::~HistoryBrowser(){list.setModel(nullptr);}
void HistoryBrowser::focusSearch(){search.grabKeyboardFocus();}
void HistoryBrowser::setQuery(const String& text){search.setText(text,false);refresh();}
void HistoryBrowser::setFilter(int value){filter.setSelectedId(jlimit(0,2,value)+1,dontSendNotification);refresh();}
std::vector<int> HistoryBrowser::matchingStates()const{std::vector<int> result;for(int i:matches)result.push_back(rows[(size_t)i].state);return result;}
int HistoryBrowser::selectedState()const{int row=list.getSelectedRow();return row>=0&&row<(int)matches.size()?rows[(size_t)matches[(size_t)row]].state:-1;}
void HistoryBrowser::selectState(int state){for(int i=0;i<(int)matches.size();++i)if(rows[(size_t)matches[(size_t)i]].state==state){list.selectRow(i);list.scrollToEnsureRowIsOnscreen(i);return;}}
void HistoryBrowser::refresh(){
    int previous=selectedState();auto words=StringArray::fromTokens(search.getText().trim().toLowerCase()," \t\r\n","");words.removeEmptyStrings();matches.clear();
    for(int i=(int)rows.size()-1;i>=0;--i){auto& row=rows[(size_t)i];if(filter.getSelectedId()==2&&row.state>=currentState)continue;if(filter.getSelectedId()==3&&row.state<=currentState)continue;
        auto text=(row.summary+" "+row.detail+" "+(row.time>0?Time(row.time).formatted("%Y-%m-%d %H:%M:%S"):String())+" state "+String(row.state)+(row.state==currentState?" current":row.state<currentState?" undo":" redo")).toLowerCase();bool match=true;for(const auto& word:words)match&=text.contains(word);if(match)matches.push_back(i);
    }
    list.deselectAllRows();list.updateContent();selectState(previous>=0?previous:currentState);if(list.getSelectedRow()<0&&!matches.empty())list.selectRow(0);selectedRowsChanged(list.getSelectedRow());repaint();
}
void HistoryBrowser::selectedRowsChanged(int row){
    int state=selectedState();restore.setEnabled(state>=0&&state!=currentState);
    if(row<0||row>=(int)matches.size())details.setText("No matching states. Try a shorter name or All states.",false);
    else {auto& entry=rows[(size_t)matches[(size_t)row]];int distance=entry.state-currentState;String heading="State "+String(entry.state)+(distance==0?" / current":distance<0?" / Undo "+String(-distance)+" edits":" / Redo "+String(distance)+" edits");details.setText(heading+"\n"+entry.detail,false);}
}
void HistoryBrowser::confirm(){int state=selectedState();if(state>=0&&state!=currentState&&finished)finished(state);}
void HistoryBrowser::cancel(){if(finished)finished(-1);}
bool HistoryBrowser::keyPressed(const KeyPress& key,Component*){
    if(key==KeyPress::escapeKey){cancel();return true;}if(key==KeyPress::returnKey){confirm();return true;}
    if(key==KeyPress::upKey||key==KeyPress::downKey||key==KeyPress::pageUpKey||key==KeyPress::pageDownKey){int delta=key==KeyPress::upKey?-1:key==KeyPress::downKey?1:key==KeyPress::pageUpKey?-5:5;if(!matches.empty())list.selectRow(jlimit(0,(int)matches.size()-1,list.getSelectedRow()+delta));return true;}return false;
}
void HistoryBrowser::resized(){
    int w=getWidth(),h=getHeight();search.setBounds(16,49,jmax(100,w-190),32);filter.setBounds(w-164,49,148,32);
    int detailHeight=h<450?66:100;int listBottom=h-detailHeight-102;list.setBounds(12,92,w-24,jmax(48,listBottom-92));details.setBounds(16,listBottom+8,w-32,detailHeight);
    restore.setBounds(16,h-47,jmin(210,w-126),31);close.setBounds(w-100,h-47,84,31);if(list.getSelectedRow()>=0)list.scrollToEnsureRowIsOnscreen(list.getSelectedRow());
}
void HistoryBrowser::paint(Graphics& g){
    g.fillAll(Colour(0xff394349));g.setColour(Colour(0xffa6b88d));g.drawRect(getLocalBounds(),2);g.setColour(Colour(0xffe2e7e0));g.setFont(FontOptions("Segoe UI",17,Font::bold));g.drawText("Project edit history",16,10,getWidth()-32,27,Justification::centredLeft);
    g.setFont(FontOptions(11));g.setColour(Colour(0xffb9c5c5));g.drawText(String((int)matches.size())+" states / session only / up to 100 edits / Enter restores / Esc closes",16,getHeight()-79,getWidth()-32,18,Justification::centredLeft);
    if(matches.empty()){g.setFont(FontOptions(13));g.drawText("No matching edit states",list.getBounds().reduced(8),Justification::centred);}
}
void HistoryBrowser::paintListBoxItem(int row,Graphics& g,int width,int height,bool selected){
    if(row<0||row>=(int)matches.size())return;auto& entry=rows[(size_t)matches[(size_t)row]];
    if(selected){g.setColour(Colour(0xff56664b));g.fillRect(0,0,width,height);}g.setFont(FontOptions(13));g.setColour(Colour(0xffe1e7df));g.drawText(entry.summary,12,3,width-112,23,Justification::centredLeft);
    String badge=entry.state==currentState?"CURRENT":entry.state<currentState?"UNDO":"REDO";g.setColour(entry.state==currentState?Colour(0xffd2dda6):Colour(0xffbbc7c9));g.setFont(FontOptions(11));g.drawText(badge,width-93,3,80,23,Justification::centredRight);
    auto date=entry.time>0?Time(entry.time).formatted("%Y-%m-%d %H:%M:%S"):"Before first retained edit";g.setColour(Colour(0xffaebabb));g.drawText("State "+String(entry.state)+" / "+date,12,27,width-24,19,Justification::centredLeft);g.setColour(Colour(0xff3b474b));g.drawHorizontalLine(height-1,8.f,(float)width-8);
}
}
