// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
namespace pulse {
using namespace juce;
namespace {
class CleanupReview final:public AlertWindow {
public:
    CleanupReview(const RecoveryCleanupPlan& plan,Component* parent):AlertWindow("Review recovery cleanup","Remove "+String((int)plan.remove.size())+" archived copies ("+File::descriptionOfSizeInBytes(plan.removeBytes)+"). Keep "+String((int)(plan.archives.size()-plan.remove.size()))+"; newest two always stay.",MessageBoxIconType::WarningIcon,parent){
        String text="Current recovery, .bak, manual versions, Before_* backups, samples and damaged evidence are excluded. Only the verified files listed below will be deleted.\n\n";for(int index:plan.remove){const auto& row=plan.archives[(size_t)index];text+=Time(row.modified).formatted("%Y-%m-%d %H:%M")+" | "+File::descriptionOfSizeInBytes(row.bytes)+" | "+row.file.getFileName()+"\n";}
        files.setComponentID("recovery-cleanup-files");files.setMultiLine(true,true);files.setReadOnly(true);files.setScrollbarsShown(true);files.setCaretVisible(false);files.setText(text.isEmpty()?"No eligible copies exceed these limits.":text,false);files.setSize(400,115);addCustomComponent(&files);
        addButton("Keep all copies",0,KeyPress(KeyPress::escapeKey),KeyPress(KeyPress::returnKey));if(!plan.remove.empty())addButton("Delete listed copies",1);
    }
private:TextEditor files;
};
bool number(const String& text,int& value){auto trimmed=text.trim();if(trimmed.isEmpty()||trimmed.length()>5||!trimmed.containsOnly("0123456789"))return false;value=trimmed.getIntValue();return true;}
}
void Studio::showRecoveryRetention(){
    if(retentionBusy||recording||recorder.isActive()||pendingNoteTool()){status="Finish the current take, note preview or recovery task first.";return;}
    auto* dialog=new AlertWindow("Recovery history limits","Archived copies only; newest two always stay.",MessageBoxIconType::NoIcon,this);
    dialog->addTextEditor("copies",String(retentionPolicy.keepNewest),"Maximum copies (2-2000)");dialog->addTextEditor("days",String(retentionPolicy.maxAgeDays),"Maximum age in days (0 disables age)");dialog->addTextEditor("mib",String(retentionPolicy.maxMiB),"Maximum total MiB (1-32768)");dialog->addComboBox("automatic",{"Off - review cleanup manually","On - clean up after permanent saves"},"Automatic cleanup");dialog->getComboBoxComponent("automatic")->setSelectedItemIndex(retentionPolicy.automatic?1:0,dontSendNotification);
    dialog->addButton("Save limits",1);dialog->addButton("Save and review cleanup",2);dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));auto previous=retentionPolicy;auto file=root.getChildFile("data/recovery-retention.json");auto token=projectFileToken(file);auto safe=Component::SafePointer<Studio>(this);
    dialog->enterModalState(true,ModalCallbackFunction::create([safe,dialog,previous,file,token](int choice){
        if(!safe||choice==0)return;if(safe->retentionBusy||safe->recording||safe->recorder.isActive()||safe->pendingNoteTool()||safe->retentionPolicy!=previous||projectFileToken(file)!=token){safe->status="Recovery settings changed or the session is busy. Reopen the limits dialog.";return;}
        RecoveryRetentionPolicy next;auto* automatic=dialog->getComboBoxComponent("automatic");if(!number(dialog->getTextEditorContents("copies"),next.keepNewest)||!number(dialog->getTextEditorContents("days"),next.maxAgeDays)||!number(dialog->getTextEditorContents("mib"),next.maxMiB)||!automatic||automatic->getSelectedItemIndex()<0){safe->status="Use whole numbers for recovery limits.";return;}next.automatic=automatic->getSelectedItemIndex()==1;auto result=saveRecoveryRetention(file,next);if(result.failed()){safe->status=result.getErrorMessage();return;}safe->retentionPolicy=next;safe->retentionReview.reset();safe->status="Recovery history limits saved locally. Automatic cleanup "+String(next.automatic?"enabled for future permanent saves.":"off.");if(choice==2)safe->startRecoveryCleanup(false);
    }),true);
}
bool Studio::startRecoveryCleanup(bool automatic){
    if(currentFile!=File()&&currentFile.getParentDirectory()==root.getChildFile("data/History")){if(!automatic)status="Save the working song outside Recovery History before cleanup.";return false;}
    if(retentionBusy||recording||recorder.isActive()||pendingNoteTool()){if(!automatic)status="Finish the current take, preview or recovery task first.";return false;}
    if(automatic&&!retentionPolicy.automatic)return false;if(retentionThread.joinable())retentionThread.join();retentionBusy=true;retentionTask=automatic?3:1;retentionShowReview=!automatic;retentionCancel.store(false);retentionDone.store(false);retentionWorkerPlan.reset();retentionReview.reset();retentionResult={};auto workspace=root;auto policy=retentionPolicy;
    if(!automatic)status="Reviewing archived recovery copies in the background...";
    retentionThread=std::thread([this,workspace,policy,automatic]{auto plan=std::make_shared<RecoveryCleanupPlan>();auto result=planRecoveryCleanup(workspace,policy,*plan,&retentionCancel);if(result.wasOk()&&automatic)result=applyRecoveryCleanup(*plan,retentionResult,&retentionCancel);retentionSucceeded=result.wasOk();retentionMessage=result.wasOk()?(automatic?"Recovery cleanup: removed "+String(retentionResult.removed)+" archived copies ("+File::descriptionOfSizeInBytes(retentionResult.bytes)+").":"Recovery cleanup review ready."):result.getErrorMessage()+" Removed "+String(retentionResult.removed)+" copies before stopping; remaining files retained.";if(result.wasOk())retentionWorkerPlan=std::move(plan);retentionDone.store(true,std::memory_order_release);});return true;
}
bool Studio::applyRecoveryReview(const std::shared_ptr<RecoveryCleanupPlan>& review){
    if(currentFile!=File()&&currentFile.getParentDirectory()==root.getChildFile("data/History")){status="Save the working song outside Recovery History before cleanup.";return false;}
    if(!review||review!=retentionReview||review->workspace!=root||review->policy!=retentionPolicy||retentionBusy||recording||recorder.isActive()||pendingNoteTool()){status="Recovery cleanup review changed or session is busy. Review again.";return false;}if(retentionThread.joinable())retentionThread.join();retentionBusy=true;retentionTask=2;retentionShowReview=false;retentionCancel.store(false);retentionDone.store(false);retentionResult={};status="Checking the reviewed files again before cleanup...";
    retentionThread=std::thread([this,review]{auto result=applyRecoveryCleanup(*review,retentionResult,&retentionCancel);retentionSucceeded=result.wasOk();retentionMessage=result.wasOk()?"Recovery cleanup complete: removed "+String(retentionResult.removed)+" copies ("+File::descriptionOfSizeInBytes(retentionResult.bytes)+").":result.getErrorMessage()+" Removed "+String(retentionResult.removed)+" copies before stopping; remaining files retained.";retentionDone.store(true,std::memory_order_release);});return true;
}
void Studio::finishRecoveryCleanup(){
    if(retentionBusy){if(!retentionDone.load(std::memory_order_acquire))return;if(retentionThread.joinable())retentionThread.join();retentionBusy=false;if(retentionTask==1&&retentionSucceeded&&!retentionCancel.load())retentionReview=std::move(retentionWorkerPlan);else retentionShowReview=false;if(retentionTask!=3||!retentionSucceeded||retentionResult.removed>0)status=retentionMessage;}
    if(retentionShowReview&&!retentionBusy&&!Component::getCurrentlyModalComponent()){
        retentionShowReview=false;if(!retentionReview||retentionReview->policy!=retentionPolicy||recording||recorder.isActive()||pendingNoteTool()){status="Recovery cleanup review retained. Reopen it when the current operation is finished.";return;}
        auto review=retentionReview;auto* dialog=new CleanupReview(*review,this);auto safe=Component::SafePointer<Studio>(this);dialog->enterModalState(true,ModalCallbackFunction::create([safe,review](int choice){if(safe&&choice==1)safe->applyRecoveryReview(review);}),true);
    }
}
void Studio::cancelRecoveryCleanup(){retentionCancel.store(true);retentionShowReview=false;status=retentionBusy?"Cancelling recovery cleanup; completed deletions stay applied, remaining copies are retained.":"No recovery cleanup task is running.";}
}
