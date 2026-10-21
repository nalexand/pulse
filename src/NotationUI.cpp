// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
namespace pulse {
using namespace juce;
namespace {
class ScorePage final:public Component {
public:
    bool setScore(const String& svg){auto xml=XmlDocument::parse(svg);drawing=xml?Drawable::createFromSVG(*xml):nullptr;repaint();return drawing!=nullptr;}
    void paint(Graphics& g)override{g.fillAll(Colours::whitesmoke);if(drawing)drawing->drawWithin(g,getLocalBounds().toFloat(),RectanglePlacement::stretchToFit,1);}
private:std::unique_ptr<Drawable> drawing;
};
class NotationView final:public Component {
public:
    explicit NotationView(Project snapshot,int pattern,int track,ScoreClef clef):source(std::move(snapshot)),patternIndex(pattern){
        setComponentID("notation-view");channels.setComponentID("notation-channel");clefs.setComponentID("notation-clef");viewport.setComponentID("notation-viewport");report.setComponentID("notation-report");
        for(int i=0;i<(int)source.tracks.size();++i)if(!source.tracks[(size_t)i].isBus&&!isDrum(source.tracks[(size_t)i].instrument)){tracks.push_back(i);channels.addItem(String(i+1)+". "+source.tracks[(size_t)i].name,i+1);}
        channels.setSelectedId(track+1,dontSendNotification);if(channels.getSelectedId()==0&&!tracks.empty())channels.setSelectedId(tracks.front()+1,dontSendNotification);
        clefs.addItemList({"Automatic clef","Treble clef","Bass clef"},1);clefs.setSelectedItemIndex((int)clef,dontSendNotification);
        channelLabel.setText("Instrument channel",dontSendNotification);clefLabel.setText("Written clef",dontSendNotification);
        for(auto* c:std::initializer_list<Component*>{&channelLabel,&clefLabel,&channels,&clefs,&viewport,&report})addAndMakeVisible(c);
        viewport.setViewedComponent(&page,false);viewport.setScrollBarsShown(true,false);report.setMultiLine(true,true);report.setReadOnly(true);report.setScrollbarsShown(true);
        channels.onChange=clefs.onChange=[this]{update();};setSize(760,460);update();
    }
    ~NotationView()override{viewport.setViewedComponent(nullptr,false);}
    void resized()override{auto w=getWidth();channelLabel.setBounds(0,0,w*2/3-10,20);clefLabel.setBounds(w*2/3,0,w/3,20);channels.setBounds(0,23,w*2/3-10,28);clefs.setBounds(w*2/3,23,w/3,28);viewport.setBounds(0,62,w,getHeight()-155);report.setBounds(0,getHeight()-84,w,84);layoutPage();}
    int track()const{return channels.getSelectedId()-1;}
    ScoreClef clef()const{return (ScoreClef)clefs.getSelectedItemIndex();}
    PatternScore score;Result result=Result::fail("No score");
private:
    void layoutPage(){if(svg.isEmpty())return;auto xml=XmlDocument::parse(svg);if(!xml)return;int width=jmax(200,viewport.getWidth()-viewport.getScrollBarThickness()-2);page.setSize(width,jmax(1,(int)std::ceil(xml->getDoubleAttribute("height")*width/1000)));}
    void update(){PatternScore next;result=buildPatternScore(source,patternIndex,track(),clef(),next);svg=result.wasOk()?scoreSvg(next):String();
        if(result.wasOk()&&(svg.isEmpty()||svg.getNumBytesAsUTF8()>4*1024*1024)){result=Result::fail("This sheet exceeds the bounded preview budget. Simplify the channel.");svg.clear();}
        if(result.wasOk()){score=std::move(next);report.setText(scoreSummary(score),false);}else report.setText(result.getErrorMessage(),false);
        if(!page.setScore(svg)&&result.wasOk()){result=Result::fail("Cannot draw this score. Simplify the channel or reopen the view.");report.setText(result.getErrorMessage(),false);}layoutPage();viewport.setViewPosition(0,0);
    }
    Project source;int patternIndex;std::vector<int> tracks;String svg;Label channelLabel,clefLabel;ComboBox channels,clefs;Viewport viewport;ScorePage page;TextEditor report;
};
class NotationDialog final:public AlertWindow {
public:
    NotationDialog(const Project& p,int pattern,int track,ScoreClef clef,Component* parent):AlertWindow("Pattern notation","Read-only pattern snapshot. Nearest 1/16 timing in 4/4; source notes and playback stay unchanged. Switch channel/clef; scroll the sheet; export locally.",MessageBoxIconType::NoIcon,parent),view(p,pattern,track,clef){
        auto area=parent->getParentMonitorArea();view.setSize(jmax(260,jmin(820,(int)(area.getWidth()*.64))),jmax(260,jmin(560,area.getHeight()-260)));addCustomComponent(&view);
        addButton("Export MusicXML...",1);addButton("Export SVG...",2);addButton("Close",0,KeyPress(KeyPress::escapeKey));
    }
    NotationView view;
};
}
void Studio::showNotation(){
    closeCommandPalette();int track=selectedTrack;if(track<0||track>=(int)project.tracks.size()||project.tracks[(size_t)track].isBus||isDrum(project.tracks[(size_t)track].instrument)){
        track=-1;for(int i=0;i<(int)project.tracks.size();++i)if(!project.tracks[(size_t)i].isBus&&!isDrum(project.tracks[(size_t)i].instrument)){track=i;break;}
    }
    if(track<0){status="Add a pitched instrument channel for pattern notation.";return;}
    int pattern=project.activePattern;auto expected=revision;auto* d=new NotationDialog(project,pattern,track,notationClef,this);auto safe=Component::SafePointer<Studio>(this);
    d->enterModalState(true,ModalCallbackFunction::create([safe,d,expected](int answer){if(!safe)return;safe->notationClef=d->view.clef();if(answer!=1&&answer!=2)return;
        if(d->view.result.failed()){safe->status=d->view.result.getErrorMessage();return;}if(safe->revision!=expected){safe->status="Project changed. Reopen notation to export a fresh snapshot.";return;}
        safe->chooseNotationExport(d->view.score,answer==2,expected);
    }),true);
}
bool Studio::exportNotation(const File& file,const PatternScore& score,bool svg,uint64_t expected){
    if(expected!=revision||recording||recorder.isActive()){status="Project changed or a take is active. Reopen notation before export.";return false;}
    auto result=savePatternScore(file,score,svg);status=result.wasOk()?"Notation snapshot saved: "+file.getFullPathName():result.getErrorMessage();return result.wasOk();
}
void Studio::chooseNotationExport(const PatternScore& score,bool svg,uint64_t expected){
    if(expected!=revision||recording||recorder.isActive()){status="Project changed or a take is active. Reopen notation before export.";return;}
    auto name=(score.title+" - "+score.pattern+" - "+score.channel).replaceCharacters("/\\:*?\"<>|","_________").substring(0,160);
    notationChooser=std::make_unique<FileChooser>(svg?"Export notation SVG":"Export notation MusicXML",root.getChildFile("Exports").getChildFile(name+(svg?".svg":".musicxml")),svg?"*.svg":"*.musicxml",true);
    auto safe=Component::SafePointer<Studio>(this);notationChooser->launchAsync(FileBrowserComponent::saveMode|FileBrowserComponent::canSelectFiles|FileBrowserComponent::warnAboutOverwriting,[safe,score,svg,expected](const FileChooser& fc){if(safe&&fc.getResult()!=File())safe->exportNotation(fc.getResult(),score,svg,expected);});
}
}
