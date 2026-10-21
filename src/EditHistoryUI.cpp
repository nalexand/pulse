// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
namespace pulse {
using namespace juce;
void Studio::showEditHistory(){
    if(auto* modal=Component::getCurrentlyModalComponent()){if(historyBrowser&&modal==historyBrowser.get())historyBrowser->focusSearch();return;}
    if(recording||recorder.isActive()||pendingNoteTool()||exporting.load()){status="Finish the take, note preview or export before opening edit history.";return;}
    releaseKeys();auto stamp=revision,generation=historyGeneration;auto safe=Component::SafePointer<Studio>(this);
    auto current=absoluteAssets(project,currentFile==File()?root:currentFile.getParentDirectory());
    historyBrowser=std::make_unique<HistoryBrowser>(projectHistoryRows(current,undoStack,redoStack),(int)undoStack.size(),[safe,stamp,generation](int state){if(!safe)return;safe->closeEditHistory();if(state>=0)safe->restoreHistoryState(state,stamp,generation);});
    addAndMakeVisible(*historyBrowser);positionEditHistory();historyBrowser->enterModalState(true,nullptr,false);historyBrowser->focusSearch();
}
void Studio::positionEditHistory(){if(historyBrowser){int width=jmin(820,getWidth()-32),height=jmin(650,getHeight()-40);historyBrowser->setBounds((getWidth()-width)/2,(getHeight()-height)/2,width,height);}}
void Studio::closeEditHistory(){if(!historyBrowser)return;historyBrowser->exitModalState(0);historyBrowser->setVisible(false);grabKeyboardFocus();repaint();}
bool Studio::restoreHistoryState(int state,uint64_t expected,uint64_t generation){
    if(recording||recorder.isActive()||pendingNoteTool()||exporting.load()){status="Finish the take, note preview or export before restoring history.";return false;}
    if(revision!=expected||historyGeneration!=generation){status="Project history changed. Reopen edit history before restoring a state.";return false;}
    if(state<0||state>(int)(undoStack.size()+redoStack.size())){status="History state is no longer available.";return false;}
    int distance=state-(int)undoStack.size();if(distance==0)return true;
    releaseKeys();project=absoluteAssets(project,currentFile==File()?root:currentFile.getParentDirectory());
    auto result=moveProjectHistory(project,undoStack,redoStack,state);if(result.failed()){status=result.getErrorMessage();return false;}
    ++historyGeneration;engine->resetRequested.store(true);selectedTrack=jlimit(0,(int)project.tracks.size()-1,selectedTrack);trackScroll=jlimit(0,jmax(0,(int)project.tracks.size()-8),trackScroll);selectedNote=-1;selectedNotes.clear();selectedClip=-1;selectedPoint=-1;pianoLow=-1;changed();
    status=String(distance<0?"Undid ":"Redid ")+String(std::abs(distance))+" "+String(std::abs(distance)==1?"edit":"edits")+". Remaining history is available in Edit > Project edit history.";return true;
}
}
