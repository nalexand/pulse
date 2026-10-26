// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <cstdlib>
#include <cmath>
namespace pulse {
using namespace juce;
var Studio::inputAlignmentState()const{
    auto result=inputAlignmentToJson(inputAlignment);auto* d=result.getDynamicObject();auto* device=deviceManager.getCurrentAudioDevice();double rate=device?device->getCurrentSampleRate():0;int in=device?device->getInputLatencyInSamples():0,out=device?device->getOutputLatencyInSamples():0;int64_t frames=0;auto valid=resolveInputAlignment(inputAlignment,rate,in,out,frames);
    d->setProperty("modeName",inputAlignmentName(inputAlignment.mode));d->setProperty("sampleRate",rate);d->setProperty("inputLatencySamples",in);d->setProperty("outputLatencySamples",out);d->setProperty("canResolve",valid.wasOk());d->setProperty("resolvedSamples",valid.wasOk()?var(frames):var());d->setProperty("resolvedMs",valid.wasOk()?var(frames*1000./rate):var());d->setProperty("error",valid.getErrorMessage());d->setProperty("lastTakeSamples",(int64)takeAlignmentSamples);d->setProperty("lastTakeRate",takeAlignmentRate);return result;
}
bool Studio::setInputAlignment(const InputAlignmentSettings& next){
    if(recording||recorder.isActive()||automationWriteContext||pendingNoteTool()||exporting.load()){status="Finish the take, automation pass, preview or export before changing input alignment.";return false;}
    auto valid=validateInputAlignment(next);if(valid.failed()){status=valid.getErrorMessage();return false;}if(next==inputAlignment){status="Input alignment is unchanged.";return true;}
    auto previous=inputAlignment;inputAlignment=next;if(!saveInputSettings()){inputAlignment=previous;return false;}
    status="Input alignment saved locally: "+inputAlignmentName(next.mode)+", manual "+String(next.manualMs,3)+" ms. The current driver is resolved once at the next audio REC; typing notes and input history are unchanged.";repaint();return true;
}
void Studio::showInputAlignment(){
    if(recording||recorder.isActive()||automationWriteContext||pendingNoteTool()||exporting.load()){status="Finish the take, automation pass, preview or export before changing input alignment.";return;}
    auto state=inputAlignmentState();String report="Reported input/output: "+state["inputLatencySamples"].toString()+" / "+state["outputLatencySamples"].toString()+" samples at "+String((double)state["sampleRate"],0)+" Hz. Driver reports are estimates, not a measured loopback.";
    auto* dialog=new AlertWindow("Input recording alignment",report+"\nPositive correction moves received audio earlier: capture waits that many samples, then the clip starts at the intended beat. Negative correction captures earlier; unavailable audio before Song start leaves a gap. Scheduled punch/end capture shifts too. REC/Stop ends at the last received input frame; it cannot recover future input.\nInput mode accounts for the reported capture delay; Input/output also includes the reported playback delay. Choose a correction from measurements of the monitoring path you use. Keyboard NOTES and retrospective input history are not compensated. Off is the default. Combined correction is limited to two seconds.",MessageBoxIconType::NoIcon,this);
    dialog->addComboBox("alignmentMode",{"Off","Manual only","Reported input + manual","Reported input/output + manual"},"Correction source");dialog->getComboBoxComponent("alignmentMode")->setSelectedItemIndex(inputAlignment.mode,dontSendNotification);
    dialog->addTextEditor("alignmentManual",String(inputAlignment.manualMs,3),"Manual milliseconds (-1000 to +1000)");dialog->addButton("Apply",1,KeyPress(KeyPress::returnKey));dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));auto safe=Component::SafePointer<Studio>(this);auto expected=inputAlignment;
    dialog->enterModalState(true,ModalCallbackFunction::create([safe,dialog,expected](int result){if(!safe||result!=1)return;if(safe->inputAlignment!=expected){safe->status="Input alignment changed. Reopen the settings dialog.";return;}auto text=dialog->getTextEditorContents("alignmentManual").trim();auto* start=text.toRawUTF8();char* end=nullptr;double value=std::strtod(start,&end);if(end==start||*end||!std::isfinite(value)){safe->status="Enter a finite manual correction in milliseconds.";return;}safe->setInputAlignment({dialog->getComboBoxComponent("alignmentMode")->getSelectedItemIndex(),value});}),true);
}
}
