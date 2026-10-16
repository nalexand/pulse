// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>
namespace pulse {
using namespace juce;
void Studio::toggleWaveMarker(double seconds){
    if(!waveSample||waveSample->audio.getNumSamples()<2||recording||recorder.isActive())return;
    int frame=jlimit(1,waveSample->audio.getNumSamples()-1,(int)std::llround(seconds*waveSample->sampleRate));
    int tolerance=jmax(1,(int)std::llround(waveSpan*8/jmax(1.f,waveRect.getWidth())*waveSample->sampleRate));auto nearest=waveMarkers.end();int distance=tolerance+1;
    for(auto it=waveMarkers.begin();it!=waveMarkers.end();++it)if(std::abs(*it-frame)<distance){distance=std::abs(*it-frame);nearest=it;}
    if(nearest!=waveMarkers.end()){waveMarkers.erase(nearest);status="Slice marker removed.";}
    else{if(waveMarkers.size()>=256){status="Maximum 256 interior slice markers. Remove a marker first.";return;}waveMarkers.push_back(frame);std::sort(waveMarkers.begin(),waveMarkers.end());status="Slice marker added. Alt+click near a marker removes it.";}
    ++waveRevision;repaint();
}
bool Studio::applyWaveDetection(const TransientOptions& options){
    if(!waveSample||recording||recorder.isActive()){status="Open audio and finish the take before detecting transients.";return false;}
    int first=jlimit(0,waveSample->audio.getNumSamples()-1,(int)std::llround(waveStart*waveSample->sampleRate));int end=jlimit(first+1,waveSample->audio.getNumSamples(),(int)std::llround(waveEnd*waveSample->sampleRate));
    std::vector<int> found;auto result=findWaveTransients(waveSample->audio,waveSample->sampleRate,first,end,options,found);if(result.failed()){status=result.getErrorMessage();return false;}
    waveMarkers=std::move(found);transientOptions=options;++waveRevision;status=String((int)waveMarkers.size())+" amplitude-rise markers found. Alt+click adds/removes marks; Slice at markers inserts the selected range.";repaint();return true;
}
void Studio::detectWaveTransients(){
    if(!waveSample){status="Open audio before detecting transients.";return;}
    auto* dialog=new AlertWindow("Detect transient markers","Find amplitude rises in the current selection. Review the markers before slicing. Alt+click the waveform to add or remove markers. Existing markers are replaced only when detection succeeds.",MessageBoxIconType::NoIcon,this);
    dialog->addTextEditor("floor",String(transientOptions.thresholdDb,1),"Noise floor (dBFS, -80 to -3)");dialog->addTextEditor("ratio",String(transientOptions.ratio,2),"Rise ratio (1.25 to 10)");dialog->addTextEditor("gap",String(transientOptions.minGapMs,1),"Minimum gap (ms, 10 to 1000)");dialog->addButton("Detect",1,KeyPress(KeyPress::returnKey));dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));
    auto safe=Component::SafePointer<Studio>(this);auto sourceRevision=waveRevision;
    dialog->enterModalState(true,ModalCallbackFunction::create([safe,dialog,sourceRevision](int choice){if(!safe||choice!=1)return;
        if(safe->waveRevision!=sourceRevision){safe->status="Audio selection changed. Reopen transient detection.";return;}
        double values[3]{};int index=0;for(auto id:{"floor","ratio","gap"}){auto text=dialog->getTextEditorContents(id).trim();auto* begin=text.toRawUTF8();char* end=nullptr;auto value=std::strtod(begin,&end);if(text.isEmpty()||end==begin||*end!='\0'||!std::isfinite(value)){safe->status="Enter numeric transient detection settings.";return;}values[index++]=value;}
        TransientOptions options{values[0],values[1],values[2]};safe->applyWaveDetection(options);
    }),true);
}
bool Studio::sliceWaveMarks(){
    if(recording||recorder.isActive()||pendingNoteTool()){status="Finish the take and resolve the note preview before inserting slices.";return false;}
    if(!waveSample){status="Open audio first.";return false;}int first=jlimit(0,waveSample->audio.getNumSamples()-1,(int)std::llround(waveStart*waveSample->sampleRate));int end=jlimit(first+1,waveSample->audio.getNumSamples(),(int)std::llround(waveEnd*waveSample->sampleRate));
    std::vector<int> boundaries{first};for(int marker:waveMarkers)if(marker>first&&marker<end)boundaries.push_back(marker);boundaries.push_back(end);
    if(boundaries.size()<3){status="Add or detect at least one marker inside the current selection.";return false;}
    Project next;auto result=sliceWaveFramesToPlaylist(project,selectedTrack,waveFile.getFullPathName(),waveSample->sampleRate,waveSample->audio.getNumSamples(),boundaries,jmax(0.,snapped(editCursorBeat)),next);
    if(result.failed()){status=result.getErrorMessage();return false;}return commitWaveSlices(std::move(next));
}
}
