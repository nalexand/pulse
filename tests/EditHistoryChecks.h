// SPDX-License-Identifier: AGPL-3.0-only
#include "EditHistory.h"
static void testEditHistory(){
    auto source=emptyProject();source.patterns[0].name="Verse";source.tracks[7].name="Vault lead";std::deque<HistorySnapshot> undo,redo;auto project=source;
    auto saved=[&](const Project& p){return JSON::toString(projectToJson(p),true,22);};
    std::vector<String> snapshots{saved(project)};
    undo.emplace_back(project,1000);project.patterns[0].lanes[7]={{0,1,60,.8f}};snapshots.push_back(saved(project));
    undo.emplace_back(project,2000);project.tracks[7].synth.fmIndex=7;project.tracks[7].instrument=Instrument::fm;snapshots.push_back(saved(project));
    undo.emplace_back(project,3000);project.clips.push_back({7,0,8,8});snapshots.push_back(saved(project));
    undo.emplace_back(project,4000);project.tracks[7].mixer.compressor=true;snapshots.push_back(saved(project));
    auto rows=projectHistoryRows(project,undo,redo);expect(rows.size()==5&&rows[0].state==0&&rows[0].time==0&&rows[1].time==1000&&rows[4].time==4000,"history rows associate original edit timestamps with chronological snapshots");
    expect(rows[1].summary.contains("Notes")&&rows[1].detail.contains("Verse")&&rows[1].detail.contains("Vault lead"),"history identifies affected note lane and channel");
    expect(rows[2].summary.contains("Sound")&&rows[2].detail.contains("Vault lead")&&rows[3].summary=="Playlist"&&rows[4].summary.contains("Mixer"),"history distinguishes sounds, arrangement and mixer edits");
    auto before=saved(project);expect(moveProjectHistory(project,undo,redo,-1).failed()&&moveProjectHistory(project,undo,redo,5).failed()&&saved(project)==before&&undo.size()==4&&redo.empty(),"invalid history destinations reject the entire move without changing snapshots");
    expect(moveProjectHistory(project,undo,redo,1).wasOk()&&saved(project)==snapshots[1]&&undo.size()==1&&redo.size()==3,"multi-edit history rewind restores the exact selected checkpoint");
    auto afterRows=projectHistoryRows(project,undo,redo);bool same=rows.size()==afterRows.size();for(size_t i=0;i<rows.size();++i)same&=rows[i].state==afterRows[i].state&&rows[i].time==afterRows[i].time&&rows[i].summary==afterRows[i].summary&&rows[i].detail==afterRows[i].detail;expect(same,"searchable rows and timestamps remain identical after multi-step Undo");
    for(int state:{4,0,2,4,1,0,4})expect(moveProjectHistory(project,undo,redo,state).wasOk()&&saved(project)==snapshots[(size_t)state]&&undo.size()+redo.size()==4,"arbitrary forward/backward jumps preserve every recoverable state exactly");
    before=saved(project);expect(moveProjectHistory(project,undo,redo,4).wasOk()&&saved(project)==before&&undo.back().editTime==4000,"restoring the current state is a no-op and retains timestamps");
    moveProjectHistory(project,undo,redo,2);undo.emplace_back(project,5000);redo.clear();project.bpm=91;rows=projectHistoryRows(project,undo,redo);expect(rows.size()==4&&rows.back().time==5000&&rows.back().summary=="Tempo"&&redo.empty(),"a new branch replaces only discarded Redo and records the new transition");
    auto changed=source;changed.patterns[0].name="Chorus";changed.patterns[0].length=4;changed.patterns[0].colour=3;auto d=describeHistoryEdit(source,changed);expect(d.summary.contains("Pattern")&&d.detail.contains("Verse")&&d.detail.contains("Chorus")&&d.detail.contains("colour"),"renamed pattern history remains searchable by old and new name");
    changed=source;changed.tracks.erase(changed.tracks.begin()+7);changed.patterns[0].lanes.resize(7);d=describeHistoryEdit(source,changed);expect(d.detail.contains("Removed or replaced: Vault lead"),"deleted channel names remain in the journal description");
    changed=source;changed.patterns.push_back(changed.patterns[0]);changed.patterns.back().editorId=Uuid().toString();changed.patterns.back().name="Bridge";d=describeHistoryEdit(source,changed);expect(d.detail.contains("Added Bridge"),"new patterns are identified without comparing unrelated lane indices");
    changed=source;changed.tracks[7].samplePath="D:/Samples/Low note.wav";changed.tracks[7].sampleRoot=48;expect(describeHistoryEdit(source,changed).detail.contains("Low note.wav"),"sampler source and root tuning changes appear in searchable history");
    auto clips=source;clips.clips.push_back({7,0,0,8});Clip automation;automation.track=7;automation.automationTarget=0;automation.automationTrack=7;automation.points={{0,0,false},{8,1,false}};clips.clips.push_back(automation);changed=clips;changed.clips[1].points[0].curve=AutomationCurve::smooth;expect(describeHistoryEdit(clips,changed).summary=="Automation","outgoing automation shape edits have a distinct history category");
    changed=clips;changed.clips[0].fadeIn=1;changed.clips[0].fadeInCurve=FadeCurve::sCurve;expect(describeHistoryEdit(clips,changed).summary=="Playlist","clip fade metadata changes are visible in project history");
    changed=source;changed.markers.push_back({"intro","Green intro",0,8,2});expect(describeHistoryEdit(source,changed).detail.contains("Green intro"),"section marker names and positions are searchable");
    auto reordered=source;std::swap(reordered.tracks[0],reordered.tracks[7]);std::swap(reordered.patterns[0].lanes[0],reordered.patterns[0].lanes[7]);expect(describeHistoryEdit(source,reordered).summary=="Channel order","channel reorder matches editor identities rather than attributing sound changes to wrong channels");
    changed=source;changed.notes="private long project text";d=describeHistoryEdit(source,changed);expect(d.summary=="Project notes"&&!d.detail.contains("private long"),"history records text edits without duplicating project prose or raw media");
    changed=source;changed.activePattern=0;expect(describeHistoryEdit(source,changed).summary=="Project checkpoint","unchanged checkpoints remain accurately labelled");
    expect(!projectToJson(source).hasProperty("history"),"session history metadata is not serialized into the project file");
}
