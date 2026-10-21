// SPDX-License-Identifier: AGPL-3.0-only
#include "Notation.h"
#include <cmath>
namespace pulse {
using namespace juce;
String scoreSvg(const PatternScore& s){
    if(validatePatternScore(s).failed())return {};
    int base=s.bass?18:30,lo=base,hi=base+8;
    for(const auto& m:s.marks)if(!m.rest()){lo=jmin(lo,scoreDiatonic(m.pitch));hi=jmax(hi,scoreDiatonic(m.pitch));}
    double bottom=80+jmax(0,hi-base-8)*5,systemHeight=jmax(bottom+50,bottom+(base-lo)*5+50);
    int systems=(s.bars+1)/2;double height=110+systems*systemHeight+75;
    XmlElement svg("svg");svg.setAttribute("xmlns","http://www.w3.org/2000/svg");svg.setAttribute("width",1000);svg.setAttribute("height",height);svg.setAttribute("viewBox","0 0 1000 "+String(height,1));
    auto element=[&](const char* name){return svg.createNewChildElement(name);};
    auto line=[&](double x,double y,double x2,double y2,double width=1.){auto* e=element("line");e->setAttribute("x1",x);e->setAttribute("y1",y);e->setAttribute("x2",x2);e->setAttribute("y2",y2);e->setAttribute("stroke","#20262a");e->setAttribute("stroke-width",width);};
    auto text=[&](double x,double y,const String& value,double size=13.){auto* e=element("text");e->setAttribute("x",x);e->setAttribute("y",y);e->setAttribute("font-family","Arial");e->setAttribute("font-size",size);e->setAttribute("fill","#20262a");e->addTextElement(value);};
    auto ellipse=[&](double x,double y,double rx,double ry,bool fill=true){auto* e=element("ellipse");e->setAttribute("cx",x);e->setAttribute("cy",y);e->setAttribute("rx",rx);e->setAttribute("ry",ry);e->setAttribute("fill",fill?"#20262a":"#fffdf6");e->setAttribute("stroke","#20262a");e->setAttribute("stroke-width",1.2);};
    auto rectangle=[&](double x,double y,double w,double h,const char* fill){auto* e=element("rect");e->setAttribute("x",x);e->setAttribute("y",y);e->setAttribute("width",w);e->setAttribute("height",h);e->setAttribute("fill",fill);};
    auto path=[&](const String& d,double stroke=1.4){auto* e=element("path");e->setAttribute("d",d);e->setAttribute("fill","none");e->setAttribute("stroke","#20262a");e->setAttribute("stroke-width",stroke);};
    auto point=[](double x,double y){return String(x,2)+" "+String(y,2);};
    rectangle(0,0,1000,height,"#fffdf6");text(40,33,s.title.substring(0,95),23);text(40,59,s.pattern.substring(0,50)+" / "+s.channel.substring(0,50),16);
    text(40,81,String(s.bpm,2)+" BPM | 4/4 | 1/16 snapshot | "+String(s.notes)+" notes | "+String(s.voices)+" voices | "+(s.bass?"Bass":"Treble")+" clef");
    for(int system=0;system<systems;++system){double y=110+system*systemHeight,b=y+bottom;
        int bars=jmin(2,s.bars-system*2);double right=120+bars*420;
        for(int i=0;i<5;++i)line(40,b-i*10,right,b-i*10);
        line(40,b-40,40,b);for(int bar=0;bar<=bars;++bar)line(120+bar*420,b-40,120+bar*420,b,bar==bars?1.5:1.);
        if(s.bass){double x=56,cy=b-30;path("M "+point(x-7,cy+2)+" C "+point(x-9,cy-15)+" "+point(x+20,cy-14)+" "+point(x+7,cy+10)+" C "+point(x+3,cy+18)+" "+point(x-4,cy+22)+" "+point(x-9,cy+25),3);ellipse(x-5,cy-3,3,3);ellipse(x+17,cy-5,1.8,1.8);ellipse(x+17,cy+5,1.8,1.8);}
        else{double x=58,cy=b-10;path("M "+point(x+2,cy+26)+" C "+point(x-17,cy+29)+" "+point(x-8,cy+5)+" "+point(x+7,cy+7)+" C "+point(x+26,cy+12)+" "+point(x+23,cy-17)+" "+point(x+3,cy-14)+" C "+point(x-27,cy-8)+" "+point(x+26,cy-37)+" "+point(x+10,cy-57)+" C "+point(x-6,cy-50)+" "+point(x+7,cy-26)+" "+point(x+12,cy+33),2.8);}
        text(91,b-21,"4",20);text(91,b-1,"4",20);
        for(int bar=0;bar<bars;++bar)text(125+bar*420,b-57,String(system*2+bar+1),12);
    }
    for(size_t index=0;index<s.marks.size();++index){if(svg.getNumChildElements()>24000)return {};const auto& m=s.marks[index];int bar=m.tick/16,system=bar/2;double b=110+system*systemHeight+bottom,barX=120+(bar%2)*420,x=barX+24+(m.tick%16)*23.5;
        if(m.rest()){
            if(m.voice!=0)continue;double y=b-20;
            if(m.duration>=8)rectangle(x-5,y+(m.duration>=16?0:-4),10,4,"#20262a");
            else if(m.duration>=4)path("M "+point(x+2,y-13)+" L "+point(x-3,y-5)+" L "+point(x+4,y+1)+" L "+point(x-2,y+6)+" Q "+point(x-8,y+8)+" "+point(x-1,y+14),2.5);
            else{line(x+4,y-7,x-3,y+12,2);ellipse(x,y-6,3,2.5);if(m.duration==1)ellipse(x-2,y+1,3,2.5);}
            if(scoreDotted(m.duration))ellipse(x+11,y,1.5,1.5);continue;
        }
        int step=scoreDiatonic(m.pitch);double y=b-(step-base)*5;
        for(int d=base-2;d>=step;d-=2)line(x-9,b-(d-base)*5,x+9,b-(d-base)*5);
        for(int d=base+10;d<=step;d+=2)line(x-9,b-(d-base)*5,x+9,b-(d-base)*5);
        // Adjacent chord seconds alternate noteheads to avoid complete overlap.
        double headX=x;if(m.chord&&index&&scoreDiatonic(s.marks[index-1].pitch)==step-1)headX+=7;
        ellipse(headX,y,m.duration==16?6:5,3.5,m.duration<8);
        if(m.accidental==1){text(headX-19,y+5,"#",17);}else if(m.accidental==0){line(headX-16,y-9,headX-16,y+5);line(headX-12,y-5,headX-12,y+9);line(headX-16,y-1,headX-12,y-3,2);line(headX-16,y+4,headX-12,y+2,2);}
        if(scoreDotted(m.duration))ellipse(headX+10,y-(step%2==0?3:0),1.5,1.5);
        if(!m.chord&&m.duration<16){double top=y,low=y;for(size_t k=index+1;k<s.marks.size()&&s.marks[k].chord;++k){double cy=b-(scoreDiatonic(s.marks[k].pitch)-base)*5;top=jmin(top,cy);low=jmax(low,cy);}
            bool up=s.voices>1?m.voice%2==0:(top+low)*.5>b-20;double sx=x+(up?5:-5),tip=up?top-30:low+30,from=up?low:top;line(sx,from,sx,tip,1.3);
            if(m.duration<=3)for(int flag=0;flag<(m.duration==1?2:1);++flag){double f=tip+(up?flag*7:-flag*7),direction=up?1:-1;path("M "+point(sx,f)+" C "+point(sx+12,f+direction*6)+" "+point(sx+11,f+direction*12)+" "+point(sx+5,f+direction*17),1.8);}
        }
        if(m.tieOut){double end=jmin(barX+414,x+m.duration*23.5-5);path("M "+point(x+4,y+10)+" Q "+point((x+end)*.5,y+19)+" "+point(end,y+10));}
        if(m.tieIn&&m.tick%16==0)path("M "+point(barX+2,y+10)+" Q "+point(barX+12,y+17)+" "+point(x-5,y+10));
    }
    double foot=height-49;text(40,foot,"Read only: nearest 1/16 timing, authored pitches, rests and ties. Final bar padded with rests.",12);
    text(40,foot+19,"Compact view shows voice-1 rests; MusicXML includes every voice. Probability/ratchets/tuning/expression are not baked.",12);
    text(40,foot+38,"Basic engraving: no beams, tuplets, inferred key, percussion or Song score. Edit the original notes in Piano roll.",12);
    return svg.toString();
}
}
