// SPDX-License-Identifier: AGPL-3.0-only
#include "Harmony.h"
#include "NoteTools.h"
#include <juce_cryptography/juce_cryptography.h>
#include <cmath>
#include <limits>
namespace pulse {
using namespace juce;
namespace {
const std::vector<std::vector<int>> scales={
    {0,2,4,5,7,9,11},{0,2,3,5,7,8,10},{0,2,3,5,7,9,10},{0,1,3,5,7,8,10},
    {0,2,4,6,7,9,11},{0,2,4,5,7,9,10},{0,1,3,5,6,8,10},{0,2,3,5,7,8,11},
    {0,2,3,5,7,9,11},{0,2,4,7,9},{0,3,5,7,10},{0,3,5,6,7,10},{0,1,2,3,4,5,6,7,8,9,10,11}};
const std::vector<std::vector<int>> chords={{0,4,7},{0,3,7},{0,3,6},{0,4,8},{0,2,7},{0,5,7},{0,7},{0,4,7,11},{0,3,7,10},{0,4,7,10},{0,3,6,10},{0,3,6,9},{0,3,7,11}};
Result validSource(const std::vector<Note>& source,double length,const std::set<int>& selection={}){NoteToolOptions identity;identity.positionStrength=0;std::vector<Note> checked;return processNotes(source,length,selection,identity,checked);}
}
String harmonyPitchClass(int pitch){static const char* names[]={"C","C# / Db","D","D# / Eb","E","F","F# / Gb","G","G# / Ab","A","A# / Bb","B"};return pitch>=0&&pitch<12?names[pitch]:"Unknown";}
String harmonyScaleName(int scale){static const char* names[]={"Major (Ionian)","Natural minor (Aeolian)","Dorian","Phrygian","Lydian","Mixolydian","Locrian","Harmonic minor","Melodic minor (ascending)","Major pentatonic","Minor pentatonic","Minor blues","Chromatic"};return scale>=0&&scale<harmonyScaleCount?names[scale]:"Unknown";}
String chordQualityName(int quality){static const char* names[]={"Major","Minor","Diminished","Augmented","Suspended 2","Suspended 4","Power fifth","Major seventh","Minor seventh","Dominant seventh","Half-diminished seventh","Diminished seventh","Minor-major seventh","Scale triad (seven-note scales)","Scale seventh (seven-note scales)"};return quality>=0&&quality<chordQualityCount?names[quality]:"Unknown";}
Result validateHarmonyGuide(const HarmonyGuide& guide){return guide.root>=0&&guide.root<12&&guide.scale>=0&&guide.scale<harmonyScaleCount?Result::ok():Result::fail("Choose a tonic C-B and a supported scale.");}
bool harmonyContains(int pitch,const HarmonyGuide& guide){if(pitch<0||pitch>127||validateHarmonyGuide(guide).failed())return false;int degree=(pitch%12-guide.root+12)%12;const auto& intervals=scales[(size_t)guide.scale];return std::find(intervals.begin(),intervals.end(),degree)!=intervals.end();}
Result fitNotesToScale(const std::vector<Note>& source,double length,const std::set<int>& selection,const HarmonyGuide& guide,ScaleDirection direction,std::vector<Note>& output){
    auto valid=validateHarmonyGuide(guide);if(valid.failed())return valid;if(direction!=ScaleDirection::nearest&&direction!=ScaleDirection::down&&direction!=ScaleDirection::up)return Result::fail("Choose Nearest, Down or Up for scale fitting.");valid=validSource(source,length,selection);if(valid.failed())return valid;auto next=source;
    for(int i=0;i<(int)source.size();++i){if(!selection.empty()&&!selection.count(i))continue;auto& note=next[(size_t)i];if(harmonyContains(note.pitch,guide))continue;int target=-1;
        for(int distance=1;distance<=12&&target<0;++distance){if(direction!=ScaleDirection::up&&harmonyContains(note.pitch-distance,guide))target=note.pitch-distance;if(target<0&&direction!=ScaleDirection::down&&harmonyContains(note.pitch+distance,guide))target=note.pitch+distance;}
        if(target<0)return Result::fail("Scale fit would exceed the piano pitch range. Use Nearest or the opposite direction.");note.pitch=target;
    }output=std::move(next);return Result::ok();
}
Result buildChord(const std::vector<Note>& source,double length,const HarmonyGuide& guide,const ChordOptions& options,std::vector<Note>& output){
    auto valid=validSource(source,length);if(valid.failed())return valid;valid=validateHarmonyGuide(guide);if(valid.failed())return valid;
    if(options.root<0||options.root>127||options.quality<0||options.quality>=chordQualityCount||options.octaves<1||options.octaves>3||options.inversion<0||!std::isfinite(options.beat)||options.beat<0||options.beat>=length||!std::isfinite(options.length)||options.length<.01||options.length>64||options.beat+options.length>length+1e-9||!std::isfinite(options.velocity)||options.velocity<=0||options.velocity>1)return Result::fail("Invalid chord settings. Keep every note within this pattern and the piano pitch range.");
    std::vector<int> intervals;
    if(options.quality<13)intervals=chords[(size_t)options.quality];else{const auto& scale=scales[(size_t)guide.scale];if(scale.size()!=7||!harmonyContains(options.root,guide))return Result::fail("Scale chords need a root inside a seven-note scale. Choose a fixed chord quality for pentatonic/blues/chromatic scales.");int degree=(options.root%12-guide.root+12)%12;auto found=std::find(scale.begin(),scale.end(),degree);int rootIndex=(int)std::distance(scale.begin(),found);for(int i=0;i<(options.quality==13?3:4);++i){int index=rootIndex+i*2;intervals.push_back(scale[(size_t)(index%7)]+12*(index/7)-degree);}}
    if(options.inversion>=(int)intervals.size())return Result::fail("This chord has fewer inversions. Choose Root position or a lower inversion.");
    for(int i=0;i<options.inversion;++i)intervals[(size_t)i]+=12;std::sort(intervals.begin(),intervals.end());auto next=source;
    for(int octave=0;octave<options.octaves;++octave)for(int interval:intervals){int pitch=options.root+interval+octave*12;if(pitch>127)return Result::fail("A chord tone would exceed pitch 127. Lower its root, inversion or octave layers.");Note note{options.beat,options.length,pitch,options.velocity};if(options.skipExact&&std::find(next.begin(),next.end(),note)!=next.end())continue;if(next.size()>=maxNotesPerLane)return Result::fail("Chord exceeds 512 notes on this channel. Existing notes are retained.");next.push_back(note);}
    output=std::move(next);return Result::ok();
}
Result parseProgression(const String& text,std::vector<int>& output){
    if(text.length()>256)return Result::fail("Use at most sixteen scale degrees (1-7 or I-VII).");
    StringArray tokens;tokens.addTokens(text," ,;-\t\r\n","");tokens.removeEmptyStrings();
    if(tokens.isEmpty()||tokens.size()>16)return Result::fail("Enter one to sixteen scale degrees (1-7 or I-VII).");
    std::vector<int> next;const char* roman[]={"I","II","III","IV","V","VI","VII"};
    for(const auto& token:tokens){int degree=0;if(token.length()==1&&token[0]>='1'&&token[0]<='7')degree=token.getIntValue();else for(int i=0;i<7;++i)if(token.equalsIgnoreCase(roman[i]))degree=i+1;if(!degree)return Result::fail("Use scale degrees 1-7 or I-VII, separated by spaces, commas or dashes. Chord quality follows the chosen scale.");next.push_back(degree);}
    output=std::move(next);return Result::ok();
}
String progressionDegrees(const std::vector<int>& degrees){StringArray result;for(int degree:degrees)result.add(String(degree));return result.joinIntoString(" - ");}
Result buildProgression(const std::vector<Note>& source,double length,const HarmonyGuide& guide,const ProgressionOptions& options,std::vector<Note>& output){
    auto valid=validSource(source,length);if(valid.failed())return valid;valid=validateHarmonyGuide(guide);if(valid.failed())return valid;
    const auto& scale=scales[(size_t)guide.scale];if(scale.size()!=7)return Result::fail("Progressions need a seven-note scale. Choose Major, Minor or a seven-note mode in Scale settings.");
    if(options.degrees.empty()||options.degrees.size()>16||options.octave<0||options.octave>7||!std::isfinite(options.beat)||options.beat<0||!std::isfinite(options.step)||options.step<.0625||options.step>64||!std::isfinite(options.gate)||options.gate<.05||options.gate>1||options.step*options.gate<.01||!std::isfinite(options.velocity)||options.velocity<=0||options.velocity>1)return Result::fail("Invalid progression settings. Use 1-16 degrees, octave 0-7, step 0.0625-64 beats, gate 5-100% and velocity 1-100%.");
    for(int degree:options.degrees)if(degree<1||degree>7)return Result::fail("Progression degrees must be 1-7.");
    if(options.beat+options.step*options.degrees.size()>length+1e-9)return Result::fail("The complete progression must fit inside this pattern. Shorten the chord step, move the start or extend the pattern first.");
    // The first chord is in root position. Later closed voicings minimize the sum
    // of sorted-voice movements from the previous chord, within a fixed register.
    const int tonic=12*(options.octave+1)+guide.root,voices=options.sevenths?4:3;
    const int low=jmax(0,tonic-12),high=jmin(127,tonic+24);
    std::vector<Note> next=source;std::vector<int> previous;
    for(size_t chord=0;chord<options.degrees.size();++chord){
        int degree=options.degrees[chord]-1;std::vector<int> root;
        for(int voice=0;voice<voices;++voice){int index=degree+voice*2;int pitch=tonic+scale[(size_t)(index%7)]+12*(index/7);if(pitch>127)return Result::fail("A progression tone exceeds pitch 127. Lower the octave.");root.push_back(pitch);}
        auto chosen=root;
        if(options.voiceLeading&&!previous.empty()){
            int best=std::numeric_limits<int>::max(),bestCentre=std::numeric_limits<int>::max();chosen.clear();
            for(int inversion=0;inversion<voices;++inversion)for(int shift=-24;shift<=24;shift+=12){auto candidate=root;for(int i=0;i<voices;++i)candidate[(size_t)i]+=shift+(i<inversion?12:0);std::sort(candidate.begin(),candidate.end());if(candidate.front()<low||candidate.back()>high)continue;int movement=0,centre=0;for(int i=0;i<voices;++i){movement+=std::abs(candidate[(size_t)i]-previous[(size_t)i]);centre+=std::abs(candidate[(size_t)i]-root[(size_t)i]);}if(movement<best||(movement==best&&(centre<bestCentre||(centre==bestCentre&&candidate<chosen)))){best=movement;bestCentre=centre;chosen=std::move(candidate);}}
            if(chosen.empty())return Result::fail("No complete voicing fits this register. Lower the octave or disable voice leading.");
        }
        for(int pitch:chosen){Note note{options.beat+chord*options.step,options.step*options.gate,pitch,options.velocity};if(options.skipExact&&std::find(next.begin(),next.end(),note)!=next.end())continue;if(next.size()>=maxNotesPerLane)return Result::fail("Progression exceeds 512 notes on this channel. Existing notes are retained.");next.push_back(note);}previous=std::move(chosen);
    }
    output=std::move(next);return Result::ok();
}
String harmonyGuideToken(const File& file){if(!file.exists())return "missing";if(!file.existsAsFile()||file.getSize()>4096)return "unavailable";return SHA256(file).toHexString();}
Result loadHarmonyGuide(const File& file,HarmonyGuide& output){if(!file.exists()){output={};return Result::ok();}if(!file.existsAsFile()||file.getSize()>4096)return Result::fail("Harmony guide preferences are unavailable or oversized.");auto json=JSON::parse(file.loadFileAsString());if(!json.isObject()||!json["schema"].isInt()||(int)json["schema"]!=1||!json["root"].isInt()||!json["scale"].isInt()||!json["enabled"].isBool())return Result::fail("Invalid harmony guide preferences. Existing file retained.");HarmonyGuide next{(int)json["root"],(int)json["scale"],(bool)json["enabled"]};auto valid=validateHarmonyGuide(next);if(valid.failed())return valid;output=next;return Result::ok();}
Result saveHarmonyGuide(const File& file,const HarmonyGuide& guide,const String& expected){auto valid=validateHarmonyGuide(guide);if(valid.failed())return valid;auto token=harmonyGuideToken(file);if(expected.isNotEmpty()&&expected!=token)return Result::fail("Harmony preferences changed. Reopen Scale settings.");HarmonyGuide previous;valid=loadHarmonyGuide(file,previous);if(valid.failed())return valid;auto* json=new DynamicObject();json->setProperty("schema",1);json->setProperty("root",guide.root);json->setProperty("scale",guide.scale);json->setProperty("enabled",guide.enabled);valid=file.getParentDirectory().createDirectory();if(valid.failed())return valid;TemporaryFile temporary(file);if(!temporary.getFile().replaceWithText(JSON::toString(var(json),false)))return Result::fail("Cannot write harmony preferences.");if(harmonyGuideToken(file)!=token)return Result::fail("Harmony preferences changed during save. Existing file retained.");if(!temporary.overwriteTargetFileWithTemporary())return Result::fail("Cannot save harmony preferences. Existing file retained.");return Result::ok();}
}
