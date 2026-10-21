// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
namespace pulse {
using namespace juce;
namespace {
Project notationFixture(){auto p=emptyProject();p.title="Notation snapshot";p.bpm=120;p.patterns[0].name="Theme & variation";p.patterns[0].length=32;p.patterns[0].lanes[7]={{0,1,60},{0,1,64},{0,1,67},{1,.5,62},{1.5,.5,63},{2,.75,64},{2.75,.25,65},{3.75,1.5,69},{6,2,72},{8,2,67},{10,2,64},{12,4,60}};p.patterns[0].lanes[5]={{0,4,48},{4,4,43},{8,4,45},{12,4,41}};return p;}
AlertWindow* notationDialog(){auto* d=dynamic_cast<AlertWindow*>(Component::getCurrentlyModalComponent());return d&&d->getName()=="Pattern notation"?d:nullptr;}
Component* notationForm(){auto* d=notationDialog();return d?d->findChildWithID("notation-view"):nullptr;}
Component* notationField(const char* id){auto* c=notationForm();return c?c->findChildWithID(id):nullptr;}
void closeNotation(){if(auto* d=notationDialog())d->exitModalState(0);}
void notationSnapshot(const File& file){if(auto* d=notationDialog()){FileOutputStream out(file);if(out.openedOk())PNGImageFormat().writeImageToStream(d->createComponentSnapshot(d->getLocalBounds()),out);}}
String notationReport(){auto* report=dynamic_cast<TextEditor*>(notationField("notation-report"));return report?report->getText():String();}
bool notationInk(){auto* viewport=dynamic_cast<Viewport*>(notationField("notation-viewport"));if(!viewport||!viewport->getViewedComponent())return false;auto* page=viewport->getViewedComponent();auto image=page->createComponentSnapshot(page->getLocalBounds());int ink=0;for(int y=0;y<image.getHeight();y+=2)for(int x=0;x<image.getWidth();x+=2)ink+=image.getPixelAt(x,y).getBrightness()<.4f;return ink>150;}
}
void Studio::runNotationChecks(const File& folder,const std::function<void(bool,const String&)>& check){
    auto saved=project;auto undo=undoStack,redo=redoStack;auto rev=revision;bool wasDirty=dirty;int track=selectedTrack;auto clef=notationClef;perform(stop);cancelNoteTool();closeCommandPalette();project=notationFixture();selectedTrack=7;notationClef=ScoreClef::automatic;publish();undoStack.clear();redoStack.clear();auto original=JSON::toString(projectToJson(project));auto expected=revision;
    check(!searchCommands(paletteEntries(),"pattern notation").empty(),"Ctrl+P discovers native pattern notation");showNotation();
    auto* channels=dynamic_cast<ComboBox*>(notationField("notation-channel"));auto* clefs=dynamic_cast<ComboBox*>(notationField("notation-clef"));auto* report=dynamic_cast<TextEditor*>(notationField("notation-report"));
    check(notationDialog()&&channels&&clefs&&report&&report->isReadOnly(),"notation opens actual channel/clef selectors and a read-only timing report");
    check(channels&&channels->getSelectedId()==8&&notationReport().contains("12 notes")&&notationReport().contains("Treble"),"score defaults to current Piano-roll channel and automatic clef");
    check(notationInk(),"standalone SVG renders real staff/notes geometry in native scrollable sheet");notationSnapshot(folder.getChildFile("notation-treble.png"));
    if(channels){channels->setSelectedId(6,dontSendNotification);channels->onChange();}
    check(notationReport().contains("4 notes")&&notationReport().contains("Bass"),"switching score channel shows its own notes and register");
    if(clefs){clefs->setSelectedItemIndex(1,dontSendNotification);clefs->onChange();}
    check(notationReport().contains("Treble"),"manual treble clef rebuilds written staff without transposing source pitches");
    if(clefs){clefs->setSelectedItemIndex(2,dontSendNotification);clefs->onChange();}
    check(notationReport().contains("Bass")&&notationInk(),"manual bass clef remains a real rendered score");notationSnapshot(folder.getChildFile("notation-bass.png"));
    auto* viewport=dynamic_cast<Viewport*>(notationField("notation-viewport"));if(viewport)viewport->setViewPosition(0,10000);
    check(viewport&&viewport->getViewPositionY()>0,"longer scores scroll to later systems");closeNotation();
    check(!notationDialog()&&selectedTrack==7&&JSON::toString(projectToJson(project))==original&&revision==expected&&undoStack.empty(),"notation selection, clef and closing retain project selection, revision and Undo");
    PatternScore score;check(buildPatternScore(project,0,7,ScoreClef::treble,score).wasOk(),"export captures explicit pattern/channel notation snapshot");auto xml=folder.getChildFile("native-notation.musicxml"),svg=folder.getChildFile("native-notation.svg");
    check(exportNotation(xml,score,false,expected)&&XmlDocument::parse(xml)!=nullptr,"native MusicXML export writes a verified local score");check(exportNotation(svg,score,true,expected)&&XmlDocument::parse(svg)!=nullptr,"native SVG export writes the standalone staff geometry");
    auto previous=xml.loadFileAsString();check(!exportNotation(xml,score,false,expected-1)&&xml.loadFileAsString()==previous,"stale notation cannot overwrite a prior export");
    recording=true;check(!exportNotation(xml,score,false,expected)&&xml.loadFileAsString()==previous,"active audio/typing take protects notation export");recording=false;
    project.tracks[7].mute=true;showNotation();check(notationReport().contains("12 notes"),"notation displays authored notes even on muted channels");closeNotation();project.tracks[7].mute=false;
    project.patterns[0].lanes[7]={{0,1,11}};showNotation();check(notationReport().contains("C0-G9"),"unsupported MusicXML octave is reported instead of shifted or dropped");closeNotation();project=notationFixture();
    selectedTrack=0;showNotation();check(notationReport().contains("4 notes")&&selectedTrack==0,"percussion selection falls back to pitched score without changing actual channel selection");closeNotation();selectedTrack=7;
    project.patterns[0].lanes[7].clear();showNotation();check(notationReport().contains("0 notes")&&notationInk(),"empty channels show a valid rest-padded staff");closeNotation();project=notationFixture();
    engine->start();AudioBuffer<float> audio(2,4096);getNextAudioBlock(AudioSourceChannelInfo(&audio,0,4096));double position=engine->position.load();showNotation();getNextAudioBlock(AudioSourceChannelInfo(&audio,0,4096));check(engine->playing.load()&&engine->position.load()>position&&audio.getMagnitude(0,0,4096)>.001f,"read-only notation allows ordinary native audio playback to continue");closeNotation();perform(stop);
    check(JSON::toString(projectToJson(project))==original&&revision==expected&&undoStack.empty(),"notation view and both file exports never alter song notes, sounds or history");
    project=std::move(saved);undoStack=std::move(undo);redoStack=std::move(redo);revision=rev;dirty=wasDirty;selectedTrack=track;notationClef=clef;publish();
}
void Studio::runNotationTransportTick(){
    auto put=[&](const char* key,bool ok){transportResults.getDynamicObject()->setProperty(key,ok);};
    if(transportTicks==7200){perform(stop);cancelNoteTool();project=notationFixture();selectedTrack=7;notationClef=ScoreClef::automatic;publish();undoStack.clear();redoStack.clear();transportResults.getDynamicObject()->setProperty("notationOriginal",JSON::toString(projectToJson(project)));transportResults.getDynamicObject()->setProperty("notationRevision",(int64)revision);engine->start();showNotation();put("nativeNotationControls",notationField("notation-channel")&&notationField("notation-clef"));notationSnapshot(transportReport.getSiblingFile("native-notation-treble.png"));}
    if(transportTicks==7210){put("nativeNotationStaff",notationInk()&&notationReport().contains("12 notes"));if(auto* c=dynamic_cast<ComboBox*>(notationField("notation-channel")))c->setSelectedId(6);}
    if(transportTicks==7220){put("nativeNotationChannel",notationReport().contains("4 notes")&&notationReport().contains("Bass"));if(auto* c=dynamic_cast<ComboBox*>(notationField("notation-clef")))c->setSelectedItemIndex(1);}
    if(transportTicks==7230){put("nativeNotationClef",notationReport().contains("Treble")&&selectedTrack==7);notationSnapshot(transportReport.getSiblingFile("native-notation-bass-treble-clef.png"));put("nativeNotationPlayback",engine->playing.load()&&engine->position.load()>.5&&engine->observedPeak.load()>.001f);closeNotation();}
    if(transportTicks==7240){PatternScore score;auto valid=buildPatternScore(project,0,7,ScoreClef::treble,score);auto xml=transportReport.getSiblingFile("native-notation.musicxml"),svg=transportReport.getSiblingFile("native-notation.svg");put("nativeNotationXml",valid.wasOk()&&exportNotation(xml,score,false,revision)&&XmlDocument::parse(xml)!=nullptr);put("nativeNotationSvg",exportNotation(svg,score,true,revision)&&XmlDocument::parse(svg)!=nullptr);auto old=xml.loadFileAsString();put("nativeNotationStale",!exportNotation(xml,score,false,revision-1)&&xml.loadFileAsString()==old);recording=true;put("nativeNotationRecording",!exportNotation(xml,score,false,revision)&&xml.loadFileAsString()==old);recording=false;put("nativeNotationReadOnly",JSON::toString(projectToJson(project))==transportResults["notationOriginal"].toString()&&revision==(uint64_t)(int64)transportResults["notationRevision"]&&undoStack.empty());put("nativeNotationClose",!notationDialog());perform(stop);selectedTrack=0;showNotation();}
    if(transportTicks==7250){put("nativeNotationFallback",notationReport().contains("4 notes")&&selectedTrack==0);closeNotation();selectedTrack=7;perform(stop);}
}
}
