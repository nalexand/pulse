// SPDX-License-Identifier: AGPL-3.0-only
#include "Templates.h"
#include "ProjectAssets.h"
#include <algorithm>
#include <cmath>
namespace pulse {
using namespace juce;
static Project builtIn(const String& id){
    auto p=emptyProject();p.title="Untitled session";
    if(id=="basic")return p;
    if(id=="demo"){p=demoProject();p.songMode=true;return p;}
    auto& pat=p.patterns[0];auto add=[&](int t,double beat,int pitch,double length,float velocity){pat.lanes[(size_t)t].push_back({beat,length,pitch,velocity});};
    if(id=="dance"){
        p.bpm=128;pat.name="Four on the floor";pat.length=4;
        for(int beat=0;beat<4;++beat){add(0,beat,36,.2,.9f);add(2,beat+.5,42,.15,.65f);if(beat%2){add(1,beat,38,.2,.75f);add(3,beat,39,.2,.55f);}add(5,beat+.5,beat<2?45:41,.35,.75f);}
    }else if(id=="hiphop"){
        p.bpm=90;p.swing=.18f;pat.name="Pocket groove";pat.length=4;
        for(double at:{0.,1.5,2.75}){add(0,at,36,.2,.9f);add(5,at+.125,at<2?40:43,.4,.8f);}for(double at:{1.,3.})add(1,at,38,.2,.8f);for(int i=0;i<8;++i)add(2,i*.5,42,.12,i%2?.7f:.45f);
    }else if(id=="ambient"){
        p.bpm=72;pat.name="Quiet chords";pat.length=8;p.tracks[6].attack=.4f;p.tracks[6].release=1.7f;p.tracks[6].mixer.reverb=true;p.tracks[6].mixer.reverbMix=.3f;p.tracks[6].mixer.room=.8f;
        const int roots[]={57,53,60,55};for(int i=0;i<4;++i){for(int interval:{0,3,7})add(6,i*2.,roots[i]+interval,1.9,.6f);add(5,i*2.,roots[i]-24,1.7,.5f);add(7,i*2.+1.5,roots[i]+12,.3,.45f);}
    }else if(id=="routed"){
        const char* names[]={"Drum bus","Music bus","Mix bus"};for(auto* name:names){Track bus;bus.name=name;bus.isBus=true;bus.volume=1;bus.cutoff=20000;p.tracks.push_back(bus);}
        for(int i=0;i<8;++i)p.tracks[(size_t)i].mixer.output=i<5?8:9;p.tracks[8].mixer.output=10;p.tracks[9].mixer.output=10;pat.lanes.resize(p.tracks.size());
    }
    return p;
}
static void describe(ProjectTemplate& info,const Project& p){info.bpm=p.bpm;info.tracks=(int)p.tracks.size();info.patterns=(int)p.patterns.size();info.clips=(int)p.clips.size();info.notes=0;for(auto& pat:p.patterns)for(auto& lane:pat.lanes)info.notes+=(int)lane.size();}
std::vector<ProjectTemplate> builtInProjectTemplates(){
    const char* ids[]={"basic","dance","hiphop","ambient","routed","demo"};
    const char* names[]={"Basic instruments","Four on the floor","Hip-hop pocket","Ambient chords","Routed mix","Midnight motion demo"};
    const char* descriptions[]={"Eight original instruments and an empty pattern. Start with the familiar Channel rack.","A 128 BPM kick, backbeat, offbeat hats and bass pattern. All notes are editable.","A 90 BPM rhythm with swing, syncopated kick and bass, snare and hats.","Slow minor chords, bass and sparse plucks with stereo reverb. Adjust the notes in Piano roll.","An empty session with eleven inserts: instruments feed Drum and Music buses, then a shared Mix bus. All buses start at unity.","The original demo, with four patterns and an arranged song. Explore it or use it as a starting point."};
    const char* categories[]={"Essentials","Rhythms","Rhythms","Composition","Mixing","Examples"};
    std::vector<ProjectTemplate> result;for(int i=0;i<6;++i){ProjectTemplate info;info.id="builtin:"+String(ids[i]);info.name=names[i];info.description=descriptions[i];info.category=categories[i];info.builtIn=true;describe(info,builtIn(ids[i]));result.push_back(std::move(info));}return result;
}
static bool packageName(const File& directory){auto name=directory.getFileName().toLowerCase();return name.length()==32&&name.containsOnly("0123456789abcdef");}
Result readProjectTemplate(const File& directory,ProjectTemplate& out){
    if(!packageName(directory)||directory.isSymbolicLink()||!directory.isDirectory())return Result::fail("Invalid template folder.");
    auto manifest=directory.getChildFile("template.json");if(!manifest.existsAsFile()||manifest.getSize()>16384||!directory.getChildFile("project.pulse").existsAsFile())return Result::fail("Template is incomplete.");
    auto json=JSON::parse(manifest.loadFileAsString());if(!json.isObject()||(int)json["schema"]!=1)return Result::fail("Unsupported template metadata.");
    ProjectTemplate next;next.id="local:"+directory.getFileName();next.directory=directory;next.name=json["name"].toString().trim().substring(0,80);next.description=json["description"].toString().substring(0,1000);next.category="Personal";
    if(next.name.isEmpty())return Result::fail("Template name is empty.");int content=(int)json["content"];if(content<0||content>1)return Result::fail("Unsupported template content mode.");next.content=(TemplateContent)content;
    auto tempo=json["bpm"];next.bpm=tempo.isDouble()||tempo.isInt()?(double)tempo:118;if(!std::isfinite(next.bpm)||next.bpm<30||next.bpm>300)next.bpm=118;
    next.tracks=jlimit(1,maxTracks,(int)json["tracks"]);next.patterns=jlimit(1,maxPatterns,(int)json["patterns"]);next.clips=jlimit(0,maxClips,(int)json["clips"]);next.notes=jlimit(0,maxTracks*maxPatterns*maxNotesPerLane,(int)json["notes"]);out=std::move(next);return Result::ok();
}
Result loadProjectTemplate(const ProjectTemplate& info,Project& out){
    Project next;
    if(info.builtIn){bool valid=false;for(auto& item:builtInProjectTemplates())valid|=item.id==info.id;if(!valid)return Result::fail("Unknown built-in template.");next=builtIn(info.id.fromFirstOccurrenceOf(":",false,false));}
    else{ProjectTemplate fresh;auto result=readProjectTemplate(info.directory,fresh);if(result.failed())return result;if(fresh.id!=info.id)return Result::fail("Template folder changed. Refresh the template list.");result=loadProject(info.directory.getChildFile("project.pulse"),next);if(result.failed())return result;next=absoluteAssets(next,info.directory);}
    Project validated;auto result=projectFromJson(projectToJson(next),validated);if(result.failed())return result;out=std::move(validated);return Result::ok();
}
std::vector<ProjectTemplate> scanProjectTemplates(const File& library,String& summary,const std::atomic<bool>* cancel){
    auto result=builtInProjectTemplates();std::vector<ProjectTemplate> personal;int visited=0,invalid=0;bool limited=false;
    if(library.isDirectory())for(auto& entry:RangedDirectoryIterator(library,false,"*",File::findDirectories,File::FollowSymlinks::no)){
        if(cancel&&cancel->load()){summary="Template scan cancelled.";return result;}if(++visited>2000||personal.size()>=500){limited=true;break;}auto folder=entry.getFile();if(!packageName(folder))continue;ProjectTemplate info;auto status=readProjectTemplate(folder,info);if(status.wasOk())personal.push_back(std::move(info));else ++invalid;
    }
    std::sort(personal.begin(),personal.end(),[](const ProjectTemplate& a,const ProjectTemplate& b){int order=a.name.compareNatural(b.name);return order?order<0:a.id<b.id;});result.insert(result.end(),personal.begin(),personal.end());summary=String((int)personal.size())+" personal templates; 6 built-in.";if(invalid)summary+=" Skipped "+String(invalid)+" incomplete or invalid packages.";if(limited)summary+=" Library scan limit reached.";return result;
}
Result saveProjectTemplate(const Project& source,const File& base,const File& library,const String& name,const String& description,TemplateContent content,ProjectTemplate& saved){
    auto cleanName=name.trim().substring(0,80);if(cleanName.isEmpty())return Result::fail("Enter a template name.");if(content!=TemplateContent::complete&&content!=TemplateContent::channelsAndMixer)return Result::fail("Unsupported template content mode.");
    auto p=source;if(content==TemplateContent::channelsAndMixer){Pattern empty;empty.name="Pattern 01";empty.lanes.resize(p.tracks.size());p.patterns={std::move(empty)};p.clips.clear();p.markers.clear();p.loopSection.clear();p.activePattern=0;p.songMode=false;p.notes.clear();p.recording.punch=false;p.recording.punchStart=0;p.recording.punchEnd=8;}
    Project validated;auto result=projectFromJson(projectToJson(p),validated);if(result.failed())return result;
    auto references=projectAssets(validated,base);inspectProjectAssets(references,validated);for(auto& row:references)if(row.issue!=AssetIssue::ready&&row.issue!=AssetIssue::emptySampler)return Result::fail(row.file.getFileName()+": "+row.detail);
    result=library.createDirectory();if(result.failed())return result;auto id=Uuid().toString();auto staging=library.getChildFile(".pending-"+id),target=library.getChildFile(id);result=staging.createDirectory();if(result.failed())return result;
    Project collected;result=collectAssets(validated,base,staging.getChildFile("project.pulse"),collected);if(result.failed())return result;result=saveProject(collected,staging.getChildFile("project.pulse"));if(result.failed())return result;
    ProjectTemplate info;info.id="local:"+id;info.directory=target;info.name=cleanName;info.description=description.substring(0,1000);info.category="Personal";info.content=content;describe(info,collected);
    auto* object=new DynamicObject();object->setProperty("schema",1);object->setProperty("name",info.name);object->setProperty("description",info.description);object->setProperty("content",(int)content);object->setProperty("bpm",info.bpm);object->setProperty("tracks",info.tracks);object->setProperty("patterns",info.patterns);object->setProperty("clips",info.clips);object->setProperty("notes",info.notes);object->setProperty("createdAt",Time::getCurrentTime().toISO8601(true));
    if(!staging.getChildFile("template.json").replaceWithText(JSON::toString(var(object),false)))return Result::fail("Could not write template metadata.");
    // Publish the complete package in one rename. Interrupted staging folders
    // remain outside the visible library; the source project is never touched.
    if(target.exists()||!staging.moveFileTo(target))return Result::fail("Could not commit the template package.");saved=std::move(info);return Result::ok();
}
}
