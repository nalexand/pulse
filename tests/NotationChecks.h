// SPDX-License-Identifier: AGPL-3.0-only
#include "Notation.h"
#include <map>
#include <tuple>
using ScoreTiming=std::tuple<int,int,int>;
static std::vector<ScoreTiming> readScoreTiming(const String& data,bool& balanced){
    auto xml=XmlDocument::parse(data);std::vector<ScoreTiming> result;balanced=xml&&xml->hasTagName("score-partwise");if(!balanced)return result;
    auto* part=xml->getChildByName("part");if(!part){balanced=false;return result;}
    std::map<std::pair<int,int>,size_t> held;int bar=0;
    for(auto* measure:part->getChildIterator())if(measure->hasTagName("measure")){int cursor=0,last=0;
        for(auto* e:measure->getChildIterator()){
            if(e->hasTagName("backup")){cursor-=e->getChildByName("duration")->getAllSubText().getIntValue();balanced&=cursor>=0;}
            if(!e->hasTagName("note"))continue;int duration=e->getChildByName("duration")->getAllSubText().getIntValue(),voice=e->getChildByName("voice")->getAllSubText().getIntValue();
            bool chord=e->getChildByName("chord")!=nullptr;int tick=chord?last:cursor;balanced&=duration>0&&tick>=0&&tick+duration<=16;if(!chord){last=tick;cursor+=duration;}
            auto* p=e->getChildByName("pitch");if(!p)continue;String step=p->getChildByName("step")->getAllSubText();int semitone=step=="C"?0:step=="D"?2:step=="E"?4:step=="F"?5:step=="G"?7:step=="A"?9:11;
            if(auto* a=p->getChildByName("alter"))semitone+=a->getAllSubText().getIntValue();int pitch=12*(p->getChildByName("octave")->getAllSubText().getIntValue()+1)+semitone;bool stop=false,start=false;
            for(auto* tie:e->getChildIterator())if(tie->hasTagName("tie")){stop|=tie->getStringAttribute("type")=="stop";start|=tie->getStringAttribute("type")=="start";}
            auto key=std::make_pair(voice,pitch);int a=bar*16+tick,b=a+duration;
            if(stop){auto it=held.find(key);if(it==held.end()){balanced=false;continue;}balanced&=std::get<1>(result[it->second])==a;std::get<1>(result[it->second])=b;if(!start)held.erase(it);}
            else{result.emplace_back(a,b,pitch);if(start)held[key]=result.size()-1;}
        }balanced&=cursor==16;++bar;
    }balanced&=held.empty();std::sort(result.begin(),result.end());return result;
}
static void testNotation(const File& root){
    auto p=emptyProject();p.title="Score <&> \"test\"";p.bpm=97.5;p.patterns[0].length=8;p.patterns[0].name="Phrase <one>";auto& lane=p.patterns[0].lanes[7];
    lane={{0,1,60},{0,1,64},{0,1,67},{.5,2,72},{1.5,.75,61},{2.25,.25,60},{3.75,1.5,69},{6.125,.4,65},{7.5,2,67},{8,.5,62}};
    auto original=JSON::toString(projectToJson(p));PatternScore score;
    expect(buildPatternScore(p,0,7,ScoreClef::automatic,score).wasOk()&&score.notes==9&&score.voices==2&&score.bars==2&&!score.bass,"pattern score groups chords and allocates overlapping melodic voices");
    expect(score.rounded>0&&score.clipped==1&&score.omitted==1&&score.maxTimingError>0,"notation discloses rounded timing, clipped gates and notes beyond pattern length");
    auto xml=scoreMusicXml(score);bool balanced=false;auto read=readScoreTiming(xml,balanced);
    std::vector<ScoreTiming> wanted={{0,4,60},{0,4,64},{0,4,67},{2,10,72},{6,9,61},{9,10,60},{15,21,69},{25,26,65},{30,32,67}};std::sort(wanted.begin(),wanted.end());
    expect(balanced&&read==wanted,"independent MusicXML counter/tie/chord reader exactly reconstructs declared quantized notes");
    expect(xml.contains("<backup>")&&xml.contains("<chord/>")&&xml.contains("type=\"stop\"")&&xml.contains("type=\"start\""),"MusicXML contains separate voices and sound plus notation ties");
    auto parsed=XmlDocument::parse(xml);expect(parsed&&parsed->getChildByName("work")->getChildByName("work-title")->getAllSubText().contains(p.title),"XML title escaping round-trips punctuation as text");
    auto svg=scoreSvg(score);auto picture=XmlDocument::parse(svg);expect(picture&&picture->hasTagName("svg")&&svg.contains("<ellipse")&&svg.contains("<path")&&!svg.contains("href=")&&!svg.contains("<!DOCTYPE"),"staff SVG is standalone vector geometry without external resources");
    expect(JSON::toString(projectToJson(p))==original,"notation creation, XML and SVG leave all musical project fields intact");
    auto file=root.getChildFile("notation.musicxml"),image=root.getChildFile("notation.svg");expect(savePatternScore(file,score,false).wasOk()&&file.loadFileAsString()==xml&&savePatternScore(image,score,true).wasOk()&&image.loadFileAsString()==svg,"notation exports commit verified local MusicXML and SVG files");
    auto invalid=score;invalid.marks[0].duration=5;auto old=file.loadFileAsString();expect(savePatternScore(file,invalid,false).failed()&&file.loadFileAsString()==old,"invalid score cannot overwrite a previous exported file");
    lane.clear();for(int pitch=12;pitch<=127;++pitch)lane.push_back({(pitch-12)*.25,.25,pitch});p.patterns[0].length=32;
    expect(buildPatternScore(p,0,7,ScoreClef::treble,score).wasOk(),"all MusicXML-supported MIDI pitches can appear in one bounded pattern");read=readScoreTiming(scoreMusicXml(score),balanced);
    expect(balanced&&read.size()==116,"all C0-G9 pitches round-trip through MusicXML sound timing");for(int pitch=12;pitch<=127;++pitch)expect(std::find(read.begin(),read.end(),ScoreTiming{pitch-12,pitch-11,pitch})!=read.end(),"MusicXML pitch spelling and octave preserve exact MIDI semitone");
    lane={{0,.25,61},{.25,.25,60},{4,.25,60},{4.25,.25,61}};p.patterns[0].length=8;buildPatternScore(p,0,7,ScoreClef::treble,score);std::vector<int> accidents;for(auto& m:score.marks)if(!m.rest())accidents.push_back(m.accidental);
    expect(accidents==std::vector<int>({1,0,-1,1}),"accidentals cancel a sharp and reset at the next bar");
    lane={{0,1,60},{0,.5,61},{.5,.5,60}};buildPatternScore(p,0,7,ScoreClef::treble,score);int explicitCount=0;for(auto& m:score.marks)if(m.tick==0&&!m.rest())explicitCount+=m.accidental>=0;
    expect(explicitCount==2,"simultaneous natural/sharp voice clashes receive explicit accidentals on both notes");
    lane={{0,8,48}};buildPatternScore(p,0,7,ScoreClef::automatic,score);expect(score.bass&&score.notes==1&&scoreMusicXml(score).contains("<sign>F</sign>"),"automatic bass clef uses the authored pitch register");
    read=readScoreTiming(scoreMusicXml(score),balanced);expect(balanced&&read==std::vector<ScoreTiming>{{0,32,48}},"long gates tie through multiple measures without retriggered note starts");
    lane.clear();p.patterns[0].length=1.1;expect(buildPatternScore(p,0,7,ScoreClef::automatic,score).wasOk()&&score.notes==0&&score.bars==1,"empty and fractional-length patterns produce a complete rest-padded measure");
    read=readScoreTiming(scoreMusicXml(score),balanced);expect(balanced&&read.empty(),"a blank score contains complete rests and no invented pitches");
    p.patterns[0].length=8;for(int i=0;i<17;++i)lane.push_back({0,(i+1)*.25,60+i});auto sentinel=scoreMusicXml(score);
    expect(buildPatternScore(p,0,7,ScoreClef::automatic,score).failed()&&scoreMusicXml(score)==sentinel,"more than sixteen independent overlaps reject atomically");
    lane={{0,1,11}};expect(buildPatternScore(p,0,7,ScoreClef::automatic,score).failed()&&scoreMusicXml(score)==sentinel,"unsupported negative octave rejects rather than transposing or dropping notes");
    for(int test=0;test<5;++test){lane={{0,1,60}};if(test==0)lane[0].beat=std::numeric_limits<double>::quiet_NaN();if(test==1)lane[0].length=0;if(test==2)lane[0].pitch=128;if(test==3)lane[0].velocity=2;if(test==4)lane[0].conditionEvery=0;expect(buildPatternScore(p,0,7,ScoreClef::automatic,score).failed(),"invalid source notes never reach notation or export");}
    expect(buildPatternScore(p,0,0,ScoreClef::automatic,score).failed()&&buildPatternScore(p,-1,7,ScoreClef::automatic,score).failed()&&buildPatternScore(p,0,7,(ScoreClef)4,score).failed(),"percussion channels, missing patterns and unknown clefs reject");
    lane={{0,.5,60,.8f,.5f,2,2,1}};p.swing=.2f;expect(buildPatternScore(p,0,7,ScoreClef::automatic,score).wasOk()&&score.performance&&score.notes==1&&scoreSummary(score).contains("not rendered"),"score discloses conditional/random/ratchet/swing performance instead of inventing a deterministic realization");
    lane={{0,64,127}};p.patterns[0].length=64;expect(buildPatternScore(p,0,7,ScoreClef::bass,score).wasOk()&&scoreSvg(score).length()>0,"maximum pattern duration and extreme supported pitch retain bounded readable staff geometry");
    Array<var> corpus;bool corpusPassed=true;
    for(int example=0;example<16;++example){Random random(7400+example);p.patterns[0].length=64;lane.clear();std::vector<ScoreTiming> expected;
        for(int voice=0;voice<4;++voice){int tick=voice%2;while(tick<256){int gate=jmin(1+random.nextInt(24),256-tick),pitch=36+voice*12+random.nextInt(12);lane.push_back({tick*.25,gate*.25,pitch});expected.emplace_back(tick,tick+gate,pitch);tick+=gate+random.nextInt(4);}}
        std::sort(expected.begin(),expected.end());bool built=buildPatternScore(p,0,7,example%2?ScoreClef::bass:ScoreClef::treble,score).wasOk();
        bool valid=false;auto observed=built?readScoreTiming(scoreMusicXml(score),valid):std::vector<ScoreTiming>();
        bool same=built&&valid&&observed==expected;expect(same,"generated polyphonic score exactly reconstructs every authored grid note and tie");
        auto target=root.getChildFile("notation-corpus-"+String(example)+".musicxml");bool saved=built&&savePatternScore(target,score,false).wasOk();expect(saved,"generated multi-voice MusicXML fixture saves as a verified local document");
        corpusPassed&=same&&saved;auto* entry=new DynamicObject();entry->setProperty("case",example);entry->setProperty("notes",(int)expected.size());entry->setProperty("voices",score.voices);entry->setProperty("passed",same&&saved);corpus.add(var(entry));
    }
    auto* report=new DynamicObject();report->setProperty("version","0.74.0");report->setProperty("passed",corpusPassed);report->setProperty("cases",corpus);root.getChildFile("notation-evaluation.json").replaceWithText(JSON::toString(var(report),false));

}
