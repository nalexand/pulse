// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <cmath>
namespace pulse {
using namespace juce;
namespace {
class MixReportDialog final:public AlertWindow {
public:
    MixReportDialog(const String& message,const String& summary,bool busy,bool canApply,Component* parent):AlertWindow("Offline mix analysis",message,MessageBoxIconType::NoIcon,parent){
        auto area=parent->getParentMonitorArea();text.setComponentID("mix-analysis-report");text.setMultiLine(true,true);text.setReadOnly(true);text.setScrollbarsShown(true);text.setText(summary,false);text.setSize(jmax(180,jmin(690,(int)(area.getWidth()*.54))),jmax(100,jmin(400,area.getHeight()-280)));addCustomComponent(&text);if(!busy)addButton("Analyze current mix",1);if(canApply)addButton("Apply level reduction",2);if(busy)addButton("Cancel analysis",3);addButton("Close",0,KeyPress(KeyPress::escapeKey));
    }
private:TextEditor text;
};
}
void Studio::restoreMixAnalysis(){
    auto file=root.getChildFile("data/mix-analysis-report.json");if(!file.existsAsFile()||file.getSize()>1024*1024)return;auto json=JSON::parse(file);if(!json.isObject()||(int)json["schema"]!=1||json["application"].toString()!="Pulse Studio mix analysis"||!json["summary"].isString()||json["summary"].toString().length()>100000)return;mixReportJson=json;mixReportLive=false;
}
bool Studio::mixCanApply()const{return !mixTask&&mixReportLive&&mixReport.canReduce&&mixReportRevision==revision&&!recording&&!recorder.isActive()&&!pendingNoteTool()&&!exporting.load();}
var Studio::mixAnalysisState()const{
    auto* o=new DynamicObject();o->setProperty("busy",mixTask!=nullptr);o->setProperty("applying",mixTask&&mixApplying);o->setProperty("progress",mixTask?mixTask->progress.load():mixReportJson.isObject()?1.f:0.f);o->setProperty("sourceRevision",(int64)mixReportRevision);o->setProperty("current",mixReportLive&&mixReportRevision==revision);o->setProperty("canApplyLevel",mixCanApply());o->setProperty("report",mixReportJson);o->setProperty("message",mixMessage);o->setProperty("reportPath",root.getChildFile("data/mix-analysis-report.json").getFullPathName());return var(o);
}
bool Studio::startMixAnalysis(uint64_t expected,bool applyLevel,bool reveal){
    if(expected!=revision||mixTask||recording||recorder.isActive()||pendingNoteTool()||exporting.load()){status="Project changed or is busy. Finish the take/preview/export and retry mix analysis.";return false;}
    if(applyLevel&&!mixCanApply()){status="Analyze the current project first; a verified level reduction must be available.";return false;}
    auto base=currentFile==File()?root:currentFile.getParentDirectory();auto source=absoluteAssets(project,base);mixExpectedHash=mixProjectHash(source,root);if(applyLevel&&mixExpectedHash!=mixReport.projectHash){status="Project source changed. Analyze the mix again.";return false;}
    RenderSettings settings;RenderPlan plan;auto valid=planRender(source,settings,plan);if(valid.failed()||plan.outputFrames>48000*600LL){status=valid.failed()?valid.getErrorMessage():"Mix analysis is limited to ten minutes including its two-second tail.";return false;}
    mixBaselineAssets=mixReport.assets;mixApplying=applyLevel;mixShowResult=reveal;mixReportRevision=revision;if(applyLevel)source.master=(float)mixReport.masterAfter;mixReportLive=false;
    mixTask=std::make_unique<MixAnalysisTask>(std::move(source),root);if(!mixTask->startThread(Thread::Priority::low)){mixTask.reset();status="Could not start mix analysis worker.";return false;}mixMessage=applyLevel?"Verifying the proposed level correction against a fresh render and the original audio bytes...":"Analyzing an offline project snapshot. Live inputs, click and Browser preview are excluded.";status=mixMessage;return true;
}
void Studio::finishMixAnalysis(){
    if(!mixTask||!mixTask->done.load(std::memory_order_acquire))return;auto* reportDialog=dynamic_cast<MixReportDialog*>(Component::getCurrentlyModalComponent());if(mixApplying&&Component::getCurrentlyModalComponent()&&!reportDialog)return;
    bool show=mixShowResult,apply=mixApplying;mixShowResult=false;auto refresh=[&]{if(reportDialog){reportDialog->exitModalState(0);showMixAnalysis();}else if(show&&!Component::getCurrentlyModalComponent())showMixAnalysis();};auto result=mixTask->result;bool cancelled=mixTask->cancel.load();auto master=mixTask->project.master;auto report=std::move(mixTask->report);mixTask.reset();
    if(cancelled||result.failed()){mixMessage=cancelled?"Mix analysis cancelled. No project edit was applied.":result.getErrorMessage();status=mixMessage;refresh();return;}
    bool current=revision==mixReportRevision&&mixProjectHash(project,currentFile==File()?root:currentFile.getParentDirectory())==mixExpectedHash;
    if(apply){double ceiling=std::pow(10.,-1./20);if(!current||recording||recorder.isActive()||pendingNoteTool()||exporting.load()||!sameMixAssets(mixBaselineAssets,report.assets)||jmax(report.measured.peak[0],report.measured.peak[1])>ceiling+1e-5){mixMessage="Level correction cancelled: project/audio changed, app is busy, or the verified peak exceeded the ceiling. Analyze again.";status=mixMessage;refresh();return;}
        checkpoint();project.master=master;changed();mixReportRevision=revision;current=true;mixMessage="Verified Master level reduction applied as one Undo. Other project settings are retained.";
    }else mixMessage=current?"Offline mix analysis complete. Review the report before applying any correction.":"Analysis complete for an older snapshot. Analyze the current mix before applying a correction.";
    mixReport=std::move(report);mixReportJson=mixAnalysisToJson(mixReport);mixReportLive=current;auto saved=saveMixAnalysisReport(root.getChildFile("data/mix-analysis-report.json"),mixReport);if(saved.failed())mixMessage+=" "+saved.getErrorMessage();status=mixMessage;refresh();
}
void Studio::showMixAnalysis(){
    closeCommandPalette();String message=mixTask?mixMessage+" "+String((int)(mixTask->progress.load()*100))+"%":mixReportJson.isObject()?(mixReportLive&&mixReportRevision==revision?"Current project snapshot. A level correction is verified again before it can apply.":"Saved/stale report: read-only. Analyze the current mix to enable corrections."):"Analyze the current Song or Pattern through the offline export signal path.";
    if(!mixTask&&mixMessage.isNotEmpty())message=mixMessage+"\n"+message;
    String summary=mixReportJson.isObject()?mixReportJson["summary"].toString():"48 kHz stereo, including a two-second tail, up to ten minutes.\n\nMeasures sample peak, RMS, full-scale sample count, DC, stereo correlation and FFT power balance.\n\nOnly an eligible Master-gain reduction to a -1 dBFS sample ceiling can be applied, with one Undo. Saturation or Master-gain automation requires manual review.\n\nThis is not LUFS/true-peak mastering. Live monitoring and Browser previews are excluded. No EQ/dynamics changes are made automatically.";
    auto* d=new MixReportDialog(message,summary,mixTask!=nullptr,mixCanApply(),this);auto safe=Component::SafePointer<Studio>(this);auto expected=revision;d->enterModalState(true,ModalCallbackFunction::create([safe,expected](int answer){if(!safe)return;if(answer==1)safe->startMixAnalysis(expected,false,true);else if(answer==2)safe->startMixAnalysis(expected,true,true);else if(answer==3&&safe->mixTask){safe->mixTask->cancel.store(true);safe->mixShowResult=false;safe->status="Cancelling mix analysis...";}}),true);
}
}
