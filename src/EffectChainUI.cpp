// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <algorithm>
namespace pulse {
using namespace juce;
bool Studio::applyEffectChain(const EffectChainPreset& preset,int track){
    if(recording||recorder.isActive()||pendingNoteTool()){status="Finish the take and resolve the note preview before replacing effects.";return false;}
    if(track<0||track>=(int)project.tracks.size()){status="Select a Mixer insert first.";return false;}auto valid=validateEffectChain(preset);if(valid.failed()){status=valid.getErrorMessage();return false;}
    auto next=preset.settings;auto& destination=project.tracks[(size_t)track].mixer;next.output=destination.output;next.sidechain=destination.sidechain;next.stereo=destination.stereo;
    auto current=captureEffectChain(preset.name,destination);if(JSON::toString(effectChainToJson(current))==JSON::toString(effectChainToJson(preset))){status="This effect chain is already loaded.";return true;}
    checkpoint();destination=next;changed();selectedTrack=track;showPane(effectsPane);status="Loaded effect chain: "+preset.name+". Instrument, notes, levels, routing and sidechain source are preserved.";return true;
}
bool Studio::moveEffect(int effect,int delta){
    if(recording||recorder.isActive()||pendingNoteTool()){status="Finish the take and resolve the note preview before reordering effects.";return false;}
    if(effect<0||effect>=builtinEffectCount||delta==0)return false;auto order=project.tracks[(size_t)selectedTrack].mixer.order;auto valid=validateEffectOrder(order);if(valid.failed()){status=valid.getErrorMessage();return false;}int from=(int)(std::find(order.begin(),order.end(),effect)-order.begin()),to=jlimit(0,builtinEffectCount-1,from+delta);if(from==to)return false;
    if(to<from)std::rotate(order.begin()+to,order.begin()+from,order.begin()+from+1);else std::rotate(order.begin()+from,order.begin()+from+1,order.begin()+to+1);
    checkpoint();project.tracks[(size_t)selectedTrack].mixer.order=order;selectedEffect=effect;changed();status=effectName(effect)+" moved to slot "+String(to+1)+". Its settings and bypass state are retained.";return true;
}
bool Studio::invokeEffectChainMenu(int result,int effect,int track,uint64_t sourceRevision){
    if(result==0)return false;if(recording||recorder.isActive()||revision!=sourceRevision||selectedTrack!=track){status="Channel or effects changed. Reopen the chain menu.";return false;}
    if(result>=100&&result<100+(int)factoryEffectChains().size())return applyEffectChain(factoryEffectChains()[(size_t)(result-100)],track);
    if(result==1){perform(effectsOpen,effect);return true;}if(result==2){perform(effectToggle,effect);return true;}
    if(result>=3&&result<=6)return moveEffect(effect,result==3?-1:result==4?1:result==5?-builtinEffectCount:builtinEffectCount);
    if(result>=10&&result<=14){perform(result==10?effectsChainCopy:result==11?effectsChainPaste:result==12?effectsChainSave:result==13?effectsChainLoad:effectsChainReset);return true;}return false;
}
void Studio::showEffectChainMenu(int effect){
    if(recording||recorder.isActive()){status="Finish the take before changing effect chains.";return;}PopupMenu menu;
    if(effect>=0&&effect<builtinEffectCount){const auto& m=project.tracks[(size_t)selectedTrack].mixer;int position=(int)(std::find(m.order.begin(),m.order.end(),effect)-m.order.begin());menu.addSectionHeader(effectName(effect)+" / slot "+String(position+1));menu.addItem(1,"Open effect");menu.addItem(2,"Enable / bypass");menu.addSeparator();menu.addItem(3,"Move earlier",position>0);menu.addItem(4,"Move later",position<builtinEffectCount-1);menu.addItem(5,"Move to first",position>0);menu.addItem(6,"Move to last",position<builtinEffectCount-1);menu.addSeparator();}
    PopupMenu factory;for(int i=0;i<(int)factoryEffectChains().size();++i)factory.addItem(100+i,factoryEffectChains()[(size_t)i].name);menu.addSubMenu("Factory effect chains",factory);menu.addItem(10,"Copy effect chain");menu.addItem(11,"Paste effect chain",effectClipboard.has_value());menu.addSeparator();menu.addItem(12,"Save effect chain file...");menu.addItem(13,"Load effect chain file...");menu.addItem(14,"Reset effects and order");
    auto safe=Component::SafePointer<Studio>(this);int track=selectedTrack;auto sourceRevision=revision;menu.showMenuAsync(menuOptions(),[safe,effect,track,sourceRevision](int result){if(safe)safe->invokeEffectChainMenu(result,effect,track,sourceRevision);});
}
void Studio::chooseEffectChainFile(bool save){
    if(recording||recorder.isActive()||pendingNoteTool()){status="Finish the take and resolve the note preview before exchanging effect chains.";return;}int track=selectedTrack;auto sourceRevision=revision;auto captured=captureEffectChain(project.tracks[(size_t)track].name+" effects",project.tracks[(size_t)track].mixer);captured.name=captured.name.substring(0,80).trim();auto folder=root.getChildFile("data/Effect chains");
    if(save){auto result=folder.createDirectory();if(result.failed()){status=result.getErrorMessage();return;}}else if(!folder.isDirectory())folder=root.getChildFile("data");
    chooser=std::make_unique<FileChooser>(save?"Save local effect chain":"Load local effect chain",folder.getChildFile("My chain.pulseeffects"),"*.pulseeffects;*.json",true);auto safe=Component::SafePointer<Studio>(this);
    chooser->launchAsync(FileBrowserComponent::canSelectFiles|(save?FileBrowserComponent::saveMode|FileBrowserComponent::warnAboutOverwriting:FileBrowserComponent::openMode),[safe,save,captured,track,sourceRevision](const FileChooser& chooser){if(!safe)return;auto file=chooser.getResult();if(file==File())return;
        if(save){auto result=saveEffectChain(captured,file);safe->status=result.wasOk()?"Captured effect chain saved locally: "+file.getFileName():result.getErrorMessage();}
        else{if(safe->recording||safe->recorder.isActive()||safe->revision!=sourceRevision||safe->selectedTrack!=track){safe->status="Channel or effects changed. Reopen Load effect chain.";return;}EffectChainPreset preset;auto result=loadEffectChain(file,preset);if(result.failed())safe->status=result.getErrorMessage();else safe->applyEffectChain(preset,track);}safe->repaint();});
}
bool Studio::performEffectChainAction(int action,int data){
    if(action<effectsChainMenu||action>effectsFactoryRecall)return false;
    if(recording||recorder.isActive()){status="Finish the take before using effect chains.";return true;}
    if(action==effectsChainMenu)showEffectChainMenu();else if(action==effectsChainCopy){effectClipboard=captureEffectChain("Copied effects",project.tracks[(size_t)selectedTrack].mixer);status="Effect chain copied. Paste on another insert or bus; routing stays with the destination.";}
    else if(action==effectsChainPaste){if(effectClipboard)applyEffectChain(*effectClipboard,selectedTrack);else status="Copy an effect chain first.";}
    else if(action==effectsChainSave||action==effectsChainLoad)chooseEffectChainFile(action==effectsChainSave);
    else if(action==effectsChainReset)applyEffectChain(factoryEffectChains().front(),selectedTrack);
    else if(action==effectMoveEarlier||action==effectMoveLater)moveEffect(data,action==effectMoveEarlier?-1:1);
    else if(action==effectsFactoryRecall&&data>=0&&data<(int)factoryEffectChains().size())applyEffectChain(factoryEffectChains()[(size_t)data],selectedTrack);
    return true;
}
}
