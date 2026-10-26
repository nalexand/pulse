// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include "Version.h"
#include <cmath>
namespace pulse {
using namespace juce;
namespace {
String healthText(const var& h){
    auto ms=[](const var& v){return String((double)v,3)+" ms";};
    String text="Device: "+h["device"].toString()+" / "+h["backend"].toString()+"\n";
    text+="Output "+String((bool)h["outputReady"]?"available":"unavailable")+"; "+String((double)h["sampleRate"],0)+" Hz / "+h["bufferSize"].toString()+" frames\n";
    text+="Reported input/output latency: "+ms(h["inputLatencyMs"])+" / "+ms(h["outputLatencyMs"])+"\n";
    text+="Application callback: last "+ms(h["lastCallbackMs"])+", mean "+ms(h["meanCallbackMs"])+", worst "+ms(h["worstCallbackMs"])+"\n";
    text+="Callback load: mean "+String((double)h["meanUtilizationPercent"],1)+"%, worst "+String((double)h["worstUtilizationPercent"],1)+"%\n";
    text+="Callbacks / frames: "+h["callbacks"].toString()+" / "+h["frames"].toString()+"; deadline misses: "+h["deadlineMisses"].toString()+"\n";
    text+="Late callback starts: "+h["lateCallbackStarts"].toString()+"; estimated missing intervals: "+h["estimatedMissingBlocks"].toString()+"\n";
    text+="Driver-reported xruns: "+String((bool)h["driverXRunsSupported"]?h["driverXRuns"].toString():"unavailable")+"; JUCE combined counter: "+h["managerXRuns"].toString()+"\n";
    text+="Device starts/stops: "+h["deviceStarts"].toString()+" / "+h["deviceStops"].toString()+"; measurement "+h["measurement"].toString()+String((bool)h["resetPending"]?" (reset pending)":"")+"\n";
    text+="Silent-output callbacks: "+h["silentCallbacks"].toString()+"; invalid blocks/clock errors: "+h["invalidBlocks"].toString()+" / "+h["clockErrors"].toString()+"\n\n";
    text+="Playlist streaming: "+h["streamingSources"].toString()+" cursors / "+String((double)h["streamingBufferBytes"]/1048576.,2)+" MiB of 128 MiB; missing source frames: "+h["streamingMissedFrames"].toString()+"; read errors: "+h["streamingReadErrors"].toString()+"\n";
    text+="Streaming counters cover this workspace session and are retained by Reset measurements. A missed frame contributes silence while read-ahead catches up; it is separate from a driver xrun.\n\n";
    text+="Timings cover the app callback, including input capture and mixing. Late starts/missing intervals are estimates, not confirmed driver dropouts. Driver latency is reported, not a measured loopback. This build has no ASIO backend.\n";
    if(!(bool)h["consistent"])text+="Measurement is updating; read again.\n";
    if(h["lastInterruption"].toString().isNotEmpty())text+="\nLast device interruption:\n"+h["lastInterruption"].toString();return text;
}
class HealthPanel final:public Component,private Timer {
public:
    HealthPanel(std::function<var()> read,std::function<void()> clear,std::function<void()> write):reader(std::move(read)){
        report.setComponentID("audio-health-report");report.setMultiLine(true);report.setReadOnly(true);report.setScrollbarsShown(true);report.setCaretVisible(false);addAndMakeVisible(report);
        reset.setButtonText("Reset measurements");reset.setComponentID("audio-health-reset");reset.onClick=std::move(clear);addAndMakeVisible(reset);
        save.setButtonText("Save local report");save.setComponentID("audio-health-save");save.onClick=std::move(write);addAndMakeVisible(save);refresh();startTimer(500);
    }
    void resized()override{auto r=getLocalBounds().reduced(10);auto bottom=r.removeFromBottom(31);reset.setBounds(bottom.removeFromLeft(jmin(170,bottom.getWidth()/2)));bottom.removeFromLeft(8);save.setBounds(bottom.removeFromLeft(jmin(170,bottom.getWidth())));r.removeFromBottom(8);report.setBounds(r);}
private:
    void refresh(){report.setText(healthText(reader()),false);}
    void timerCallback()override{refresh();}std::function<var()> reader;TextEditor report;TextButton reset,save;
};
class SettingsPanel final:public Component,private Timer {
public:
    SettingsPanel(AudioDeviceManager& manager,std::function<var()> read,std::function<void()> health,std::function<void()> recover):reader(std::move(read)){
        viewport.setViewedComponent(new AudioDeviceSelectorComponent(manager,0,2,2,2,false,false,true,false),true);viewport.setScrollBarsShown(true,false);addAndMakeVisible(viewport);viewport.setComponentID("audio-device-selector");
        summary.setComponentID("audio-health-summary");summary.setJustificationType(Justification::centredLeft);summary.setFont(Font(FontOptions(12)));addAndMakeVisible(summary);
        diagnostics.setButtonText("Audio diagnostics...");diagnostics.setComponentID("audio-health-open");diagnostics.onClick=std::move(health);addAndMakeVisible(diagnostics);
        restart.setButtonText("Restart device");restart.setComponentID("audio-device-restart");restart.onClick=std::move(recover);addAndMakeVisible(restart);refresh();startTimer(500);
    }
    void resized()override{auto r=getLocalBounds().reduced(8);auto bottom=r.removeFromBottom(30);diagnostics.setBounds(bottom.removeFromLeft(jmin(170,bottom.getWidth()/2)));bottom.removeFromLeft(8);restart.setBounds(bottom.removeFromLeft(jmin(150,bottom.getWidth())));summary.setBounds(r.removeFromBottom(46));viewport.setBounds(r);viewport.getViewedComponent()->setSize(jmax(260,viewport.getWidth()-viewport.getScrollBarThickness()),420);}
private:
    void refresh(){auto h=reader();summary.setText(String((bool)h["outputReady"]?"Output available":"No active output")+" / callback worst "+String((double)h["worstCallbackMs"],2)+" ms / deadline misses "+h["deadlineMisses"].toString()+"\nDriver xruns: "+String((bool)h["driverXRunsSupported"]?h["driverXRuns"].toString():"unavailable")+". Diagnostics separates callback estimates from driver reports.",dontSendNotification);}
    void timerCallback()override{refresh();}std::function<var()> reader;Viewport viewport;Label summary;TextButton diagnostics,restart;
};
void sizeAudioPanel(Component& panel,Component& owner,int width,int height){auto area=owner.getParentMonitorArea();panel.setSize(jlimit(280,width,jmax(280,area.getWidth()-70)),jlimit(260,height,jmax(260,area.getHeight()-100)));}
}
void Studio::signalDeviceInterruption(){
    bool active=engine->playing.exchange(false);active|=inputMonitor.exchange(false);active|=recorder.isActive();active|=engine->previewPlaying.load();
    engine->cancelPreview();engine->takeGateOpen.store(false);
    if(active&&!deviceInterruptionPending.load(std::memory_order_acquire)){interruptedBeat.store(engine->position.load(),std::memory_order_relaxed);deviceInterruptionPending.store(true,std::memory_order_release);}
}
void Studio::handleDeviceInterruption(){
    if(!deviceInterruptionPending.load(std::memory_order_acquire))return;
    if(captureCallbacks.load()>0){status="Audio device interrupted; waiting for the in-flight callback before finalizing the take.";return;}
    double ended=interruptedBeat.load(std::memory_order_relaxed);deviceInterruptionPending.store(false,std::memory_order_release);finalizeInterruptedBeat=ended;
    if(automationWriteContext)discardAutomationWrite();
    bool take=recording||recorder.isActive();releaseKeys();if(recording&&!audioRecordMode)while(!recordedKeys.empty())recordOff(recordedKeys.begin()->first);
    if(recording)finishAudioRecording();else if(recorder.isActive())recorder.finish();
    auto takeStatus=status;engine->stop();engine->auditionPanicRequested.store(true);inputMonitor.store(false);recordDeviceStopped.store(false);engine->seekRequested.store(jmax(0.,ended));finalizeInterruptedBeat=-1;
    lastDeviceInterruption="Audio device stopped or changed. Playback and monitoring were disarmed; the cursor was retained. "+String(take?"Take result: "+takeStatus+" ":"")+"Open F10 to select/restart an output, then press Play explicitly.";status=lastDeviceInterruption;
}
var Studio::audioHealthState()const{
    auto s=audioHealth.snapshot();auto* d=new DynamicObject();var result(d);
    d->setProperty("schema",1);d->setProperty("consistent",s.consistent);d->setProperty("measurement",(int64)s.measurement);d->setProperty("resetPending",s.resetPending);d->setProperty("running",s.running);d->setProperty("deviceStarts",(int64)s.deviceStarts);d->setProperty("deviceStops",(int64)s.deviceStops);
    d->setProperty("callbacks",(int64)s.callbacks);d->setProperty("frames",(int64)s.frames);d->setProperty("deadlineMisses",(int64)s.deadlineMisses);d->setProperty("lateCallbackStarts",(int64)s.lateStarts);d->setProperty("estimatedMissingBlocks",(int64)s.estimatedMissingBlocks);d->setProperty("invalidBlocks",(int64)s.invalidBlocks);d->setProperty("clockErrors",(int64)s.clockErrors);d->setProperty("silentCallbacks",(int64)s.silentBlocks);
    d->setProperty("lastCallbackMs",s.lastMs);d->setProperty("meanCallbackMs",s.meanMs);d->setProperty("worstCallbackMs",s.maxMs);d->setProperty("meanUtilizationPercent",s.meanUtilization);d->setProperty("worstUtilizationPercent",s.worstUtilization);d->setProperty("maximumStartGapMs",s.maxStartGapMs);
    auto* device=deviceManager.getCurrentAudioDevice();double rate=device?device->getCurrentSampleRate():s.rate;int in=device?jmax(0,device->getInputLatencyInSamples()):0,out=device?jmax(0,device->getOutputLatencyInSamples()):0;int xruns=device?device->getXRunCount():-1;
    d->setProperty("backend",deviceManager.getCurrentAudioDeviceType());d->setProperty("device",device?device->getName():"No device");d->setProperty("sampleRate",rate);d->setProperty("bufferSize",device?device->getCurrentBufferSizeSamples():s.nominalBlock);d->setProperty("inputLatencySamples",in);d->setProperty("outputLatencySamples",out);d->setProperty("inputLatencyMs",rate>0?in*1000/rate:0);d->setProperty("outputLatencyMs",rate>0?out*1000/rate:0);
    bool stalled=stalledPlaybackTicks<0&&engine->processedFrames.load()==lastPlaybackFrames;d->setProperty("callbacksStalled",stalled);d->setProperty("outputReady",device&&device->isOpen()&&activeOutputs.load()>0&&!stalled);d->setProperty("driverXRunsSupported",xruns>=0);d->setProperty("driverXRuns",xruns>=0?var(xruns):var());d->setProperty("managerXRuns",deviceManager.getXRunCount());auto streaming=assets.streamingStatistics();d->setProperty("streamingSources",(int64)streaming.sources);d->setProperty("streamingBufferBytes",(int64)streaming.bytes);d->setProperty("streamingBufferLimitBytes",(int64)StreamingBudget::limit);d->setProperty("streamingMissedFrames",(int64)streaming.missedFrames);d->setProperty("streamingReadErrors",(int64)streaming.readErrors);d->setProperty("asioCompiled",false);d->setProperty("lastInterruption",lastDeviceInterruption);return result;
}
void Studio::showAudioSettings(){
    auto safe=Component::SafePointer<Studio>(this);auto* panel=new SettingsPanel(deviceManager,[safe]{return safe?safe->audioHealthState():var();},[safe]{if(safe)safe->showAudioHealth();},[safe]{if(safe)safe->restartAudioDevice();});sizeAudioPanel(*panel,*this,560,510);
    DialogWindow::LaunchOptions options;options.dialogTitle="Audio settings — Pulse Studio";options.dialogBackgroundColour=Colour(0xff434c51);options.content.setOwned(panel);options.componentToCentreAround=this;options.useNativeTitleBar=true;options.resizable=true;options.launchAsync();
}
void Studio::showAudioHealth(){
    auto safe=Component::SafePointer<Studio>(this);auto* panel=new HealthPanel([safe]{return safe?safe->audioHealthState():var();},[safe]{if(safe)safe->perform(audioHealthReset);},[safe]{if(safe)safe->saveAudioHealth();});sizeAudioPanel(*panel,*this,620,420);
    DialogWindow::LaunchOptions options;options.dialogTitle="Audio diagnostics — Pulse Studio";options.dialogBackgroundColour=Colour(0xff434c51);options.content.setOwned(panel);options.componentToCentreAround=this;options.useNativeTitleBar=true;options.resizable=true;options.launchAsync();
}
bool Studio::saveAudioHealth(){
    auto report=audioHealthState();report.getDynamicObject()->setProperty("applicationVersion",appVersion);report.getDynamicObject()->setProperty("capturedAt",Time::getCurrentTime().toISO8601(true));auto file=root.getChildFile("data/audio-diagnostics.json");TemporaryFile temp(file);
    bool ok=temp.getFile().replaceWithText(JSON::toString(report,false))&&temp.overwriteTargetFileWithTemporary();status=ok?"Audio diagnostics saved locally: "+file.getFullPathName():"Could not save audio diagnostics. Check the workspace and disk space.";return ok;
}
void Studio::restartAudioDevice(){
    if(!audioEnabled){status="Audio device restart requires the running application.";return;}
    signalDeviceInterruption();handleDeviceInterruption();engine->stop();releaseKeys();inputMonitor.store(false);deviceManager.closeAudioDevice();deviceManager.restartLastAudioDevice();handleDeviceInterruption();
    auto* device=deviceManager.getCurrentAudioDevice();status=device&&device->isOpen()&&activeOutputs.load()>0?"Audio device restarted. Playback and monitoring stay off; press Play when ready.":"Audio device could not restart. Select an available output in F10.";
}
}
