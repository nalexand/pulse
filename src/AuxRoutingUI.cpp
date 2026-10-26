// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include "AuxRouting.h"
#include <cstdlib>
#include <cmath>
namespace pulse {
using namespace juce;
namespace {
class AuxSendEditor final:public Component {
public:
    AuxSends initial;std::array<String,auxSendCount> initialText;
    std::array<ComboBox,auxSendCount> target;
    std::array<TextEditor,auxSendCount> level;
    std::array<ToggleButton,auxSendCount> pre;
    AuxSendEditor(const Project& project,int source):initial(project.tracks[(size_t)source].auxSends){
        for(int i=0;i<auxSendCount;++i){auto& box=target[(size_t)i];box.setComponentID("aux-target-"+String(i));box.setName("Send "+String(i+1)+" bus destination");level[(size_t)i].setName("Send "+String(i+1)+" level percent");pre[(size_t)i].setName("Send "+String(i+1)+" pre-fader tap");level[(size_t)i].setComponentID("aux-level-"+String(i));pre[(size_t)i].setComponentID("aux-pre-"+String(i));box.addItem("Off",1);for(int t=0;t<(int)project.tracks.size();++t)if(t!=source&&project.tracks[(size_t)t].isBus)box.addItem(String(t+1)+" - "+project.tracks[(size_t)t].name,t+2);const auto& s=project.tracks[(size_t)source].auxSends[(size_t)i];box.setSelectedId(s.target+2,dontSendNotification);initialText[(size_t)i]=String(s.level*100,3);level[(size_t)i].setText(initialText[(size_t)i]);pre[(size_t)i].setButtonText("Pre fader");pre[(size_t)i].setToggleState(s.preFader,dontSendNotification);addAndMakeVisible(box);addAndMakeVisible(level[(size_t)i]);addAndMakeVisible(pre[(size_t)i]);}setSize(440,188);
    }
    void paint(Graphics& g)override{g.setColour(Colour(0xffd9ddde));g.setFont(FontOptions(12));g.drawText("BUS DESTINATION",32,0,205,24,Justification::centredLeft);g.drawText("LEVEL %",244,0,80,24,Justification::centredLeft);g.drawText("TAP",342,0,80,24,Justification::centredLeft);for(int i=0;i<auxSendCount;++i)g.drawText(String(i+1),0,30+i*38,26,27,Justification::centred);}
    void resized()override{for(int i=0;i<auxSendCount;++i){int y=30+i*38;target[(size_t)i].setBounds(32,y,202,27);level[(size_t)i].setBounds(244,y,80,27);pre[(size_t)i].setBounds(338,y,100,27);}}
    Result read(AuxSends& out)const{AuxSends next;for(int i=0;i<auxSendCount;++i){auto text=level[(size_t)i].getText().trim();auto* first=text.toRawUTF8();char* last=nullptr;double amount=std::strtod(first,&last);if(last==first||*last||!std::isfinite(amount)||amount<0||amount>150||target[(size_t)i].getSelectedId()==0)return Result::fail("Each send needs an existing bus or Off, and a numeric level from 0 to 150%.");next[(size_t)i]={target[(size_t)i].getSelectedId()-2,text==initialText[(size_t)i]?initial[(size_t)i].level:(float)(amount/100),pre[(size_t)i].getToggleState()};}out=next;return Result::ok();}
};
class AuxSendDialog final:public AlertWindow {
public:
    AuxSendEditor editor;
    AuxSendDialog(const Project& p,int track,Component* owner):AlertWindow("Auxiliary sends - "+p.tracks[(size_t)track].name,"Four parallel bus paths in addition to the main output. All sends are after inserts and pan. Pre fader ignores channel volume; Mute closes both taps. Bus processing is included. Add buses in Mixer first. Feedback cycles are rejected, including zero-level routes.",MessageBoxIconType::NoIcon,owner),editor(p,track){addCustomComponent(&editor);addButton("Apply sends",1,KeyPress(KeyPress::returnKey));addButton("Cancel",0,KeyPress(KeyPress::escapeKey));}
};
}
void Studio::showAuxSends(){
    if(editingMasterEffects()){status="Auxiliary sends belong to channels and buses. Select one in Mixer first.";return;}
    if(recording||recorder.isActive()||pendingNoteTool()||automationWriteContext||exporting.load()){status="Finish the take, automation pass, preview or export before editing sends.";return;}
    int track=selectedTrack;auto expected=revision;auto* dialog=new AuxSendDialog(project,track,this);auto safe=Component::SafePointer<Studio>(this);
    dialog->enterModalState(true,ModalCallbackFunction::create([safe,dialog,track,expected](int result){if(!safe||result!=1)return;AuxSends sends;auto valid=dialog->editor.read(sends);if(valid.failed()){safe->status=valid.getErrorMessage();return;}safe->setAuxSends(track,expected,sends);}),true);
}
bool Studio::setAuxSends(int track,uint64_t expected,const AuxSends& sends){
    if(expected!=revision||track<0||track>=(int)project.tracks.size()||track!=selectedTrack||recording||recorder.isActive()||pendingNoteTool()||automationWriteContext||exporting.load()){status="The target changed or the app is busy. Reopen Auxiliary sends.";return false;}
    auto next=project;next.tracks[(size_t)track].auxSends=sends;auto valid=validateRouting(next);if(valid.failed()){status=valid.getErrorMessage();return false;}
    if(project.tracks[(size_t)track].auxSends==sends){status="Auxiliary sends are unchanged.";return true;}
    auto playback=std::make_unique<Playback>();String warnings;auto base=currentFile==File()?root:currentFile.getParentDirectory();valid=compilePlayback(next,assets,base,*playback,warnings);if(valid.failed()||warnings.isNotEmpty()){status="Sends cancelled: "+(valid.failed()?valid.getErrorMessage():warnings);return false;}
    bool topology=auxTopologyChanged(project,next);double at=engine->seekRequested.load();if(at<0)at=engine->position.load();checkpoint();project=std::move(next);changed();if(topology){releaseKeys();engine->seekRequested.store(jlimit(0.,project.songMode?project.songLength():project.patterns[(size_t)project.activePattern].length,at));}
    status="Auxiliary sends applied as one Undo. Level changes are smoothed; routing/tap changes refresh playback.";repaint();return true;
}
void Studio::paintAuxLevels(Graphics& g,Rectangle<float> r){
    const Colour text(0xffd9ddde),muted(0xffadb7bd);const auto& tr=project.tracks[(size_t)selectedTrack];
    g.setColour(Colour(0xff3b464c));g.fillRect(r);float x=r.getX()+16,w=r.getWidth()-32,y=r.getY()+8;
    label(g,tr.name+" / Auxiliary send levels",{x,y,w-202,25},13,text,true);
    button(g,"Routing / taps...",{r.getRight()-192,y,174,25},auxSendsOpen);
    for(int slot=0;slot<auxSendCount;++slot){const auto& send=tr.auxSends[(size_t)slot];int action=auxLevel1+slot;float normal=liveParameter(action,selectedTrack,send.level/1.5f);float pitch=jmin(66.f,jmax(34.f,(r.getHeight()-151)/4));float row=y+44+slot*pitch;
        String destination=send.target<0?String("Off"):project.tracks[(size_t)send.target].name;
        dial(g,"Send "+String(slot+1)+" - "+destination+"  "+String(normal*150,1)+"%",{x,row,w-124,40},action,normal,selectedTrack);
        label(g,send.preFader?"PRE FADER":"POST FADER",{r.getRight()-128,row+7,108,25},10,muted);
    }
    label(g,"Right-click: actual level or automation. Shift-right-click: create curve. Drag/wheel: level.",{x,r.getBottom()-96,w,22},11,text);
    label(g,"Song automation and Touch / Latch / Write / Trim use these knobs. Off routes remain silent.",{x,r.getBottom()-72,w,22},10,muted);
    button(g,"Insert effects",{x,r.getBottom()-42,138,26},effectsOpen,0);
}

}
