// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <cstdlib>
#include <cmath>
namespace pulse {
using namespace juce;
namespace {
bool readNumber(const String& text,double& result){auto clean=text.trim();auto* begin=clean.toRawUTF8();char* end=nullptr;double value=std::strtod(begin,&end);if(clean.isEmpty()||begin==end||*end||!std::isfinite(value))return false;result=value;return true;}
class GeneratorPanel final:public Component {
public:
    GeneratorPanel(double clipLength,const AutomationGenerator& initial,int width,int height):length(clipLength),settings(initial){
        setSize(width,height);shape.setComponentID("automation-wave");for(int i=0;i<6;++i)shape.addItem(automationWaveName((AutomationWave)i),i+1);shape.setSelectedId((int)initial.wave+1,dontSendNotification);addAndMakeVisible(shape);
        const char* keys[]={"period","phase","low","high","resolution","seed","steps"};const char* names[]={"Period (beats)","Phase (0-1)","Minimum (0-1)","Maximum (0-1)","Resolution (even 4-64)","Random seed","Step values (0-1, comma-separated)"};
        for(int i=0;i<7;++i){labels[(size_t)i].setText(names[i],dontSendNotification);labels[(size_t)i].setFont(Font(FontOptions(11)));addAndMakeVisible(labels[(size_t)i]);auto& e=fields[(size_t)i];e.setComponentID("automation-"+String(keys[i]));e.setInputRestrictions(i==6?512:24);addAndMakeVisible(e);e.onTextChange=[this]{refresh();};}
        fields[0].setText(String(initial.period,6),false);fields[1].setText(String(initial.phase,6),false);fields[2].setText(String(initial.low,6),false);fields[3].setText(String(initial.high,6),false);fields[4].setText(String(initial.resolution),false);fields[5].setText(String((int64)initial.seed),false);StringArray values;for(float value:initial.steps)values.add(String(value,5));fields[6].setText(values.joinIntoString(", "),false);
        shape.onChange=[this]{refresh();};refresh();
    }
    Result draft(AutomationGenerator& out,std::vector<AutomationPoint>& points)const{
        AutomationGenerator next=settings;next.wave=(AutomationWave)(shape.getSelectedId()-1);double resolution=0,seed=0;
        if(!readNumber(fields[0].getText(),next.period)||!readNumber(fields[1].getText(),next.phase)||!readNumber(fields[2].getText(),next.low)||!readNumber(fields[3].getText(),next.high)||!readNumber(fields[4].getText(),resolution)||!readNumber(fields[5].getText(),seed)||resolution!=std::floor(resolution)||resolution<4||resolution>64||seed!=std::floor(seed)||seed<0||seed>4294967295.)return Result::fail("Enter finite numbers, an even resolution 4-64 and a seed 0-4294967295.");
        next.resolution=(int)resolution;next.seed=(uint32_t)seed;next.steps.clear();auto values=StringArray::fromTokens(fields[6].getText(),",","");if(values.size()<2||values.size()>32)return Result::fail("Enter 2-32 comma-separated step values.");for(const auto& text:values){double value=0;if(!readNumber(text,value)||value<0||value>1)return Result::fail("Step values must be numbers from 0 to 1.");next.steps.push_back((float)value);}
        auto result=generateAutomation(length,next,points);if(result.failed())return result;out=std::move(next);return Result::ok();
    }
    void resized()override{bool compact=getHeight()<360;int w=getWidth()-20,column=(w-10)/2;shape.setBounds(10,5,w,27);for(int i=0;i<6;++i){int x=10+(i%2)*(column+10),y=42+(i/2)*(compact?43:49);labels[(size_t)i].setBounds(x,y,column,compact?16:18);fields[(size_t)i].setBounds(x,y+(compact?18:19),column,compact?23:25);}labels[6].setBounds(10,compact?174:192,w,18);fields[6].setBounds(10,compact?192:212,w,25);}
    void paint(Graphics& g)override{
        float graphTop=getHeight()<360?224.f:245.f;Rectangle<float> graph{10,graphTop,(float)getWidth()-20,jmax(1.f,getHeight()-graphTop-50)};g.setColour(Colour(0xff253035));g.fillRect(graph);if(!candidate.empty()){Path path;int width=(int)graph.getWidth();for(int x=0;x<=width;++x){float y=graph.getBottom()-automationValue(candidate.data(),(int)candidate.size(),length*x/width)*graph.getHeight();if(x==0)path.startNewSubPath(graph.getX(),y);else path.lineTo(graph.getX()+x,y);}g.setColour(Colour(0xffd3af71));g.strokePath(path,PathStrokeType(1.8f));}
        g.setColour(valid?Colour(0xffb8c997):Colour(0xffe7b2a3));g.setFont(Font(FontOptions(11)));g.drawFittedText(message,{10,getHeight()-43,getWidth()-20,36},Justification::centredLeft,2);
    }
private:
    void refresh(){AutomationGenerator checked;std::vector<AutomationPoint> points;auto result=draft(checked,points);valid=result.wasOk();candidate=valid?std::move(points):std::vector<AutomationPoint>{};message=valid?String((int)candidate.size())+" points / "+String(length,3)+" beats. Preview only; Apply replaces this clip's curve as one Undo.":result.getErrorMessage();repaint();}
    double length;AutomationGenerator settings;ComboBox shape;std::array<Label,7> labels;std::array<TextEditor,7> fields;std::vector<AutomationPoint> candidate;bool valid=false;String message;
};
}
std::optional<Studio::AutomationContext> Studio::automationToolContext()const{
    if(recording||recorder.isActive()||pendingNoteTool()||selectedClip<0||selectedClip>=(int)project.clips.size()||!project.clips[(size_t)selectedClip].isAutomation())return {};
    const auto& c=project.clips[(size_t)selectedClip];return AutomationContext{revision,selectedClip,c.editorId,c.length,c.points};
}
bool Studio::automationToolCurrent(const AutomationContext& context)const{
    auto now=automationToolContext();return now&&now->revision==context.revision&&now->clip==context.clip&&now->id==context.id&&now->length==context.length&&now->points==context.points;
}
bool Studio::applyAutomationPoints(const AutomationContext& context,const std::vector<AutomationPoint>& points,bool endpoints){
    if(!automationToolCurrent(context)){status="Automation clip or project changed. Reopen the tool.";return false;}
    auto next=project;auto valid=replaceAutomationPoints(next,context.clip,points,endpoints);if(valid.failed()){status=valid.getErrorMessage();return false;}
    if(points==context.points){status="No automation changes to apply.";return true;}
    auto playback=std::make_unique<Playback>();String warnings;auto base=currentFile==File()?root:currentFile.getParentDirectory();valid=compilePlayback(next,assets,base,*playback,warnings);if(valid.wasOk()){next.songMode=!next.songMode;valid=compilePlayback(next,assets,base,*playback,warnings);}if(valid.failed()){status=valid.getErrorMessage();return false;}
    checkpoint();project.clips[(size_t)context.clip].points=points;selectedPoint=-1;changed();showPane(automationPane);status="Automation curve applied. One Undo restores the previous points.";return true;
}
void Studio::showAutomationShape(){
    auto context=automationToolContext();if(!context){status="Select an automation clip and finish the take/note preview first.";return;}
    int point=selectedPoint;bool segment=point>=0&&point<(int)context->points.size()-1;auto value=segment?context->points[(size_t)point]:context->points.front();
    auto* dialog=new AlertWindow("Automation segment shape","Shape belongs to the outgoing segment. Tension -1 to +1 warps its timing while retaining endpoint values. Hold keeps the left value until the next point.",MessageBoxIconType::NoIcon,this);
    dialog->addComboBox("scope",{"Selected segment","All segments"},"Apply to");dialog->getComboBoxComponent("scope")->setSelectedItemIndex(segment?0:1);dialog->addComboBox("curve",{"Linear","Smooth","Ease in","Ease out","Hold / step"},"Shape");dialog->getComboBoxComponent("curve")->setSelectedItemIndex(value.hold?4:(int)value.curve);dialog->addTextEditor("tension",String(value.tension,5),"Tension (-1 to +1; ignored by Hold)");dialog->addButton("Apply",1,KeyPress(KeyPress::returnKey));dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));auto safe=Component::SafePointer<Studio>(this);
    dialog->enterModalState(true,ModalCallbackFunction::create([safe,dialog,context=*context,point,segment](int result){if(!safe||result!=1)return;double tension=0;int curve=dialog->getComboBoxComponent("curve")->getSelectedItemIndex();bool all=dialog->getComboBoxComponent("scope")->getSelectedItemIndex()==1;if(!readNumber(dialog->getTextEditorContents("tension"),tension)||tension< -1||tension>1||curve<0||curve>4||(!all&&(!segment||safe->selectedPoint!=point))){safe->status="Use a current segment and tension from -1 to +1.";return;}auto points=context.points;for(int i=0;i<(int)points.size()-1;++i)if(all||i==point){auto& p=points[(size_t)i];p.hold=curve==4;p.curve=curve==4?AutomationCurve::linear:(AutomationCurve)curve;p.tension=curve==4?0:(float)tension;}safe->applyAutomationPoints(context,points,false);}),true);
}
void Studio::showAutomationGenerator(){
    auto context=automationToolContext();if(!context){status="Select an automation clip and finish the take/note preview first.";return;}
    auto area=getParentMonitorArea();int width=jlimit(360,560,(int)(area.getWidth()*.8)-40),height=jlimit(300,400,area.getHeight()-200);
    auto* dialog=new AlertWindow("Generate automation","Preview the curve, then Apply. Undo restores the previous curve.",MessageBoxIconType::NoIcon,this);auto panel=std::make_shared<GeneratorPanel>(context->length,automationGenerator,width,height);panel->setComponentID("automation-generator-panel");dialog->addCustomComponent(panel.get());dialog->addButton("Apply",1,KeyPress(KeyPress::returnKey));dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));auto safe=Component::SafePointer<Studio>(this);
    dialog->enterModalState(true,ModalCallbackFunction::create([safe,dialog,panel,context=*context](int result){if(!safe||result!=1)return;AutomationGenerator settings;std::vector<AutomationPoint> points;auto valid=panel->draft(settings,points);if(valid.failed()){safe->status=valid.getErrorMessage();return;}if(safe->applyAutomationPoints(context,points))safe->automationGenerator=std::move(settings);}),true);
}
bool Studio::performAutomationToolAction(int action){if(action!=automationShapeOpen&&action!=automationGenerateOpen)return false;if(action==automationShapeOpen)showAutomationShape();else showAutomationGenerator();return true;}
}
