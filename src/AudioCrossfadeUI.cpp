// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
namespace pulse {
using namespace juce;
namespace {
const StringArray fadeNames{"Linear", "Equal power", "S-curve"};
class CrossfadeReview final:public AlertWindow {
public:
    CrossfadeReview(const String& details,FadeCurve initial,Component* parent):AlertWindow("Crossfade selected audio clips","Fit fades to the existing overlaps. Clip positions and outer fade ends stay unchanged.",MessageBoxIconType::NoIcon,parent){
        addComboBox("curve",fadeNames,"Crossfade curve");getComboBoxComponent("curve")->setSelectedItemIndex((int)initial,dontSendNotification);
        information.setComponentID("crossfade-details");information.setMultiLine(true,true);information.setReadOnly(true);information.setScrollbarsShown(true);information.setCaretVisible(false);information.setText(details,false);information.setSize(400,96);addCustomComponent(&information);
        addButton("Apply crossfades",1,KeyPress(KeyPress::returnKey));addButton("Cancel",0,KeyPress(KeyPress::escapeKey));
    }
private:TextEditor information;
};
}

void Studio::showPlaylistAudioTools(){
    auto selection=playlistSelection();bool audio=!selection.empty();for(int i:selection)audio&=project.clips[(size_t)i].isAudio();
    PopupMenu menu;menu.addItem(importAudioClip,"Import audio clip...");menu.addItem(audioFadeCurvesOpen,"Audio fade curves...",selectedClip>=0&&selectedClip<(int)project.clips.size()&&project.clips[(size_t)selectedClip].isAudio());menu.addItem(audioClipSettings,"Audio clip settings...",selectedClip>=0&&selectedClip<(int)project.clips.size()&&project.clips[(size_t)selectedClip].isAudio());menu.addSeparator();menu.addItem(audioCrossfadeOpen,"Crossfade selected audio clips...",audio&&selection.size()>=2);menu.addItem(audioFadesClear,"Clear selected audio fades",audio);auto safe=Component::SafePointer<Studio>(this);auto rev=revision;auto ids=selectedPlaylistIds;int primary=selectedClip;
    menu.showMenuAsync(menuOptions(importAudioClip),[safe,rev,ids,primary](int action){if(!safe||!action)return;if(rev!=safe->revision||ids!=safe->selectedPlaylistIds||primary!=safe->selectedClip){safe->status="The project or clip selection changed. Reopen audio tools.";return;}safe->perform(action);});
}
void Studio::showAudioCrossfades(){
    if(recording||recorder.isActive()||pendingNoteTool()){status="Finish the take and note preview before crossfading audio.";return;}
    auto selection=playlistSelection();Project next;AudioCrossfadeStats stats;auto valid=crossfadeSelectedAudio(project,selection,false,next,&stats);if(valid.failed()){status=valid.getErrorMessage();return;}
    setPlaylistSelection(selection,selectedClip);auto ids=selectedPlaylistIds;auto rev=revision;String details=String((int)stats.pairs.size())+(stats.pairs.size()==1?" transition. ":" transitions. ")+"Fade lengths match the existing overlaps. No clips move; source audio stays intact. Other fade ends are retained.\n";
    for(size_t i=0;i<jmin((size_t)6,stats.pairs.size());++i){const auto& pair=stats.pairs[i];details+="\nLane "+String(project.clips[(size_t)pair.outgoing].track+1)+": clip "+String(pair.outgoing+1)+" -> "+String(pair.incoming+1)+", bars "+String(pair.start/4+1,3)+" - "+String(pair.end/4+1,3)+" ("+String(pair.end-pair.start,3)+" beats)";}
    if(stats.pairs.size()>6)details+="\n...and "+String((int)stats.pairs.size()-6)+" more transitions.";details+="\n\nLinear and S-curve use constant-sum transitions. Equal power suits unrelated sources and can boost matching material. Shape changes become one Undo step.";
    auto initial=project.clips[(size_t)stats.pairs[0].outgoing].fadeOutCurve;auto* dialog=new CrossfadeReview(details,initial,this);auto safe=Component::SafePointer<Studio>(this);
    dialog->enterModalState(true,ModalCallbackFunction::create([safe,dialog,ids,rev](int result){if(safe&&result==1){auto* box=dialog->getComboBoxComponent("curve");if(box)safe->applyAudioCrossfades(ids,rev,false,(FadeCurve)box->getSelectedItemIndex());}}),true);
}
bool Studio::applyAudioCrossfades(const std::set<String>& ids,uint64_t rev,bool clear,FadeCurve curve){
    if(recording||recorder.isActive()||pendingNoteTool()){status="Finish the take and note preview before editing audio fades.";return false;}
    if(rev!=revision||ids.empty()||ids!=selectedPlaylistIds){status="The project or audio selection changed. Select clips and reopen the fade tool.";return false;}
    auto selection=playlistSelection();Project next;AudioCrossfadeStats stats;auto valid=crossfadeSelectedAudio(project,selection,clear,next,&stats,curve);if(valid.failed()){status=valid.getErrorMessage();return false;}
    if(stats.changed.empty()){status=clear?"Selected audio clips already have no fades.":"Selected transitions already match this fade shape.";return true;}
    if(!clear){auto base=currentFile==File()?root:currentFile.getParentDirectory();for(int i:selection){const auto& clip=project.clips[(size_t)i];String error;auto* sample=assets.load(File::isAbsolutePath(clip.audioPath)?File(clip.audioPath):base.getChildFile(clip.audioPath),error);if(!sample){status="Crossfade cancelled: "+error;return false;}if(clip.sourceOffset+clip.length*60/project.bpm>sample->audio.getNumSamples()/sample->sampleRate+1/sample->sampleRate){status="Crossfade cancelled: selected clip "+String(i+1)+" extends beyond its source audio. Trim it first.";return false;}}}
    return commitPlaylistGroup(std::move(next),std::vector<int>(selection.begin(),selection.end()),selectedClip,clear?String((int)stats.changed.size())+" selected audio clips have no fades.":String((int)stats.pairs.size())+" "+fadeNames[(int)curve]+" audio crossfades applied to "+String((int)stats.changed.size())+" clips.");
}
void Studio::editAudioFadeCurves(){
    if(recording||recorder.isActive()||pendingNoteTool()||selectedClip<0||selectedClip>=(int)project.clips.size()||!project.clips[(size_t)selectedClip].isAudio()){status="Select an audio clip and finish the take or note preview first.";return;}
    int index=selectedClip;const auto& clip=project.clips[(size_t)index];auto* dialog=new AlertWindow("Audio fade curves","Shapes apply independently to both ends. Set fade lengths in Audio clip settings.",MessageBoxIconType::NoIcon,this);dialog->addComboBox("inCurve",fadeNames,"Fade-in shape");dialog->addComboBox("outCurve",fadeNames,"Fade-out shape");dialog->getComboBoxComponent("inCurve")->setSelectedItemIndex((int)clip.fadeInCurve,dontSendNotification);dialog->getComboBoxComponent("outCurve")->setSelectedItemIndex((int)clip.fadeOutCurve,dontSendNotification);dialog->addButton("Apply curves",1,KeyPress(KeyPress::returnKey));dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));auto safe=Component::SafePointer<Studio>(this);auto expected=revision;
    dialog->enterModalState(true,ModalCallbackFunction::create([safe,dialog,index,expected](int result){if(!safe||result!=1)return;auto* a=dialog->getComboBoxComponent("inCurve");auto* b=dialog->getComboBoxComponent("outCurve");if(a&&b)safe->applyAudioFadeCurves(index,expected,(FadeCurve)a->getSelectedItemIndex(),(FadeCurve)b->getSelectedItemIndex());}),true);
}
bool Studio::applyAudioFadeCurves(int index,uint64_t expected,FadeCurve in,FadeCurve out){
    if(recording||recorder.isActive()||pendingNoteTool()||expected!=revision||index!=selectedClip||index<0||index>=(int)project.clips.size()||!project.clips[(size_t)index].isAudio()||!validFadeCurve(in)||!validFadeCurve(out)){status="Audio clip or preview changed. Reopen fade curves with a current audio selection.";return false;}
    auto next=project;auto& clip=next.clips[(size_t)index];if(clip.fadeInCurve==in&&clip.fadeOutCurve==out){status="Audio clip already uses these fade shapes.";return true;}clip.fadeInCurve=in;clip.fadeOutCurve=out;auto selection=playlistSelection();if(selection.empty())selection.insert(index);return commitPlaylistGroup(std::move(next),std::vector<int>(selection.begin(),selection.end()),index,"Audio fade shapes applied. Fade lengths and source audio retained.");
}

}

