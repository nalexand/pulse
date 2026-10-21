// SPDX-License-Identifier: AGPL-3.0-only
#include "Notation.h"
#include <cmath>
#include <map>
#include <numeric>
namespace pulse {
using namespace juce;
namespace {
int sharp(int p){int pc=p%12;return pc==1||pc==3||pc==6||pc==8||pc==10;}
struct Event {int tick=0,end=1,voice=0;std::vector<int> pitches;};
int durationAt(int tick,int remaining){
    int limit=jmin(remaining,16-tick%16);if(tick%4)limit=jmin(limit,4-tick%4);
    for(int length:{16,12,8,6,4,3,2,1})if(length<=limit)return length;return 1;
}
void addRest(std::vector<ScoreMark>& marks,int first,int end,int voice){
    while(first<end){int duration=durationAt(first,end-first);marks.push_back({first,duration,-1,voice});first+=duration;}
}
XmlElement* text(XmlElement* parent,const char* name,const String& value){auto* e=parent->createNewChildElement(name);e->addTextElement(value);return e;}
}
int scoreDiatonic(int pitch){constexpr int steps[]={0,0,1,1,2,3,3,4,4,5,5,6};return (pitch/12-1)*7+steps[pitch%12];}
const char* scoreNoteType(int n){return n>=16?"whole":n>=8?"half":n>=4?"quarter":n>=2?"eighth":"16th";}
bool scoreDotted(int n){return n==3||n==6||n==12;}
Result buildPatternScore(const Project& p,int pattern,int track,ScoreClef clef,PatternScore& out){
    if(pattern<0||pattern>=(int)p.patterns.size()||track<0||track>=(int)p.tracks.size()
       ||p.tracks[(size_t)track].isBus||isDrum(p.tracks[(size_t)track].instrument)||(int)clef<0||(int)clef>2
       ||!std::isfinite(p.bpm)||p.bpm<30||p.bpm>300)return Result::fail("Choose a pitched instrument channel, valid pattern and clef.");
    const auto& pat=p.patterns[(size_t)pattern];
    if(!std::isfinite(pat.length)||pat.length<1||pat.length>64||pat.lanes.size()!=p.tracks.size()
       ||pat.lanes[(size_t)track].size()>maxNotesPerLane)return Result::fail("Notation requires a 1-64 beat pattern with at most 512 notes per channel.");
    PatternScore next;next.title=p.title.substring(0,256);next.pattern=pat.name.substring(0,256);next.channel=p.tracks[(size_t)track].name.substring(0,256);
    next.bpm=p.bpm;next.length=pat.length;next.bars=(int)std::ceil(pat.length/4);next.performance=p.swing!=0||p.tracks[(size_t)track].tuning.enabled;
    std::map<std::pair<int,int>,std::vector<int>> groups;std::vector<int> pitches;
    for(const auto& n:pat.lanes[(size_t)track]){
        if(!std::isfinite(n.beat)||!std::isfinite(n.length)||n.beat<0||n.beat>64||n.length<=0||n.length>64||n.pitch<0||n.pitch>127
           ||!std::isfinite(n.velocity)||n.velocity<=0||n.velocity>1||!std::isfinite(n.probability)||n.probability<0||n.probability>1
           ||n.ratchets<1||n.ratchets>8||!validNoteCondition(n))return Result::fail("Invalid authored note; notation was not changed.");
        next.performance|=n.probability!=1||n.ratchets!=1||n.conditionEvery!=1;
        if(n.beat>=pat.length){++next.omitted;continue;}
        if(n.pitch<12)return Result::fail("This MusicXML view supports C0-G9 (MIDI 12-127). Notes below C0 cannot be exported without transposition.");
        double finish=jmin(n.beat+n.length,pat.length);next.clipped+=finish<n.beat+n.length;
        int a=jlimit(0,next.bars*16-1,(int)std::llround(n.beat*4)),b=jlimit(a+1,next.bars*16,(int)std::llround(finish*4));
        double error=jmax(std::abs(a*.25-n.beat),std::abs(b*.25-finish));next.maxTimingError=jmax(next.maxTimingError,error);next.rounded+=error>1e-8;
        groups[{a,b}].push_back(n.pitch);pitches.push_back(n.pitch);++next.notes;
    }
    std::vector<Event> events;for(auto& [range,notes]:groups){std::sort(notes.begin(),notes.end());events.push_back({range.first,range.second,0,std::move(notes)});}
    std::stable_sort(events.begin(),events.end(),[](const Event& a,const Event& b){return a.tick!=b.tick?a.tick<b.tick:a.end>b.end;});
    std::vector<int> ends;
    for(auto& e:events){int voice=0;while(voice<(int)ends.size()&&ends[(size_t)voice]>e.tick)++voice;
        if(voice==16)return Result::fail("This score exceeds 16 independent overlapping voices. Simplify the channel for notation.");
        if(voice==(int)ends.size())ends.push_back(0);e.voice=voice;ends[(size_t)voice]=e.end;
    }
    next.voices=jmax(1,(int)ends.size());
    for(int voice=0;voice<next.voices;++voice){int cursor=0;
        for(const auto& e:events)if(e.voice==voice){
            addRest(next.marks,cursor,e.tick,voice);int pos=e.tick;
            while(pos<e.end){int duration=durationAt(pos,e.end-pos);
                for(size_t i=0;i<e.pitches.size();++i)next.marks.push_back({pos,duration,e.pitches[i],voice,-1,i!=0,pos>e.tick,pos+duration<e.end});
                pos+=duration;
            }cursor=e.end;
        }addRest(next.marks,cursor,next.bars*16,voice);
    }
    // Explicit accidental changes per diatonic pitch and measure. Simultaneous
    // natural/sharp clashes are labelled on both notes, then treated as unknown.
    std::vector<size_t> order(next.marks.size());std::iota(order.begin(),order.end(),0);
    std::stable_sort(order.begin(),order.end(),[&](size_t a,size_t b){return next.marks[a].tick<next.marks[b].tick;});
    std::array<int,70> state{};int bar=-1;
    for(size_t i=0;i<order.size();){int tick=next.marks[order[i]].tick;if(tick/16!=bar){bar=tick/16;state.fill(0);}size_t j=i;std::array<int,70> mask{};
        while(j<order.size()&&next.marks[order[j]].tick==tick){auto& m=next.marks[order[j]];if(!m.rest())mask[(size_t)scoreDiatonic(m.pitch)]|=1<<sharp(m.pitch);++j;}
        for(size_t k=i;k<j;++k){auto& m=next.marks[order[k]];if(m.rest())continue;int d=scoreDiatonic(m.pitch),alter=sharp(m.pitch);
            if(!m.tieIn&&(mask[(size_t)d]==3||state[(size_t)d]!=alter))m.accidental=alter;
            state[(size_t)d]=mask[(size_t)d]==3?2:alter;
        }i=j;
    }
    if(!pitches.empty()){std::nth_element(pitches.begin(),pitches.begin()+pitches.size()/2,pitches.end());next.bass=clef==ScoreClef::bass||(clef==ScoreClef::automatic&&pitches[pitches.size()/2]<60);}
    else next.bass=clef==ScoreClef::bass;
    auto valid=validatePatternScore(next);if(valid.wasOk())out=std::move(next);return valid;
}
Result validatePatternScore(const PatternScore& s){
    if(!std::isfinite(s.bpm)||s.bpm<30||s.bpm>300||!std::isfinite(s.length)||s.length<1||s.length>64
       ||s.bars!=(int)std::ceil(s.length/4)||s.voices<1||s.voices>16||s.notes<0||s.notes>512||s.marks.empty()||s.marks.size()>32768
       ||s.title.length()>256||s.pattern.length()>256||s.channel.length()>256)return Result::fail("Invalid notation snapshot.");
    std::array<int,16> ends{},starts{},durations{};
    // Per-voice traversal must cover every bar with rests/notes, without a gap.
    for(const auto& m:s.marks){if(m.voice<0||m.voice>=s.voices||m.tick<0||m.tick>=s.bars*16||m.duration<1||m.duration>16||m.tick%16+m.duration>16
        ||(m.duration!=1&&m.duration!=2&&m.duration!=3&&m.duration!=4&&m.duration!=6&&m.duration!=8&&m.duration!=12&&m.duration!=16)
        ||m.accidental< -1||m.accidental>1||m.pitch< -1||m.pitch>127||(!m.rest()&&m.pitch<12)||m.rest()&&(m.chord||m.tieIn||m.tieOut||m.accidental!=-1))return Result::fail("Invalid score note or rhythm.");
        auto v=(size_t)m.voice;if(m.chord){if(m.rest()||m.tick!=starts[v]||m.duration!=durations[v])return Result::fail("Invalid score chord.");}
        else{if(m.tick!=ends[v])return Result::fail("Score voice contains a gap or overlap.");starts[v]=m.tick;durations[v]=m.duration;ends[v]=m.tick+m.duration;}
    }
    for(int v=0;v<s.voices;++v)if(ends[(size_t)v]!=s.bars*16)return Result::fail("Incomplete score voice.");return Result::ok();
}
String scoreSummary(const PatternScore& s){return String(s.notes)+" notes / "+String(s.bars)+" bars / "+String(s.voices)+" voices / "+(s.bass?"Bass":"Treble")+" clef\n4/4; nearest 1/16 grid. "+String(s.rounded)+" rounded notes; max timing change "+String(s.maxTimingError,4)+" beats. "+String(s.clipped)+" gates clipped at pattern end; "+String(s.omitted)+" starts beyond pattern omitted. Final bar padded with rests.\nRead-only snapshot: no edits or playback changes. Written pitches use sharps; no key inference. Probability, ratchets, conditions, swing, microtuning, effects and performance expression are not rendered into the score. Triplets, engraving-quality beams/layout, percussion, Song/full-score view and MusicXML import remain future work.";}
String scoreMusicXml(const PatternScore& s){
    if(validatePatternScore(s).failed())return {};
    XmlElement root("score-partwise");root.setAttribute("version","4.0");text(root.createNewChildElement("work"),"work-title",s.title+" / "+s.pattern+" / "+s.channel);
    auto* ident=root.createNewChildElement("identification");text(ident->createNewChildElement("encoding"),"software","Pulse Studio");
    auto* note=text(ident->createNewChildElement("miscellaneous"),"miscellaneous-field",scoreSummary(s));note->setAttribute("name","pulse-notation-boundary");
    auto* part=root.createNewChildElement("part-list")->createNewChildElement("score-part");part->setAttribute("id","P1");text(part,"part-name",s.channel);
    part=root.createNewChildElement("part");part->setAttribute("id","P1");
    for(int bar=0;bar<s.bars;++bar){auto* measure=part->createNewChildElement("measure");measure->setAttribute("number",bar+1);
        if(bar==0){auto* a=measure->createNewChildElement("attributes");text(a,"divisions","4");text(a->createNewChildElement("key"),"fifths","0");auto* time=a->createNewChildElement("time");text(time,"beats","4");text(time,"beat-type","4");auto* clef=a->createNewChildElement("clef");text(clef,"sign",s.bass?"F":"G");text(clef,"line",s.bass?"4":"2");
            auto* direction=measure->createNewChildElement("direction");auto* metro=direction->createNewChildElement("direction-type")->createNewChildElement("metronome");text(metro,"beat-unit","quarter");text(metro,"per-minute",String(s.bpm,6));direction->createNewChildElement("sound")->setAttribute("tempo",s.bpm);
        }
        for(int voice=0;voice<s.voices;++voice){if(voice)text(measure->createNewChildElement("backup"),"duration","16");
            for(const auto& m:s.marks)if(m.tick/16==bar&&m.voice==voice){auto* n=measure->createNewChildElement("note");if(m.chord)n->createNewChildElement("chord");
                if(m.rest())n->createNewChildElement("rest");else{auto* pitch=n->createNewChildElement("pitch");static constexpr const char* steps[]={"C","C","D","D","E","F","F","G","G","A","A","B"};text(pitch,"step",steps[m.pitch%12]);if(sharp(m.pitch))text(pitch,"alter","1");text(pitch,"octave",String(m.pitch/12-1));}
                text(n,"duration",String(m.duration));if(m.tieIn)n->createNewChildElement("tie")->setAttribute("type","stop");if(m.tieOut)n->createNewChildElement("tie")->setAttribute("type","start");
                text(n,"voice",String(voice+1));text(n,"type",scoreNoteType(m.duration));if(scoreDotted(m.duration))n->createNewChildElement("dot");if(m.accidental>=0)text(n,"accidental",m.accidental?"sharp":"natural");
                if(m.tieIn||m.tieOut){auto* notation=n->createNewChildElement("notations");if(m.tieIn)notation->createNewChildElement("tied")->setAttribute("type","stop");if(m.tieOut)notation->createNewChildElement("tied")->setAttribute("type","start");}
            }
        }
    }return root.toString();
}
Result savePatternScore(const File& file,const PatternScore& score,bool svg){
    auto valid=validatePatternScore(score);if(valid.failed())return valid;
    auto data=svg?scoreSvg(score):scoreMusicXml(score);if(data.isEmpty()||data.getNumBytesAsUTF8()>4*1024*1024)return Result::fail("Score output is empty or exceeds 4 MiB.");
    auto xml=XmlDocument::parse(data);if(!xml||!xml->hasTagName(svg?"svg":"score-partwise"))return Result::fail("Cannot validate score output.");
    valid=file.getParentDirectory().createDirectory();if(valid.failed())return valid;TemporaryFile temporary(file);
    if(!temporary.getFile().replaceWithData(data.toRawUTF8(),data.getNumBytesAsUTF8()))return Result::fail("Cannot stage score. Previous destination retained.");
    if(temporary.getFile().loadFileAsString()!=data)return Result::fail("Score read-back differs. Previous destination retained.");
    if(!temporary.overwriteTargetFileWithTemporary())return Result::fail("Cannot publish score. Previous destination retained.");return Result::ok();
}
}
