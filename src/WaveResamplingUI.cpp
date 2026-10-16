// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <cmath>
namespace pulse {
using namespace juce;
void Studio::showWaveResample(){
    if(!waveSample){status="Open audio before converting its sample rate.";return;}
    auto* dialog=new AlertWindow("Convert sample rate","Convert the entire file while preserving pitch and duration (nearest output frame). Processing runs in the background. A new local WAV is created; To sampler / To Playlist applies it. Audio-edit Undo restores the previous file.",MessageBoxIconType::NoIcon,this);
    StringArray choices;for(int rate:{8000,16000,22050,32000,44100,48000,88200,96000,192000})choices.add(String(rate));
    dialog->addComboBox("rate",choices,"Sample rate (Hz)");auto* combo=dialog->getComboBoxComponent("rate");combo->setSelectedItemIndex(choices.indexOf(String((int)waveSample->sampleRate))>=0?choices.indexOf(String((int)waveSample->sampleRate)):5);
    dialog->addButton("Convert",1,KeyPress(KeyPress::returnKey));dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));
    auto safe=Component::SafePointer<Studio>(this);auto expected=waveRevision;
    dialog->enterModalState(true,ModalCallbackFunction::create([safe,dialog,expected](int choice){if(!safe||choice!=1)return;
        if(safe->waveRevision!=expected){safe->status="Audio or selection changed. Reopen sample-rate conversion.";return;}
        safe->beginWaveResample(dialog->getComboBoxComponent("rate")->getText().getDoubleValue());
    }),true);
}
bool Studio::beginWaveResample(double targetRate,bool paste,bool replace){
    if(waveEditBusy||recording||recorder.isActive()||!waveSample){status="Open audio and finish the take or conversion first.";return false;}
    if(!std::isfinite(targetRate)||targetRate<8000||targetRate>192000||targetRate!=std::floor(targetRate)){status="Use a whole-number sample rate between 8000 and 192000 Hz.";return false;}
    double sourceRate=waveSample->sampleRate;
    if(!paste&&targetRate==sourceRate){status="Audio already uses that sample rate.";return false;}
    if(paste&&(targetRate!=sourceRate||validateWaveAudio(waveClipboard,waveClipboardRate).failed())){status="Copy valid audio before pasting.";return false;}
    int first=jlimit(0,waveSample->audio.getNumSamples()-1,(int)std::llround(waveStart*sourceRate));int end=jlimit(first+1,waveSample->audio.getNumSamples(),(int)std::llround(waveEnd*sourceRate));
    if(paste){int added=jmax(1,(int)std::llround(waveClipboard.getNumSamples()*sourceRate/waveClipboardRate));if(waveSample->audio.getNumSamples()-(replace?end-first:0)+added>sourceRate*120){status="This edit exceeds the 120-second sample limit.";return false;}}
    if(waveEditThread.joinable())waveEditThread.join();
    AudioBuffer<float> source,clipboard;source.makeCopyOf(waveSample->audio);if(paste)clipboard.makeCopyOf(waveClipboard);
    waveEditSourceRevision=waveRevision;waveEditRate=targetRate;waveEditFirst=first;waveEditEnd=end;waveEditOutput.setSize(0,0);waveEditError.clear();waveEditCancel.store(false);waveEditDone.store(false);waveEditBusy=true;
    double clipboardRate=waveClipboardRate;
    waveEditThread=std::thread([this,source=std::move(source),clipboard=std::move(clipboard),sourceRate,targetRate,clipboardRate,first,end,paste,replace]()mutable{
        try{
            auto cancelled=[this]{return waveEditCancel.load();};Result result=Result::ok();
            if(paste)result=spliceWave(source,sourceRate,first,end,replace?WaveOperation::replace:WaveOperation::insert,clipboard,clipboardRate,waveEditOutput,waveEditFirst,waveEditEnd,cancelled);
            else{result=resampleWave(source,sourceRate,targetRate,waveEditOutput,cancelled);if(result.wasOk()){int frames=waveEditOutput.getNumSamples();waveEditFirst=jlimit(0,frames-1,(int)std::llround(first*targetRate/sourceRate));waveEditEnd=jlimit(waveEditFirst+1,frames,(int)std::llround(end*targetRate/sourceRate));}}
            if(result.failed())waveEditError=result.getErrorMessage();
        }catch(const std::exception&){waveEditError="Not enough resources to convert audio. The previous file is retained.";}
        waveEditDone.store(true,std::memory_order_release);
    });
    status=paste?"Converting clipboard audio to destination rate in the background. Edit menu / Ctrl+P can cancel.":"Converting audio in the background. Edit menu / Ctrl+P can cancel.";repaint();return true;
}
void Studio::finishWaveResample(){
    if(!waveEditBusy||!waveEditDone.load(std::memory_order_acquire))return;
    if(waveEditThread.joinable())waveEditThread.join();waveEditBusy=false;waveEditDone.store(false);
    if(waveEditCancel.load())status="Audio conversion cancelled. Previous file and history retained.";
    else if(waveEditSourceRevision!=waveRevision)status="Audio or selection changed. Converted result discarded; repeat the conversion.";
    else if(recording||recorder.isActive())status="Recording started. Converted result discarded; repeat after the take.";
    else if(waveEditError.isNotEmpty())status=waveEditError;
    else saveWaveEdit(waveEditOutput,waveEditFirst,waveEditEnd,waveEditRate);
    waveEditOutput.setSize(0,0);repaint();
}
}
