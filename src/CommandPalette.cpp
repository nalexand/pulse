// SPDX-License-Identifier: AGPL-3.0-only
#include "CommandPalette.h"
#include <algorithm>
namespace pulse {
using namespace juce;
std::vector<int> searchCommands(const std::vector<PaletteEntry>& entries,const String& query){
    auto tokens=StringArray::fromTokens(query.trim().toLowerCase()," \t\r\n","");tokens.removeEmptyStrings();
    struct Match{int index,score;};std::vector<Match> ranked;
    for(int i=0;i<(int)entries.size();++i){auto& e=entries[(size_t)i];auto name=e.name.toLowerCase(),all=(e.name+" "+e.detail+" "+e.keywords+" "+e.shortcut).toLowerCase();int score=0;bool found=true;
        for(auto& token:tokens){if(!all.contains(token)){found=false;break;}score+=name==token?100:name.startsWith(token)?40:name.contains(token)?20:1;}
        if(found)ranked.push_back({i,score});
    }
    std::stable_sort(ranked.begin(),ranked.end(),[](const Match& a,const Match& b){return a.score>b.score;});std::vector<int> result;for(auto m:ranked)result.push_back(m.index);return result;
}
CommandPalette::CommandPalette(std::vector<PaletteEntry> values,std::function<void(String)> callback):entries(std::move(values)),finished(std::move(callback)){
    setOpaque(true);setWantsKeyboardFocus(true);search.setTextToShowWhenEmpty("Search commands, channels, patterns, clips, instruments or help...",Colour(0xff9aabad));
    search.setColour(TextEditor::backgroundColourId,Colour(0xff253035));search.setColour(TextEditor::textColourId,Colour(0xffe0e5df));search.setColour(TextEditor::outlineColourId,Colour(0xff839775));search.setFont(FontOptions("Segoe UI",15,Font::plain));search.setSelectAllWhenFocused(true);search.setMultiLine(false);search.addListener(this);search.addKeyListener(this);addAndMakeVisible(search);
    list.setModel(this);list.setRowHeight(56);list.setColour(ListBox::backgroundColourId,Colour(0xff2e383d));list.addKeyListener(this);addAndMakeVisible(list);textEditorTextChanged(search);
}
void CommandPalette::focusSearch(){search.grabKeyboardFocus();search.selectAll();}
void CommandPalette::setQuery(const String& query){search.setText(query,false);textEditorTextChanged(search);}
std::vector<String> CommandPalette::matchingIds()const{std::vector<String> result;for(int i:matches)result.push_back(entries[(size_t)i].id);return result;}
void CommandPalette::textEditorTextChanged(TextEditor&){matches=searchCommands(entries,search.getText());list.updateContent();list.deselectAllRows();if(!matches.empty())list.selectRow(0);repaint();}
void CommandPalette::moveSelection(int amount){if(!matches.empty())list.selectRow(jlimit(0,(int)matches.size()-1,list.getSelectedRow()+amount));}
void CommandPalette::confirm(){int row=list.getSelectedRow();if(row<0||row>=(int)matches.size())return;const auto& entry=entries[(size_t)matches[(size_t)row]];if(!entry.enabled)return;if(finished)finished(entry.id);}
void CommandPalette::cancel(){if(finished)finished({});}
bool CommandPalette::keyPressed(const KeyPress& key,Component*){if(key==KeyPress::upKey||key==KeyPress::downKey){moveSelection(key==KeyPress::upKey?-1:1);return true;}if(key==KeyPress::pageUpKey||key==KeyPress::pageDownKey){moveSelection(key==KeyPress::pageUpKey?-6:6);return true;}if(key==KeyPress::returnKey){confirm();return true;}if(key==KeyPress::escapeKey){cancel();return true;}return false;}
void CommandPalette::resized(){search.setBounds(20,52,getWidth()-40,36);list.setBounds(12,102,getWidth()-24,getHeight()-148);}
void CommandPalette::paint(Graphics& g){g.fillAll(Colour(0xff394349));g.setColour(Colour(0xffa6b88d));g.drawRect(getLocalBounds(),2);g.setFont(FontOptions("Segoe UI",17,Font::bold));g.setColour(Colour(0xffe2e7e0));g.drawText("Find a command",20,12,getWidth()-40,30,Justification::centredLeft);
    g.setFont(FontOptions("Segoe UI",11,Font::plain));g.setColour(Colour(0xffafbdc0));g.drawText(String((int)matches.size())+" results  |  Up/Down select  |  Enter runs  |  Esc closes",20,getHeight()-37,getWidth()-40,24,Justification::centredLeft);
    if(matches.empty()){g.setFont(FontOptions("Segoe UI",14,Font::plain));g.drawText("No matching commands. Try a shorter name or an instrument.",list.getBounds().reduced(15),Justification::centred);}
}
void CommandPalette::paintListBoxItem(int row,Graphics& g,int width,int height,bool selected){if(row<0||row>=(int)matches.size())return;auto& entry=entries[(size_t)matches[(size_t)row]];if(selected){g.setColour(Colour(0xff56664b));g.fillRect(0,0,width,height);}g.setColour(entry.enabled?Colour(0xffe1e7df):Colour(0xff8d999b));g.setFont(FontOptions("Segoe UI",14,Font::plain));g.drawText(entry.name,13,4,width-155,25,Justification::centredLeft);g.setFont(FontOptions("Segoe UI",11,Font::plain));g.drawText(entry.shortcut,width-140,4,126,25,Justification::centredRight);g.setColour(Colour(0xffaebabb));g.drawText(entry.detail,13,29,width-26,22,Justification::centredLeft);g.setColour(Colour(0xff3b474b));g.drawHorizontalLine(height-1,10.f,(float)width-10);}
}
