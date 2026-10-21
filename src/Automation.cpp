// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <algorithm>
namespace pulse {
using namespace juce;
int Studio::automationTargetFor(int action,int track)const{
    if(action==masterDial||(action==mixFader&&track<0))return 5;
    switch(action){case volumeDial:case mixFader:return 0;case panDial:return 1;case cutoffDial:return 2;case driveDial:return 3;case sendDial:return 4;default:return -1;}
}
float Studio::liveParameter(int action,int track,float fallback)const{
    if(!engine->playing.load()||!project.songMode)return fallback;
    int target=automationTargetFor(action,track);float value=-1;
    if(target==5)value=engine->masterAutomation.load();
    else if(target>=0&&track>=0&&track<(int)project.tracks.size())value=engine->automationReadout[(size_t)track][(size_t)target].load();
    return value<0?fallback:value;
}
Clip* Studio::automationClip(){return selectedClip>=0&&selectedClip<(int)project.clips.size()&&project.clips[(size_t)selectedClip].isAutomation()?&project.clips[(size_t)selectedClip]:nullptr;}
void Studio::createAutomation(int action,int track){
    if(recorder.isActive()){status="Finish the take before creating automation.";return;}
    int target=automationTargetFor(action,track);if(target<0)return;
    int count=0,points=0;for(const auto& c:project.clips)if(c.isAutomation()){++count;points+=(int)c.points.size();}
    if(project.clips.size()>=maxClips||count>=maxAutomationClips||points+2>maxTotalAutomationPoints){status="Automation limit: 64 clips and 4096 points.";return;}
    checkpoint();Clip c;c.track=track<0?selectedTrack:jlimit(0,(int)project.tracks.size()-1,track);c.beat=jlimit(0.,4096.,snapped(editCursorBeat));c.length=8;c.automationTarget=target;c.automationTrack=target==5?-1:c.track;
    float value=parameterValue(action,track);c.points={{0,value,false},{c.length,value,false}};
    project.clips.push_back(c);selectedClip=(int)project.clips.size()-1;selectedPoint=-1;project.songMode=true;changed();showPane(automationPane);
    status="Automation linked. Click to add points, drag to move, right-click to remove. Held segments use the STEP button.";
}
void Studio::deleteAutomationPoint(){
    auto* c=automationClip();if(!c||selectedPoint<=0||selectedPoint>=(int)c->points.size()-1)return;
    checkpoint();c->points.erase(c->points.begin()+selectedPoint);selectedPoint=-1;changed();
}
void Studio::editAutomationClipboard(int action){
    auto* c=automationClip();if(!c)return;
    bool copying=action==copySelection||action==cutSelection,duplicate=action==duplicateSelection;
    if(copying||duplicate){
        if(selectedPoint<0||selectedPoint>=(int)c->points.size()){status="Select a curve point first.";return;}
        automationClipboard={c->points[(size_t)selectedPoint]};
        if(copying){status="Automation point copied. Paste at the curve cursor.";if(action==cutSelection)deleteAutomationPoint();return;}
    }
    if(automationClipboard.empty())return;
    auto point=automationClipboard.front();double at=duplicate?point.beat+jmax(.25,snap):automationCursor;
    if(at<0||at>c->length){status="Paste position must be inside the automation clip.";return;}
    at=jlimit(0.,c->length,snapped(at));int index=0;while(index<(int)c->points.size()&&c->points[(size_t)index].beat<at)++index;
    bool replace=index<(int)c->points.size()&&std::abs(c->points[(size_t)index].beat-at)<.00001;
    if(!replace){int total=0;for(const auto& clip:project.clips)total+=(int)clip.points.size();if(c->points.size()>=maxAutomationPoints||total>=maxTotalAutomationPoints){status="Automation point limit reached.";return;}}
    checkpoint();point.beat=at;if(replace)c->points[(size_t)index]=point;else c->points.insert(c->points.begin()+index,point);
    selectedPoint=index;changed();
}
bool Studio::automationMouseDown(const Hit& hit,const MouseEvent& e){
    if(hit.action!=automationPoint&&hit.action!=automationGrid)return false;
    auto* c=automationClip();if(!c)return true;
    if(hit.action==automationPoint){selectedPoint=hit.data;
        if(e.mods.isRightButtonDown()||editorTool==2){deleteAutomationPoint();return true;}
    }else{
        if(e.mods.isRightButtonDown()||editorTool==2)return true;
        double at=jlimit(0.,c->length,snapped((e.position.x-automationRect.getX())/automationRect.getWidth()*c->length,e.mods.isAltDown()));
        int index=0;while(index<(int)c->points.size()&&c->points[(size_t)index].beat<at)++index;
        if(index<(int)c->points.size()&&std::abs(c->points[(size_t)index].beat-at)<.00001)selectedPoint=index;
        else{
            if(c->points.size()>=maxAutomationPoints){status="This automation clip supports up to 256 points.";return true;}
            int total=0;for(const auto& clip:project.clips)total+=(int)clip.points.size();if(total>=maxTotalAutomationPoints){status="The project supports up to 4096 automation points.";return true;}
            checkpoint();c->points.insert(c->points.begin()+index,{at,jlimit(0.f,1.f,(automationRect.getBottom()-e.position.y)/automationRect.getHeight()),false});selectedPoint=index;changed();
            dragAction=automationPoint;dragData=selectedClip;dragExtra=selectedPoint;return true;
        }
    }
    checkpoint();dragAction=automationPoint;dragData=selectedClip;dragExtra=selectedPoint;return true;
}
void Studio::automationMouseDrag(const MouseEvent& e){
    if(dragData!=selectedClip)return;auto* c=automationClip();if(!c||dragExtra<0||dragExtra>=(int)c->points.size())return;
    auto& point=c->points[(size_t)dragExtra];float v=jlimit(0.f,1.f,(automationRect.getBottom()-e.position.y)/automationRect.getHeight());
    if(dragExtra>0&&dragExtra<(int)c->points.size()-1){double at=snapped((e.position.x-automationRect.getX())/automationRect.getWidth()*c->length,e.mods.isAltDown());
        double previous=c->points[(size_t)dragExtra-1].beat,next=c->points[(size_t)dragExtra+1].beat,epsilon=jmin(.00001,(next-previous)*.25);point.beat=jlimit(previous+epsilon,next-epsilon,at);}
    point.value=v;changed();
}
void Studio::paintAutomation(Graphics& g,Rectangle<float> r){
    const Colour text(0xffd9ddde),accent(0xffd3af71),muted(0xffadb7bd);
    g.setColour(Colour(0xff3b464c));g.fillRect(r);auto* c=automationClip();
    if(!c){label(g,"Select an automation clip, or right-click a mixer control and choose Create automation clip.",r.reduced(20),12,text);return;}
    label(g,(c->automationTarget==5?"Master":project.tracks[(size_t)c->automationTrack].name)+" - "+automationName((AutomationTarget)c->automationTarget),{r.getX()+11,r.getY()+3,r.getWidth()-400,25},12,text,true);
    button(g,"Shape...",{r.getRight()-387,r.getY()+4,87,23},automationShapeOpen);
    button(g,"Generate...",{r.getX()+11,r.getY()+33,125,23},automationGenerateOpen);
    bool hold=selectedPoint>=0&&selectedPoint<(int)c->points.size()&&c->points[(size_t)selectedPoint].hold;
    button(g,hold?"STEP":selectedPoint>=0&&selectedPoint<(int)c->points.size()?String(automationCurveName(c->points[(size_t)selectedPoint].curve)):"LINEAR",{r.getRight()-294,r.getY()+4,69,23},automationHold,0,hold);
    button(g,"Reset curve",{r.getRight()-220,r.getY()+4,87,23},automationReset);button(g,"Playlist",{r.getRight()-128,r.getY()+4,70,23},arrangeTab);
    button(g,snap>0?"Snap":"Free",{r.getRight()-53,r.getY()+4,47,23},snapMenu);
    automationRect={r.getX()+48,r.getY()+83,r.getWidth()-63,r.getHeight()-112};
    g.setColour(Colour(0xff293439));g.fillRect(automationRect);
    for(int n=0;n<=4;++n){float y=automationRect.getBottom()-n*.25f*automationRect.getHeight();g.setColour(Colour(0xff58656b));g.drawHorizontalLine((int)y,automationRect.getX(),automationRect.getRight());label(g,String(n*25)+"%",{r.getX()+3,y-8,40,16},10,muted,false,Justification::centredRight);}
    double step=jmax(1.,std::ceil(c->length/16));for(double beat=0;beat<=c->length;beat+=step){float x=automationRect.getX()+(float)(beat/c->length)*automationRect.getWidth();g.setColour(Colour(0xff46535a));g.drawVerticalLine((int)x,automationRect.getY(),automationRect.getBottom());label(g,String(beat,0),{x+3,automationRect.getY()-21,45,18},10,muted);}
    hits.push_back({automationRect,automationGrid});
    Path curve;int pixels=(int)automationRect.getWidth();for(int x=0;x<=pixels;++x){float y=automationRect.getBottom()-automationValue(c->points.data(),(int)c->points.size(),x/(double)pixels*c->length)*automationRect.getHeight();if(x==0)curve.startNewSubPath(automationRect.getX(),y);else curve.lineTo(automationRect.getX()+x,y);}
    auto area=curve;area.lineTo(automationRect.getRight(),automationRect.getBottom());area.lineTo(automationRect.getX(),automationRect.getBottom());area.closeSubPath();g.setColour(accent.withAlpha(.12f));g.fillPath(area);g.setColour(accent);g.strokePath(curve,PathStrokeType(1.8f));
    for(int i=0;i<(int)c->points.size();++i){const auto& p=c->points[(size_t)i];if(p.beat>c->length)continue;float x=automationRect.getX()+(float)(p.beat/c->length)*automationRect.getWidth(),y=automationRect.getBottom()-p.value*automationRect.getHeight();g.setColour(i==selectedPoint?Colours::white:accent);g.fillEllipse(x-4,y-4,8,8);hits.push_back({{x-7,y-7,14,14},automationPoint,i});}
    if(engine->playing.load()&&project.songMode){double at=engine->position.load()-c->beat;if(at>=0&&at<=c->length){g.setColour(Colour(0xffb2c98d));g.drawVerticalLine((int)(automationRect.getX()+at/c->length*automationRect.getWidth()),automationRect.getY(),automationRect.getBottom());}}
    String value=selectedPoint>=0&&selectedPoint<(int)c->points.size()?"Point "+String(selectedPoint+1)+"  |  "+String(c->points[(size_t)selectedPoint].beat,3)+" beats  |  "+String(c->points[(size_t)selectedPoint].value*100,1)+"%  |  "+String(c->points[(size_t)selectedPoint].hold?"Hold":automationCurveName(c->points[(size_t)selectedPoint].curve))+" / tension "+String(c->points[(size_t)selectedPoint].tension,2):"Click to add points; drag to edit; right-click deletes interior points.";
    label(g,value,{automationRect.getX(),r.getBottom()-24,automationRect.getWidth(),20},10,muted);
}
}
