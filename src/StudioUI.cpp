// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <cmath>

namespace pulse {
using namespace juce;
namespace {
void fill(Graphics& g,float x,float y,float w,float h){g.fillRect(Rectangle<float>(x,y,w,h));}
template<typename T> void fill(Graphics& g,Rectangle<T> r){g.fillRect(r.toFloat());}
const Colour bg(0xff343b3f),surface(0xff434c51),raised(0xff535f65),border(0xff262d31),text(0xffd9ddde),muted(0xffadb7bd),accent(0xffb2c98d),orange(0xffe3aa63),grid(0xff242b2f);
String pitchName(int p){static const char* names[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};return String(names[p%12])+String(p/12-1);}
bool blackKey(int p){int k=p%12;return k==1||k==3||k==6||k==8||k==10;}
}
Colour Studio::colorFor(int t) const {
    static const uint32_t colors[]={0xffb8c397,0xffb6ada1,0xffb9ba92,0xffad9daa,0xffbaa28a,0xff8faeac,0xff9aac87,0xff8da4b6};return Colour(colors[t%8]);
}
void Studio::label(Graphics& g,const String& s,Rectangle<float> r,float size,Colour c,bool bold,int alignment){g.setColour(c);g.setFont(Font(FontOptions("Segoe UI",size,bold?Font::bold:Font::plain)));g.drawText(s,r,alignment,true);}
void Studio::panel(Graphics& g,Rectangle<float> r,Colour c,float){g.setColour(c);fill(g,r);g.setColour(border);g.drawRect(r,1);g.setColour(Colours::white.withAlpha(.08f));g.drawHorizontalLine((int)r.getY()+1,r.getX()+1,r.getRight()-1);}
void Studio::button(Graphics& g,const String& s,Rectangle<float> r,int action,int data,bool active,bool primary){
    Colour c=active?Colour(0xff5c704c):primary?Colour(0xff64764f):raised;
    g.setGradientFill(ColourGradient(c.brighter(.08f),r.getCentreX(),r.getY(),c.darker(.12f),r.getCentreX(),r.getBottom(),false));g.fillRoundedRectangle(r,2);
    g.setColour(border);g.drawRoundedRectangle(r,2,1);g.setColour(Colours::white.withAlpha(.10f));g.drawHorizontalLine((int)r.getY()+1,r.getX()+2,r.getRight()-2);
    label(g,s,r.reduced(3,0),11,active?Colour(0xffd3e1bd):text,false,Justification::centred);hits.push_back({r,action,data});
}
void Studio::rotary(Graphics& g,Rectangle<float> r,int action,float value,int data,Colour color){
    value=liveParameter(action,data,value);
    auto c=r.getCentre();float radius=jmin(r.getWidth(),r.getHeight())*.38f;
    g.setColour(border);g.fillEllipse(c.x-radius-1,c.y-radius-1,2*radius+2,2*radius+2);
    g.setGradientFill(ColourGradient(Colour(0xff69767d),c.x,c.y-radius,Colour(0xff384247),c.x,c.y+radius,false));g.fillEllipse(c.x-radius,c.y-radius,2*radius,2*radius);
    double angle=(-.75+1.5*jlimit(0.f,1.f,value))*MathConstants<double>::pi;
    g.setColour(color);g.drawLine(c.x,c.y,c.x+(float)std::sin(angle)*radius*.76f,c.y-(float)std::cos(angle)*radius*.76f,1.7f);
    hits.push_back({r,action,data});
}
void Studio::dial(Graphics& g,const String& name,Rectangle<float> r,int action,float value,int data){
    rotary(g,{r.getX(),r.getY(),30,30},action,value,data);label(g,name,{r.getX()+37,r.getY(),r.getWidth()-37,30},11,text);hits.push_back({r,action,data});
}
void Studio::paint(Graphics& g){
    if(!layoutReady)resetLayout();g.fillAll(bg);hits.clear();paintHeader(g);paintBrowser(g);
    auto workspace=workspaceBounds();g.setColour(Colour(0xff30373b));fill(g,workspace);
    for(int y=(int)workspace.getY();y<workspace.getBottom();y+=8){g.setColour(Colours::black.withAlpha(.025f));g.drawHorizontalLine(y,workspace.getX(),workspace.getRight());}
    for(int pane:paneOrder)if(panes[(size_t)pane].visible)paintPane(g,pane);
    g.setColour(Colour(0xff293135));fill(g,0,getHeight()-22,getWidth(),22);
    label(g,status,{9.f,(float)getHeight()-22,(float)getWidth()-285,22},11,text);
    String tail=exporting.load()?"Rendering "+String((int)(exportProgress.load()*100))+"%":"Pulse Studio 0.37.0  |  Local session";
    label(g,tail,{(float)getWidth()-270,(float)getHeight()-22,258,22},10,muted,false,Justification::centredRight);
}
void Studio::paintHeader(Graphics& g){
    inputMonitorTrack.store(selectedTrack);
    g.setGradientFill(ColourGradient(Colour(0xff515d63),0,0,Colour(0xff3b454a),0,94,false));fill(g,0,0,getWidth(),94);
    panel(g,{3,3,297,39},Colour(0xff394247));label(g,project.title+(dirty?" *":""),{12,4,250,17},12,text,true);
    label(g,hoverHint.isNotEmpty()?hoverHint:"Pulse Studio - local workstation",{12,22,278,16},10,muted);
    const char* menus[]={"FILE","EDIT","ADD","PATTERNS","VIEW","OPTIONS","TOOLS","HELP"};float menuX=9;
    for(int i=0;i<8;++i){float width=i==3?58.f:i==5?56.f:35.f;label(g,menus[i],{menuX,46,width,18},10,text);hits.push_back({{menuX,45,width,22},menuOpen,i});menuX+=width;}
    rotary(g,{307,5,34,35},masterDial,project.master,0,orange);label(g,"VOL",{305,40,39,12},8,muted,false,Justification::centred);
    Rectangle<float> modeRect{353,5,36,38};panel(g,modeRect,Colour(0xff303a3f));
    g.setColour(!project.songMode?Colour(0xff728552):Colour(0xff455048));fill(g,modeRect.getX()+2,modeRect.getY()+2,32,16);
    label(g,"PAT",{355,7,32,15},9,!project.songMode?Colour(0xffeff3d6):muted,true,Justification::centred);
    g.setColour(project.songMode?Colour(0xffae7b40):Colour(0xff455048));fill(g,modeRect.getX()+2,modeRect.getY()+20,32,16);
    label(g,"SONG",{355,25,32,15},8,project.songMode?Colour(0xffffdfac):muted,true,Justification::centred);hits.push_back({modeRect,mode});
    button(g,"",{394,5,33,38},play,0,engine->playing.load());g.setColour(engine->playing.load()?accent:text);
    if(engine->playing.load()){fill(g,404,16,4,16);fill(g,413,16,4,16);}else{Path p;p.addTriangle(405,15,405,33,418,24);g.fillPath(p);}
    button(g,"",{431,5,33,38},stop);g.setColour(text);fill(g,442,18,11,11);
    button(g,"",{468,5,33,38},record,0,recording);g.setColour(recording?Colour(0xffed7567):Colour(0xffb9786c));g.fillEllipse(478,16,13,13);
    panel(g,{506,5,73,38},Colour(0xff283236));label(g,String(project.bpm,3),{509,8,67,23},18,text,false,Justification::centred);
    label(g,"TEMPO",{509,31,67,10},7,muted,false,Justification::centred);hits.push_back({{506,5,73,38},bpmDial});
    panel(g,{587,5,158,39},Colour(0xff222a2e));double beat=engine->position.load();
    String position=String((int)(beat/4)+1).paddedLeft('0',3)+":"+String((int)std::fmod(beat,4)+1).paddedLeft('0',2)+":"+String((int)(std::fmod(beat,1)*96)).paddedLeft('0',2);
    label(g,position,{596,6,140,30},24,Colour(0xffdce5df),false,Justification::centred);label(g,engine->countInRemaining.load()>0?"COUNT-IN  "+String(engine->countInRemaining.load()):"BAR    BEAT    TICK",{602,32,131,10},7,muted,false,Justification::centred);
    panel(g,{752,5,154,39},Colour(0xff293236));label(g,"CPU "+String(engine->cpuPercent.load(),1)+"%",{759,6,85,16},10,muted);
    button(g,"MON",{848,6,49,16},monitorToggle,0,inputMonitor.load());
    float peak=engine->masterMeter.load();g.setColour(Colour(0xff1d2629));fill(g,760,26,137,8);g.setColour(peak>.94?orange:accent);fill(g,760.f,26.f,jlimit(1.f,137.f,peak*137),8.f);
    float px=913;button(g,"LOOP",{px,5,43,38},loop,0,project.loop);button(g,"CLICK",{px+47,5,43,38},metro,0,project.metronome);
    button(g,typingSustain?"KEYS+":"KEYS",{px+94,5,43,38},typingToggle,0,typingEnabled);button(g,"UNDO",{px+141,5,44,38},undoAction);button(g,"SAVE",{px+189,5,44,38},saveAction);
    button(g,"?",{(float)getWidth()-31,5,26,38},help);
    label(g,project.songMode?"Song":"Pattern",{353,51,48,23},10,muted);
    button(g,"<",{405,49,24,29},patternPrevious);button(g,project.patterns[(size_t)project.activePattern].name,{432,49,164,29},patternMenu);if(project.patterns[(size_t)project.activePattern].colour){g.setColour(patternColor(project.activePattern));fill(g,434,74,160,2);}
    button(g,">",{599,49,24,29},patternNext);button(g,"+",{626,49,26,29},patternAdd);
    float viewsX=664;button(g,"Playlist",{viewsX,49,69,29},arrangeTab,0,panes[playlistPane].visible);button(g,"Channel rack",{viewsX+72,49,89,29},composeTab,0,panes[rackPane].visible);
    button(g,"Piano roll",{viewsX+164,49,77,29},pianoOpen,selectedTrack,panes[pianoPane].visible);button(g,"Mixer",{viewsX+244,49,56,29},mixTab,0,panes[mixerPane].visible);
    button(g,"Browser",{viewsX+303,49,62,29},browserToggle,0,browserVisible);button(g,"Audio",{viewsX+368,49,52,29},settings);
    button(g,audioRecordMode?"AUDIO":"NOTES",{1090,49,63,29},recordMode,0,audioRecordMode);
    if(getWidth()>1250)button(g,"REC setup",{1159,49,81,29},recordingOpen);
    g.setColour(Colour(0xff262f33));g.drawHorizontalLine(93,0,(float)getWidth());
}
void Studio::paintBrowser(Graphics& g){
    if(!browserVisible)return;Rectangle<float> r{0,96,browserWidth,(float)getHeight()-118};panel(g,r,Colour(0xff30393e));
    g.setColour(raised);fill(g,r.withHeight(23));label(g,"Browser - All",{8,97,browserWidth-39,21},11,text,true);
    button(g,"<",{browserWidth-25,98,21,19},browserToggle);hits.push_back({{browserWidth-3,96,6,r.getHeight()},browserResize});
    button(g,browserLocal?"Factory":"Local files",{8,122,browserWidth-77,23},localBrowser);button(g,"Folder",{browserWidth-66,122,58,23},folderChoose);
    if(browserLocal){paintLocalBrowser(g,r.withTrimmedTop(54));return;}
    float y=151;
    auto folder=[&](const String& name,int id){g.setColour(Colour(0xff354045));fill(g,1.f,y,browserWidth-4,23);label(g,browserExpanded[(size_t)id]?"v":">",{8,y,13,23},9,muted);
        g.setColour(Colour(0xffb59870));fill(g,25.f,y+8,12,9);fill(g,25.f,y+5,6,4);label(g,name,{45,y,browserWidth-50,23},11,text,true);hits.push_back({{2,y,browserWidth-6,23},browserFolder,id});y+=25;};
    auto row=[&](const String& name,int action,int data,Colour color){g.setColour(color.withAlpha(.85f));fill(g,39.f,y+7,4,10);label(g,name,{51,y,browserWidth-57,23},11,muted);hits.push_back({{30,y,browserWidth-36,23},action,data});y+=23;};
    folder("Current project",0);if(browserExpanded[0]){for(int i=0;i<(int)project.patterns.size();++i){if(y>r.getBottom()-185)break;row(project.patterns[(size_t)i].name,patternSelect,i,patternColor(i));}}
    folder("Drums",1);if(browserExpanded[1])for(int i=0;i<5;++i)row(instrumentName((Instrument)i),instrumentSelect,i,Colour(0xffa4b191));
    folder("Instruments",2);if(browserExpanded[2])for(int i=5;i<instrumentCount;++i)row(instrumentName((Instrument)i),instrumentSelect,i,Colour(0xff8eabad));
    row("Factory synth sounds...",synthPresets,0,accent);folder("Project samples",3);if(browserExpanded[3]){bool any=false;for(int i=0;i<(int)project.tracks.size();++i)if(project.tracks[(size_t)i].samplePath.isNotEmpty()){row(File(project.tracks[(size_t)i].samplePath).getFileName(),channelOpen,i,Colour(0xffc2a47f));any=true;}if(!any){label(g,"No imported samples",{41,y,browserWidth-47,22},10,muted.withAlpha(.6f));y+=23;}}
    y=jmax(y+10,r.getBottom()-116);if(y+103<r.getBottom()){button(g,"Import sample...",{12,y,browserWidth-27,25},importSample);button(g,"Import MIDI...",{12,y+32,browserWidth-27,25},importMidiAction);button(g,"Export WAV...",{12,y+64,browserWidth-27,25},exportAudio);}
}
void Studio::windowFrame(Graphics& g,Rectangle<float> r,const String& name,int pane){
    panel(g,r,surface);hits.push_back({r,paneBody,pane});auto title=r.withHeight(24);
    Colour c=activePane==pane?Colour(0xff66747b):Colour(0xff515e65);g.setGradientFill(ColourGradient(c,r.getX(),r.getY(),c.darker(.15f),r.getX(),r.getY()+24,false));fill(g,title.reduced(1,1));
    label(g,">",{r.getX()+4,r.getY()+2,16,20},11,text);hits.push_back({{r.getX()+3,r.getY()+2,17,20},menuOpen,10+pane});
    label(g,name,{r.getX()+24,r.getY()+1,r.getWidth()-99,22},12,text,true);hits.push_back({{r.getX()+23,r.getY()+1,r.getWidth()-91,22},paneMove,pane});
    button(g,panes[(size_t)pane].maximized?"=":"[]",{r.getRight()-47,r.getY()+3,19,17},paneMaximize,pane);
    button(g,"x",{r.getRight()-25,r.getY()+3,19,17},paneClose,pane);
}
void Studio::paintPane(Graphics& g,int pane){
    auto r=panes[(size_t)pane].maximized?workspaceBounds():panes[(size_t)pane].bounds;
    auto first=hits.size();g.saveState();g.reduceClipRegion(r.toNearestInt());
    String title=pane==synthPane?instrumentName(project.tracks[(size_t)selectedTrack].instrument)+" - Generator":pane==noteToolsPane?"Piano roll - Note tools":pane==markersPane?"Song markers and sections":pane==exportPane?"Export audio":pane==recordingPane?"Tempo and recording":pane==templatesPane?"Project templates":pane==assetsPane?"Project audio files":pane==wavePane?"Waveform editor - "+waveFile.getFileName():pane==effectsPane?(analyzerView?String("Audio analyzer"):"Mixer effects - "+project.tracks[(size_t)selectedTrack].name):pane==automationPane?"Automation clip editor":pane==playlistPane?"Playlist - Arrangement":pane==rackPane?"Channel rack":pane==pianoPane?"Piano roll - "+project.tracks[(size_t)selectedTrack].name:pane==mixerPane?"Mixer - "+project.tracks[(size_t)selectedTrack].name:"Channel settings - "+project.tracks[(size_t)selectedTrack].name;
    windowFrame(g,r,title,pane);auto content=r.reduced(2,2).withTrimmedTop(23);
    if(pane==synthPane)paintSynth(g,content);else if(pane==noteToolsPane)paintNoteTools(g,content);else if(pane==markersPane)paintMarkers(g,content);else if(pane==exportPane)paintExport(g,content);else if(pane==recordingPane)paintRecording(g,content);else if(pane==templatesPane)paintTemplates(g,content);else if(pane==assetsPane)paintProjectAssets(g,content);else if(pane==wavePane)paintWaveform(g,content);else if(pane==playlistPane)paintArrangement(g,content);else if(pane==rackPane)paintRack(g,content);else if(pane==pianoPane)paintPiano(g,content);else if(pane==mixerPane)paintMixer(g,content);else if(pane==automationPane)paintAutomation(g,content);else if(pane==effectsPane)paintEffects(g,content);else paintChannel(g,content);
    if(!panes[(size_t)pane].maximized){g.setColour(muted.withAlpha(.55f));for(int k=3;k<=11;k+=4)g.drawLine(r.getRight()-k,r.getBottom()-2,r.getRight()-2,r.getBottom()-k);hits.push_back({{r.getRight()-14,r.getBottom()-14,14,14},paneResize,pane});}
    for(size_t i=first;i<hits.size();++i){hits[i].pane=pane;hits[i].rect=hits[i].rect.getIntersection(r);}
    g.restoreState();
}
void Studio::paintRack(Graphics& g,Rectangle<float> r){
    g.setColour(Colour(0xff414b50));fill(g,r);label(g,"All",{r.getX()+9,r.getY()+2,55,25},11,text);
    label(g,project.patterns[(size_t)project.activePattern].name,{r.getX()+75,r.getY()+2,190,25},11,project.patterns[(size_t)project.activePattern].colour?patternColor(project.activePattern):muted);hits.push_back({{r.getX()+75,r.getY()+2,190,25},patternMenu});
    rotary(g,{r.getRight()-94,r.getY()+1,25,25},swingDial,project.swing/.6f,0,orange);label(g,"Swing",{r.getRight()-66,r.getY()+2,56,24},10,muted);
    const auto& pat=project.patterns[(size_t)project.activePattern];
    int steps=jlimit(4,256,(int)std::llround(pat.length*4));int groups=jmax(1,steps/4);
    float rowH=27,gridX=r.getX()+243,gridW=r.getWidth()-255;
    int visible=jmin((int)project.tracks.size()-trackScroll,jmax(1,(int)((r.getHeight()-55)/rowH)));
    rackRect={gridX,r.getY()+30,gridW,visible*rowH};float groupGap=steps<=64?5.f:2.f,cell=(gridW-(groups-1)*groupGap)/(float)steps;if(cell<1.f){groupGap=0;cell=gridW/(float)steps;}
    for(int row=0;row<visible;++row){int t=trackScroll+row;float y=rackRect.getY()+row*rowH;const auto& tr=project.tracks[(size_t)t];
        g.setColour(t==selectedTrack?Colour(0xff505e60):Colour(0xff3c464b));fill(g,r.getX()+4,y,r.getWidth()-8,rowH-1);
        g.setColour(tr.mute?Colour(0xff252e31):accent);g.fillEllipse(r.getX()+12,y+10,7,7);hits.push_back({{r.getX()+5,y,23,rowH},mute,t});
        rotary(g,{r.getX()+31,y+2,23,23},panDial,(tr.pan+1)/2,t);rotary(g,{r.getX()+58,y+2,23,23},volumeDial,tr.volume/1.5f,t);
        panel(g,{r.getX()+87,y+4,27,19},Colour(0xff2d373c));label(g,String(t+1),{r.getX()+87,y+4,27,19},10,muted,false,Justification::centred);hits.push_back({{r.getX()+87,y+4,27,19},trackSelect,t});
        button(g,tr.name,{r.getX()+121,y+3,98,22},channelOpen,t,t==selectedTrack);
        if(t==selectedTrack){g.setColour(accent);Path p;p.addTriangle(r.getX()+225,y+10,r.getX()+225,y+18,r.getX()+230,y+14);g.fillPath(p);}
        if(tr.isBus){label(g,"Mixer bus - route sources here from the Mixer",{gridX,y+4,gridW,19},10,muted);continue;}
        const auto& ns=visibleNotes(project.activePattern,t);
        if(!isDrum(tr.instrument)&&tr.instrument!=Instrument::sample){Rectangle<float> preview{gridX,y+4,gridW,19};g.setColour(Colour(0xff283135));fill(g,preview);
            int lo=127,hi=0;for(const auto& n:ns){lo=jmin(lo,n.pitch);hi=jmax(hi,n.pitch);}for(const auto& n:ns){float x=gridX+(float)(n.beat/project.patterns[(size_t)project.activePattern].length)*gridW;float yy=y+17-(float)(n.pitch-lo)/jmax(1,hi-lo)*10;g.setColour(accent);fill(g,x,yy,jmax(2.f,(float)n.length/(float)project.patterns[(size_t)project.activePattern].length*gridW),2.f);}hits.push_back({preview,pianoOpen,t});
        }else for(int s=0;s<steps;++s){float x=gridX+s*cell+(s/4)*groupGap;bool on=false;for(const auto& n:ns)if((int)std::floor(n.beat*4+.0001)==s){on=true;break;}Rectangle<float> step{x,y+5,jmax(.5f,cell-jmin(2.f,cell*.2f)),18};
            Colour off=(s/4)%2?Colour(0xff675d59):Colour(0xff505d62);Colour c=on?((s/4)%2?Colour(0xffd7a5a0):Colour(0xffbec8cd)):off;
            g.setGradientFill(ColourGradient(c.brighter(.1f),x,y+5,c.darker(.15f),x,y+23,false));g.fillRoundedRectangle(step,2);g.setColour(border);g.drawRoundedRectangle(step,2,1);
            if(engine->playing.load()&&s==(int)(engine->position.load()*4)%steps){g.setColour(Colour(0xffd7e7b2));g.drawRect(step.expanded(1),1);}hits.push_back({step,rackGrid,t,s});}
    }
    button(g,"+",{r.getX()+119,r.getBottom()-25,30,20},addChannel,-1);label(g,String(steps)+" steps",{r.getRight()-100,r.getBottom()-26,81,21},9,muted);hits.push_back({{r.getRight()-100,r.getBottom()-26,81,21},patternSteps});
}
Rectangle<float> Studio::noteBounds(const Note& n)const{return {pianoRect.getX()+(float)((n.beat-pianoStart)/pianoSpan)*pianoRect.getWidth(),pianoRect.getY()+(pianoLow+pianoRows-1-n.pitch)*pianoRect.getHeight()/pianoRows+1,jmax(4.f,(float)(n.length/pianoSpan)*pianoRect.getWidth()-1),pianoRect.getHeight()/pianoRows-1};}
void Studio::paintPiano(Graphics& g,Rectangle<float> r){
    const auto& shown=visibleNotes(project.activePattern,selectedTrack);
    const bool generatedPreview=noteToolCurrent()&&noteToolValid&&noteToolAudition&&(noteToolOptions.mode==NoteToolMode::euclidean||noteToolOptions.mode==NoteToolMode::arpeggiator)&&noteToolPattern==project.activePattern&&noteToolTrack==selectedTrack;
    g.setColour(surface);fill(g,r);const char* tools[]={"Draw","Paint","Delete","Select"};
    for(int t=0;t<4;++t)button(g,tools[t],{r.getX()+7+t*48,r.getY()+3,45,23},toolSelect,t,editorTool==t);
    button(g,snap==0?"Snap: none":"Snap: line",{r.getX()+205,r.getY()+3,86,23},snapMenu);
    button(g,"Quantize",{r.getX()+297,r.getY()+3,67,23},quantize);button(g,"Generate",{r.getX()+370,r.getY()+3,69,23},generate);
    button(g,"Scale",{r.getRight()-134,r.getY()+3,54,23},scaleToggle,0,scaleGuide);button(g,"-12",{r.getRight()-75,r.getY()+3,32,23},transposeDown);button(g,"+12",{r.getRight()-39,r.getY()+3,32,23},transposeUp);
    if(pianoLow<0){int lo=48,hi=72;if(!shown.empty()){lo=127;hi=0;for(const auto& n:shown){lo=jmin(lo,n.pitch);hi=jmax(hi,n.pitch);}}pianoLow=jlimit(0,128-pianoRows,(lo+hi)/2-pianoRows/2);}
    float gridY=r.getY()+48;float gridH=r.getHeight()-130;
    pianoRect={r.getX()+54,gridY,r.getWidth()-65,gridH};velocityRect={pianoRect.getX(),pianoRect.getBottom()+23,pianoRect.getWidth(),48};
    g.setColour(Colour(0xff2f383c));fill(g,pianoRect.withY(gridY-20).withHeight(20));hits.push_back({pianoRect.withY(gridY-20).withHeight(20),seekRuler,pianoPane});
    float rowH=gridH/pianoRows;
    for(int row=0;row<pianoRows;++row){int pitch=pianoLow+pianoRows-1-row;float y=gridY+row*rowH;bool black=blackKey(pitch);
        g.setColour(black?Colour(0xff242c30):Colour(0xff2c3539));fill(g,pianoRect.getX(),y,pianoRect.getWidth(),rowH);
        if(scaleGuide&&!black){g.setColour(accent.withAlpha(.03f));fill(g,pianoRect.getX(),y,pianoRect.getWidth(),rowH);}
        g.setColour(black?Colour(0xff353d40):Colour(0xffbac0bf));fill(g,r.getX()+5,y,46,rowH-1);if(black){g.setColour(Colour(0xff727c7b));fill(g,r.getX()+5,y+1,7,rowH-3);}
        if(pitch%12==0)label(g,pitchName(pitch),{r.getX()+8,y,40,rowH},9,black?text:Colour(0xff384044),false,Justification::centredRight);
        hits.push_back({{r.getX()+5,y,46,rowH},pianoKey,pitch});g.setColour(border.withAlpha(pitch%12==0?.75f:.35f));g.drawHorizontalLine((int)y,pianoRect.getX(),pianoRect.getRight());
    }
    int steps=(int)std::ceil(pianoSpan*4);
    for(int s=0;s<=steps;++s){double beat=pianoStart+s*.25;float x=pianoRect.getX()+(float)(s*.25/pianoSpan)*pianoRect.getWidth();g.setColour(Colour(0xff657178).withAlpha(std::fmod(beat,4)<.01?.48f:s%4==0?.28f:.12f));g.drawVerticalLine((int)x,gridY,velocityRect.getBottom());
        if(std::fmod(beat,4)<.01)label(g,String((int)beat/4+1),{x+4,gridY-20,55,19},11,text);}
    hits.push_back({pianoRect,pianoGrid});g.saveState();g.reduceClipRegion(pianoRect.toNearestInt());
    auto& pat=project.patterns[(size_t)project.activePattern];for(int t=0;t<(int)project.tracks.size();++t)if(t!=selectedTrack&&!isDrum(project.tracks[(size_t)t].instrument))for(const auto& n:visibleNotes(project.activePattern,t)){g.setColour(Colour(0xff718079).withAlpha(.25f));fill(g,noteBounds(n));}
    if(noteToolCurrent()&&noteToolAudition&&noteToolPattern==project.activePattern&&noteToolTrack==selectedTrack){g.setColour(Colour(0xff849294));for(size_t i=0;i<noteToolSource.size();++i)if(noteToolOptions.mode==NoteToolMode::euclidean||noteToolOptions.mode==NoteToolMode::arpeggiator||i>=noteToolCandidate.size()||noteToolSource[i]!=noteToolCandidate[i])g.drawRect(noteBounds(noteToolSource[i]).getIntersection(pianoRect),1.f);}
    for(int i=0;i<(int)shown.size();++i){const auto& n=shown[(size_t)i];auto nr=noteBounds(n).getIntersection(pianoRect);if(nr.isEmpty())continue;bool selected=!generatedPreview&&(selectedNotes.count(i)||i==selectedNote);g.setColour(selected?Colour(0xffdfb776):patternColor(project.activePattern).withAlpha(.86f));fill(g,nr);g.setColour(Colour(0xff5c724f));g.drawRect(nr,1);hits.push_back({nr,noteHit,i});}
    if(!selectionBox.isEmpty()){g.setColour(orange.withAlpha(.12f));fill(g,selectionBox);g.setColour(orange);g.drawRect(selectionBox,1);}
    if(engine->playing.load()){float x=pianoRect.getX()+(float)((std::fmod(engine->position.load(),pat.length)-pianoStart)/pianoSpan)*pianoRect.getWidth();g.setColour(Colour(0xffdbe9aa));g.drawVerticalLine((int)x,gridY,pianoRect.getBottom());}g.restoreState();
    label(g,"Control",{r.getX()+4,pianoRect.getBottom()+4,49,18},9,muted);label(g,"Note velocity",{pianoRect.getX()+5,pianoRect.getBottom()+3,125,18},10,muted);
    g.setColour(Colour(0xff273135));fill(g,velocityRect);hits.push_back({velocityRect,velocityDrag,-1});
    for(int i=0;i<(int)shown.size();++i){const auto& n=shown[(size_t)i];float x=pianoRect.getX()+(float)((n.beat-pianoStart)/pianoSpan)*pianoRect.getWidth();if(x<pianoRect.getX()||x>pianoRect.getRight())continue;float height=n.velocity*velocityRect.getHeight();g.setColour(i==selectedNote&&!generatedPreview?orange:patternColor(project.activePattern));fill(g,x,velocityRect.getBottom()-height,2,height);fill(g,x-2,velocityRect.getBottom()-height,6,3);hits.push_back({{x-5,velocityRect.getY(),12,velocityRect.getHeight()},velocityDrag,i});}
    g.setColour(raised);fill(g,r.getX()+54,r.getBottom()-12,r.getWidth()-65,8);g.setColour(Colour(0xff758288));fill(g,r.getX()+55,r.getBottom()-11,jmax(32.f,(r.getWidth()-67)*(float)(pianoSpan/pat.length)),6);
}
void Studio::paintArrangement(Graphics& g,Rectangle<float> r){
    g.setColour(Colour(0xff3a444a));fill(g,r);const char* tools[]={"Draw","Paint","Delete","Select"};for(int t=0;t<4;++t)button(g,tools[t],{r.getX()+6+t*48,r.getY()+2,45,22},toolSelect,t,editorTool==t);
    button(g,snap==0?"Snap: none":"Snap: line",{r.getX()+204,r.getY()+2,85,22},snapMenu);button(g,"Audio...",{r.getX()+295,r.getY()+2,67,22},importAudioClip);button(g,"Split",{r.getX()+368,r.getY()+2,47,22},splitClip);button(g,"Auto",{r.getX()+421,r.getY()+2,47,22},automationCreate);button(g,"Markers",{r.getX()+474,r.getY()+2,70,22},markersOpen);button(g,"Fill pattern",{r.getRight()-105,r.getY()+2,96,22},arrangeFill);
    float pickerW=126,headerW=116,top=r.getY()+73;float left=r.getX()+pickerW+headerW+4;float height=r.getHeight()-89;
    playlistLaneHeight=jlimit(28.f,53.f,height/jmax(8,(int)project.tracks.size()));arrangeRect={left,top,r.getRight()-left-11,playlistLaneHeight*(float)project.tracks.size()};
    g.setColour(Colour(0xff303a3f));fill(g,r.getX()+2,r.getY()+27,pickerW-4,r.getHeight()-31);label(g,"Patterns",{r.getX()+11,r.getY()+29,98,18},10,muted);
    for(int p=0;p<(int)project.patterns.size();++p){float y=top+3+p*31;if(y+28>r.getBottom()-10)break;Rectangle<float> pr{r.getX()+8,y,pickerW-15,27};panel(g,pr,p==project.activePattern?Colour(0xff64734f):Colour(0xff475258));if(project.patterns[(size_t)p].colour){g.setColour(patternColor(p));fill(g,pr.getX()+1,pr.getY()+2,3,pr.getHeight()-4);}label(g,project.patterns[(size_t)p].name,pr.reduced(7,0),11,text);hits.push_back({pr,patternSelect,p});}
    float beatW=arrangeRect.getWidth()/(float)playlistSpan;
    g.setColour(Colour(0xff2d373c));fill(g,left,top-21,arrangeRect.getWidth(),21);hits.push_back({{left,top-21,arrangeRect.getWidth(),21},seekRuler,playlistPane});
    markerRailRect={left,top-43,arrangeRect.getWidth(),20};paintMarkerRail(g);
    for(int t=0;t<(int)project.tracks.size();++t){float y=top+t*playlistLaneHeight;g.setColour(t%2==0?Colour(0xff313a3e):Colour(0xff2d363b));fill(g,left,y,arrangeRect.getWidth(),playlistLaneHeight-1);
        panel(g,{r.getX()+pickerW,y,headerW,playlistLaneHeight},Colour(0xff414c52));label(g,"Track "+String(t+1),{r.getX()+pickerW+11,y+4,headerW-18,19},11,text);
        label(g,project.tracks[(size_t)t].name,{r.getX()+pickerW+11,y+23,headerW-28,17},9,muted);hits.push_back({{r.getX()+pickerW,y,headerW-23,playlistLaneHeight},trackSelect,t});
        g.setColour(project.tracks[(size_t)t].mute?border:accent.withAlpha(.8f));g.fillEllipse(left-16,y+10,6,6);hits.push_back({{left-23,y,20,22},mute,t});}
    for(int beat=(int)playlistStart;beat<=playlistStart+playlistSpan;++beat){float x=left+(float)(beat-playlistStart)*beatW;g.setColour(Colour(0xff6a787d).withAlpha(beat%4==0?.4f:.14f));g.drawVerticalLine((int)x,top,arrangeRect.getBottom());if(beat%4==0)label(g,String(beat/4+1),{x+3,top-20,beatW*4,19},10,text);}
    hits.push_back({arrangeRect,arrangeGrid});g.saveState();g.reduceClipRegion(arrangeRect.toNearestInt());
    if(auto* section=findSongMarker(project,project.loopSection);section&&project.loop){float a=left+(float)(section->beat-playlistStart)*beatW,b=left+(float)(section->endBeat-playlistStart)*beatW;g.setColour(colorFor(section->colour).withAlpha(.10f));fill(g,a,top,b-a,arrangeRect.getHeight());g.setColour(colorFor(section->colour).withAlpha(.8f));g.drawVerticalLine((int)a,top,arrangeRect.getBottom());g.drawVerticalLine((int)b,top,arrangeRect.getBottom());}
    if(project.recording.punch){float a=left+(float)(project.recording.punchStart-playlistStart)*beatW,b=left+(float)(project.recording.punchEnd-playlistStart)*beatW;g.setColour(Colour(0xffe3aa63).withAlpha(.08f));fill(g,a,top,b-a,arrangeRect.getHeight());g.setColour(Colour(0xffe3aa63));g.drawVerticalLine((int)a,top,arrangeRect.getBottom());g.drawVerticalLine((int)b,top,arrangeRect.getBottom());}
    for(int i=0;i<(int)project.clips.size();++i){const auto& c=project.clips[(size_t)i];
        Rectangle<float> cr{left+(float)(c.beat-playlistStart)*beatW+1,top+c.track*playlistLaneHeight+2,jmax(3.f,(float)c.length*beatW-2),playlistLaneHeight-4};
        if(!cr.intersects(arrangeRect))continue;
        bool coloured=!c.isAudio()&&!c.isAutomation()&&project.patterns[(size_t)c.pattern].colour!=0;Colour patColour=coloured?patternColor(c.pattern):Colour(0xff7d927b);
        panel(g,cr,i==selectedClip?Colour(0xff697555):c.isAutomation()?Colour(0xff716451):c.isAudio()?Colour(0xff4b646b):coloured?patColour.darker(.52f):Colour(0xff53635a));
        g.setColour(c.isAutomation()?Colour(0xffad946d):c.isAudio()?Colour(0xff809da4):patColour);fill(g,cr.withHeight(15));
        label(g,c.isAutomation()?automationName((AutomationTarget)c.automationTarget):c.isAudio()?File(c.audioPath).getFileNameWithoutExtension():project.patterns[(size_t)c.pattern].name,cr.reduced(4,0).withHeight(15),9,Colour(0xff202b24));
        if(c.isAutomation()){
            Path curve;float start=jmax(cr.getX(),arrangeRect.getX()),end=jmin(cr.getRight(),arrangeRect.getRight());
            for(float x=start;x<=end;x+=2){double local=(x-cr.getX())/cr.getWidth()*c.length;float y=cr.getBottom()-3-automationValue(c.points.data(),(int)c.points.size(),local)*jmax(2.f,cr.getHeight()-21);if(x==start)curve.startNewSubPath(x,y);else curve.lineTo(x,y);}
            g.setColour(Colour(0xffe0c594));g.strokePath(curve,PathStrokeType(1.3f));
        }else if(c.isAudio()){
            if(auto* sample=clipSamples[(size_t)i]){float centre=cr.getY()+15+(cr.getHeight()-15)*.5f;float amplitude=(cr.getHeight()-18)*.5f;
                float first=jmax(cr.getX(),arrangeRect.getX()),last=jmin(cr.getRight(),arrangeRect.getRight());g.setColour(Colour(0xffbdd4d7));
                for(float x=first;x<last;x+=2){double local=(x-cr.getX())/cr.getWidth()*c.length;int start=(int)((c.sourceOffset+local*60/project.bpm)*sample->sampleRate);int span=jmax(1,(int)(2/beatW*60/project.bpm*sample->sampleRate));float peak=0;
                    for(int k=start;k<jmin(start+span,sample->audio.getNumSamples());k+=jmax(1,span/48))if(k>=0)peak=jmax(peak,std::abs(sample->audio.getSample(0,k)),std::abs(sample->audio.getSample(1,k)));
                    g.drawVerticalLine((int)x,centre-jmin(1.f,peak*c.gain)*amplitude,centre+jmin(1.f,peak*c.gain)*amplitude+1);}
                g.setColour(Colour(0xffffce89));if(c.fadeIn>0)g.drawLine(cr.getX(),cr.getBottom(),cr.getX()+(float)c.fadeIn*beatW,cr.getY()+16,1);
                if(c.fadeOut>0)g.drawLine(cr.getRight()-(float)c.fadeOut*beatW,cr.getY()+16,cr.getRight(),cr.getBottom(),1);
            }else label(g,"Missing audio",cr.withTrimmedTop(16).reduced(4),9,orange);
        }else{const auto& pat=project.patterns[(size_t)c.pattern];int lo=127,hi=0;
            for(const auto& n:pat.lanes[(size_t)c.track]){lo=jmin(lo,n.pitch);hi=jmax(hi,n.pitch);}
            for(double off=0;off<c.length;off+=pat.length)for(const auto& n:pat.lanes[(size_t)c.track])if(off+n.beat<c.length){float x=cr.getX()+(float)(off+n.beat)*beatW;float y=cr.getBottom()-6-(float)(n.pitch-lo)/jmax(1,hi-lo)*jmax(2.f,cr.getHeight()-24);g.setColour(coloured?patColour.brighter(.18f):Colour(0xffb8cca5));fill(g,x,y,jmax(2.f,(float)jmin(n.length,c.length-off-n.beat)*beatW),2.f);}}
        auto hit=cr.getIntersection(arrangeRect);if(!hit.isEmpty())hits.push_back({hit,clipHit,i});}
    if(recording&&audioRecordMode){Rectangle<float> take{left+(float)(takeBeat-playlistStart)*beatW,top+takeTrack*playlistLaneHeight+2,(float)(recorder.frames()/recorder.rate()*takeTempo/60)*beatW,playlistLaneHeight-4};panel(g,take,Colour(0xff825049));label(g,"Recording...",take.reduced(4,0).withHeight(16),9,text);}
    if(engine->playing.load()&&project.songMode){float x=left+(float)(engine->position.load()-playlistStart)*beatW;g.setColour(accent);g.drawVerticalLine((int)x,top,arrangeRect.getBottom());}g.restoreState();
    g.setColour(raised);fill(g,left,r.getBottom()-12,arrangeRect.getWidth(),8);
}
void Studio::paintMixer(Graphics& g,Rectangle<float> r){
    g.setColour(Colour(0xff3c474d));fill(g,r);float effectW=204,stripArea=r.getWidth()-effectW-14;
    float stripW=jmin(68.f,stripArea/((float)project.tracks.size()+1));int count=(int)project.tracks.size();float top=r.getY()+3,bottom=r.getBottom()-15;
    for(int index=0;index<=count;++index){bool master=index==0;int t=index-1;float x=r.getX()+4+index*stripW;bool selected=!master&&t==selectedTrack;auto col=master?orange:colorFor(t);
        panel(g,{x,top,stripW-2,bottom-top},selected?Colour(0xff59665d):Colour(0xff455258));
        label(g,master?"Master":String(index),{x+3,top+1,stripW-8,15},10,master?orange:text,true,Justification::centred);
        label(g,master?"":project.tracks[(size_t)t].name,{x+3,top+17,stripW-8,16},9,text,false,Justification::centred);if(!master)hits.push_back({{x,top,stripW-3,34},trackSelect,t});
        if(!master){g.setColour(project.tracks[(size_t)t].mute?border:accent);g.fillEllipse(x+stripW/2-3,top+38,6,6);hits.push_back({{x+8,top+34,stripW-18,15},mute,t});}
        rotary(g,{x+stripW/2-11,top+52,22,22},master?masterDial:panDial,master?project.master:(project.tracks[(size_t)t].pan+1)/2,master?0:t,col);
        float fy=top+85,fh=jmax(35.f,bottom-fy-41);float faderX=x+stripW*.40f;g.setColour(border);fill(g,faderX-2,fy,4,fh);
        for(int db=0;db<6;++db){float y=fy+db*fh/5;g.setColour(muted.withAlpha(.2f));g.drawHorizontalLine((int)y,x+6,x+stripW-15);}
        float gain=master?liveParameter(masterDial,-1,project.master):liveParameter(volumeDial,t,project.tracks[(size_t)t].volume/1.5f)*1.5f;float norm=master?gain:gain/1.5f;float knobY=fy+(1-norm)*fh;
        g.setGradientFill(ColourGradient(Colour(0xffb2bdbf),faderX,knobY-6,Colour(0xff738086),faderX,knobY+6,false));fill(g,faderX-10,knobY-6,20,12);g.setColour(border);g.drawRect(Rectangle<float>(faderX-10,knobY-6,20,12),1);g.drawHorizontalLine((int)knobY,faderX-8,faderX+8);
        hits.push_back({{x+5,fy-8,stripW*.56f,fh+16},mixFader,master?-1:t});
        float peak=master?engine->masterMeter.load():engine->meters[(size_t)t].load();float mx=x+stripW-12;g.setColour(Colour(0xff263331));fill(g,mx,fy,6,fh);g.setColour(peak>.95f?orange:accent);float level=jlimit(0.f,1.f,peak*1.4f)*fh;fill(g,mx,fy+fh-level,6,jmax(1.f,level));
        label(g,gain<.0001?"-inf":String(20*std::log10(gain),1),{x+3,bottom-32,stripW-7,15},9,text,false,Justification::centred);
        float smallW=jlimit(7.f,14.f,(stripW-8)*.4f);if(!master)button(g,"S",{x+3,bottom-17,smallW,14},solo,t,project.tracks[(size_t)t].solo);
        if(master||t!=selectedTrack){bool routed=project.tracks[(size_t)selectedTrack].mixer.output==t;Rectangle<float> arrow{x+stripW-smallW-3,bottom-18,smallW,15};button(g,"",arrow,routeDestination,t,routed);g.setColour(routed?accent:muted);Path icon;icon.addTriangle(arrow.getCentreX()-3,arrow.getY()+5,arrow.getCentreX()+3,arrow.getY()+5,arrow.getCentreX(),arrow.getY()+10);g.fillPath(icon);}
        g.setColour(col.withAlpha(.5f));fill(g,x+3,bottom-2,stripW-8,2);
    }
    float fx=r.getRight()-effectW;panel(g,{fx,r.getY()+2,effectW-3,r.getHeight()-6},Colour(0xff374247));
    label(g,project.tracks[(size_t)selectedTrack].name,{fx+9,r.getY()+3,effectW-75,19},12,text,true);
    button(g,"+ Bus",{r.getRight()-66,r.getY()+3,56,21},addBus);
    label(g,"Effect slots",{fx+9,r.getY()+24,effectW-20,14},9,muted);
    const auto& tr=project.tracks[(size_t)selectedTrack];
    for(int slot=0;slot<6;++slot){int effect=tr.mixer.order[(size_t)slot];float y=r.getY()+43+slot*23;bool enabled=effect==0?tr.mixer.eq:effect==1?tr.mixer.compressor:effect==2?tr.mixer.reverb:effect==3?tr.mixer.chorus.enabled:effect==4?tr.mixer.flanger.enabled:tr.mixer.phaser.enabled;
        button(g,String(slot+1)+"  "+effectName(effect),{fx+9,y,effectW-43,20},effectsOpen,effect);button(g,enabled?"o":"-",{r.getRight()-30,y,20,20},effectToggle,effect,enabled);}
    dial(g,"Delay send",{fx+7,r.getY()+183,effectW-19,30},sendDial,tr.send/.8f,selectedTrack);
    button(g,"Out: "+(tr.mixer.output<0?String("Master"):project.tracks[(size_t)tr.mixer.output].name),{fx+10,r.getY()+218,effectW-26,23},outputRoute);
    button(g,"Analyzer",{fx+10,r.getY()+244,(effectW-32)/2,20},analyzerOpen);button(g,"Stereo...",{fx+16+(effectW-32)/2,r.getY()+244,(effectW-32)/2,20},stereoOpen);
    button(g,"Channel settings",{fx+10,r.getBottom()-30,effectW-26,23},channelOpen,selectedTrack);

}
void Studio::paintChannel(Graphics& g,Rectangle<float> r){
    g.setColour(surface);fill(g,r);const auto& tr=project.tracks[(size_t)selectedTrack];
    label(g,instrumentName(tr.instrument),{r.getX()+13,r.getY()+7,r.getWidth()-120,24},15,text,true);button(g,"Load sample",{r.getRight()-105,r.getY()+8,94,24},importSample);
    auto* sample=previewSamples[(size_t)selectedTrack];Rectangle<float> wave{r.getX()+13,r.getY()+44,r.getWidth()-26,66};panel(g,wave,Colour(0xff273237));
    if(sample){Path p;int size=sample->audio.getNumSamples();for(int i=0;i<(int)wave.getWidth();++i){int frame=(int)((double)i/wave.getWidth()*size);float value=sample->audio.getSample(0,jlimit(0,size-1,frame));float x=wave.getX()+i,y=wave.getCentreY()-value*wave.getHeight()*.44f;if(i==0)p.startNewSubPath(x,y);else p.lineTo(x,y);}g.setColour(accent);g.strokePath(p,PathStrokeType(1));}
    else{label(g,tr.instrument==Instrument::sample?"No sample loaded - click Load sample":"Synthesized instrument",wave.reduced(10,0),12,muted,false,Justification::centred);}
    if(sample)button(g,"Edit sample range...",{r.getX()+13,r.getY()+113,145,23},waveformOpen,1);
    if(isSynth(tr.instrument))button(g,"Open synthesizer...",{r.getX()+13,r.getY()+113,154,23},synthOpen);
    float y=r.getY()+144,w=(r.getWidth()-44)/2;
    dial(g,"Volume",{r.getX()+14,y,w,32},volumeDial,tr.volume/1.5f,selectedTrack);dial(g,"Pan",{r.getX()+29+w,y,w,32},panDial,(tr.pan+1)/2,selectedTrack);
    dial(g,"Attack "+String(tr.attack*1000,0)+" ms",{r.getX()+14,y+43,w,32},attackDial,(float)(std::log(tr.attack/.001f)/std::log(2000.)),selectedTrack);
    dial(g,"Release "+String(tr.release*1000,0)+" ms",{r.getX()+29+w,y+43,w,32},releaseDial,(float)(std::log(tr.release/.02f)/std::log(150.)),selectedTrack);
    dial(g,"Tone filter",{r.getX()+14,y+86,w,32},cutoffDial,(float)(std::log(tr.cutoff/60)/std::log(20000./60)),selectedTrack);dial(g,"Delay send",{r.getX()+29+w,y+86,w,32},sendDial,tr.send/.8f,selectedTrack);
    if(auto* n=note()){label(g,"Note: "+pitchName(n->pitch),{r.getX()+14,y+129,140,17},10,text);dial(g,"Velocity",{r.getX()+14,y+151,w,31},noteVelocity,n->velocity);dial(g,"Chance",{r.getX()+29+w,y+151,w,31},noteProbability,n->probability);label(g,"Ratchets: "+String(n->ratchets)+" (wheel)",{r.getX()+14,y+187,220,19},10,muted);hits.push_back({{r.getX()+14,y+185,220,23},noteRatchet});}
    else button(g,"Open piano roll",{r.getX()+14,y+132,128,24},pianoOpen,selectedTrack);
}
void Studio::paintInspector(Graphics&){ }
}
