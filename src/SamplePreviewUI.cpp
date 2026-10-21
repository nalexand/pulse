// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <cstdlib>
#include <cmath>
namespace pulse {
using namespace juce;
bool Studio::configureSamplePreview(const BrowserAsset& asset,const SamplePreviewOptions& options,uint64_t expected,
                                   const String& catalogToken,const String& sourceStamp,bool audition){
    if(recording||recorder.isActive()){status="Finish the take before changing sample audition.";return false;}
    if(revision!=expected){status="Project changed. Reopen the sample preview dialog.";return false;}
    auto valid=validateSamplePreview(options);if(valid.failed()){status=valid.getErrorMessage();return false;}
    String error;auto* source=assets.load(asset.file,error);if(!source){status=error;return false;}
    double duration=source->audio.getNumSamples()/source->sampleRate;
    if(!std::isfinite(asset.start)||!std::isfinite(asset.end)||asset.start<0||asset.end<0||asset.start>=duration||(asset.end>0&&(asset.end<=asset.start||asset.end>duration+1e-9))){status="Invalid sample audition range.";return false;}
    valid=saveSamplePreview(samplePreviewFile(),asset.file,options,catalogToken,sourceStamp);
    if(valid.failed()){status=valid.getErrorMessage();return false;}
    if(audition)return previewBrowserSample(asset);
    // A settings-only edit never leaves the previous settings running audibly.
    engine->cancelPreview();status="Saved source audition tempo and repeat settings locally. Click this file in Local files or Favorite samples to preview; channel and Playlist sounds are unchanged.";repaint();return true;
}
void Studio::showSamplePreviewSettings(const BrowserAsset& asset){
    if(recording||recorder.isActive()){status="Finish the take before changing sample audition.";return;}
    String error;if(!assets.load(asset.file,error)){status=error;return;}
    SamplePreviewOptions options;auto loaded=loadSamplePreview(samplePreviewFile(),asset.file,options);if(loaded.failed()){status=loaded.getErrorMessage();return;}
    auto stamp=samplePreviewStamp(asset.file),token=samplePreviewToken(samplePreviewFile());auto rev=revision;
    auto* dialog=new AlertWindow("Sample audition tempo",asset.file.getFileName()+"\nEnter the original loop BPM. Project-tempo mode changes playback speed AND pitch, follows tempo changes, and waits for the next beat if the song is playing. Repeat uses the chosen source range. Original audio and channel settings stay unchanged.",MessageBoxIconType::NoIcon,this);
    dialog->addComboBox("mode",{"Original speed","Project tempo (pitch follows speed)"},"Audition speed");dialog->getComboBoxComponent("mode")->setSelectedItemIndex(options.sync?1:0);
    dialog->addTextEditor("bpm",String(options.sourceBpm,3),"Original source BPM (20 to 400)");
    dialog->addComboBox("repeat",{"Play once","Repeat source range"},"Playback");dialog->getComboBoxComponent("repeat")->setSelectedItemIndex(options.repeat?1:0);
    dialog->addButton("Save + Preview",1,KeyPress(KeyPress::returnKey));dialog->addButton("Save",2);dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));
    auto safe=Component::SafePointer<Studio>(this);
    dialog->enterModalState(true,ModalCallbackFunction::create([safe,dialog,asset,rev,token,stamp](int result){
        if(!safe||(result!=1&&result!=2))return;
        auto text=dialog->getTextEditorContents("bpm").trim();const char* begin=text.toRawUTF8();char* end=nullptr;double bpm=std::strtod(begin,&end);
        if(text.isEmpty()||end==begin||*end!='\0'){safe->status="Enter a numeric source BPM from 20 to 400.";safe->repaint();return;}
        SamplePreviewOptions next{dialog->getComboBoxComponent("mode")->getSelectedItemIndex()==1,dialog->getComboBoxComponent("repeat")->getSelectedItemIndex()==1,bpm};
        safe->configureSamplePreview(asset,next,rev,token,stamp,result==1);safe->repaint();
    }),true);
}
}
