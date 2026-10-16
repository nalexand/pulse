// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <algorithm>
namespace pulse {
using namespace juce;
bool Studio::templateMatches(const ProjectTemplate& info)const{
    if(templateGroup==1&&!info.builtIn)return false;if(templateGroup==2&&info.builtIn)return false;
    return templateQuery.isEmpty()||(info.name+" "+info.description+" "+info.category).containsIgnoreCase(templateQuery);
}
void Studio::refreshTemplates(){
    if(templateBusy){status="Template library is busy.";return;}if(templateThread.joinable())templateThread.join();templateBusy=true;templatesNeedRefresh=false;templateTask=1;templateDone.store(false);templateCancel.store(false);auto library=root.getChildFile("data/Templates");templateMessage="Reading local templates...";
    templateThread=std::thread([this,library]{templateWorkerRows=scanProjectTemplates(library,templateWorkerMessage,&templateCancel);templateDone.store(true,std::memory_order_release);});
}
void Studio::saveTemplateCopy(Project source,const File& base,const String& name,const String& description,TemplateContent content){
    if(templateBusy){status="Wait for the current template operation.";return;}if(templateThread.joinable())templateThread.join();templateBusy=true;templateTask=2;templateDone.store(false);templateSaved=false;auto library=root.getChildFile("data/Templates");templateMessage="Saving template and collecting its samples...";
    templateThread=std::thread([this,source=std::move(source),base,library,name,description,content]{ProjectTemplate saved;auto result=saveProjectTemplate(source,base,library,name,description,content,saved);templateSaved=result.wasOk();templateSavedId=result.wasOk()?saved.id:String();templateWorkerMessage=result.wasOk()?"Saved template: "+saved.name:result.getErrorMessage();templateDone.store(true,std::memory_order_release);});
}
void Studio::finishTemplateTask(){
    if(!templateBusy||!templateDone.load(std::memory_order_acquire))return;if(templateThread.joinable())templateThread.join();templateBusy=false;
    if(templateTask==2){status=templateWorkerMessage;templateMessage=templateWorkerMessage;if(templateSaved){templateQuery.clear();templateGroup=2;templateScroll=0;showPane(templatesPane);refreshTemplates();}else{assetRevealMissing=true;assetNeedsRefresh=true;}repaint();return;}
    String previous=templateSavedId;if(previous.isEmpty()&&selectedTemplate>=0&&selectedTemplate<(int)templateRows.size())previous=templateRows[(size_t)selectedTemplate].id;
    templateRows=std::move(templateWorkerRows);templateMessage=templateWorkerMessage;selectedTemplate=-1;
    for(int i=0;i<(int)templateRows.size();++i)if(templateRows[(size_t)i].id==previous&&templateMatches(templateRows[(size_t)i])){selectedTemplate=i;break;}
    if(selectedTemplate<0)for(int i=0;i<(int)templateRows.size();++i)if(templateMatches(templateRows[(size_t)i])){selectedTemplate=i;break;}
    templateSavedId.clear();templateScroll=0;repaint();
}
void Studio::saveTemplateDialog(){
    if(pendingNoteTool()){status="Apply or cancel the note preview before saving a template.";showPane(noteToolsPane);return;}
    if(templateBusy){status="Wait for the template library to finish.";return;}releaseKeys();auto source=project;auto base=currentFile==File()?root:currentFile.getParentDirectory();
    auto* dialog=new AlertWindow("Save project as template","Samples are collected locally. The current project is preserved.",MessageBoxIconType::NoIcon,this);
    dialog->addTextEditor("name",project.title,"Name");dialog->addTextEditor("description","","Description");dialog->addComboBox("content",{"Complete project","Channels and mixer only"},"Include");dialog->getComboBoxComponent("content")->setSelectedItemIndex(1);dialog->addButton("Save template",1,KeyPress(KeyPress::returnKey));dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));auto safe=Component::SafePointer<Studio>(this);
    dialog->enterModalState(true,ModalCallbackFunction::create([safe,dialog,source=std::move(source),base](int result){if(!safe||result!=1)return;auto name=dialog->getTextEditorContents("name").trim();if(name.isEmpty()){safe->status="Enter a name for the template.";return;}auto content=dialog->getComboBoxComponent("content")->getSelectedItemIndex()==1?TemplateContent::channelsAndMixer:TemplateContent::complete;safe->saveTemplateCopy(source,base,name,dialog->getTextEditorContents("description"),content);safe->repaint();}),true);
}
bool Studio::startFromTemplate(const ProjectTemplate& info){
    if(recording||recorder.isActive()){status="Finish the take before starting another session.";return false;}
    Project next;auto result=loadProjectTemplate(info,next);if(result.failed()){status=result.getErrorMessage();return false;}
    releaseKeys();auto history=root.getChildFile("data/History");result=history.createDirectory();if(result.failed()){status="Could not back up the current session: "+result.getErrorMessage();return false;}
    auto backup=history.getNonexistentChildFile("Before_template_"+Time::getCurrentTime().formatted("%Y%m%d_%H%M%S")+"_"+Uuid().toString().substring(0,8),".pulse");result=saveProject(absoluteAssets(project,currentFile==File()?root:currentFile.getParentDirectory()),backup);if(result.failed()){status="Current session was kept: "+result.getErrorMessage();return false;}
    checkpoint();engine->preview(nullptr);engine->stop();recording=false;project=std::move(next);project.title=("Untitled - "+info.name).substring(0,200);currentFile=File();lastTemplateBackup=backup;
    selectedTrack=0;trackScroll=0;selectedNote=-1;selectedNotes.clear();selectedClip=-1;selectedPoint=-1;clipboardHasClip=false;pianoLow=-1;pianoStart=0;pianoSpan=jmin(8.,project.patterns[(size_t)project.activePattern].length);playlistStart=0;playlistSpan=40;editCursorBeat=0;
    changed();assetRevealMissing=true;panes[templatesPane].visible=false;showPane(project.songMode?playlistPane:rackPane);status="Started from "+info.name+". Previous session backed up in History; Ctrl+Z restores it.";return true;
}
void Studio::paintTemplates(Graphics& g,Rectangle<float> r){
    const Colour text(0xffd9ddde),muted(0xffa5b0b2),accent(0xffb8c997);g.setColour(Colour(0xff394349));g.fillRect(r);float x=r.getX()+10,y=r.getY()+8;
    button(g,"Refresh",{x,y,62,25},templatesRefresh);button(g,"Search",{x+67,y,62,25},templateFind);button(g,"All",{x+134,y,39,25},templateFilter,0,templateGroup==0);button(g,"Built-in",{x+178,y,63,25},templateFilter,1,templateGroup==1);button(g,"Personal",{x+246,y,70,25},templateFilter,2,templateGroup==2);
    button(g,"Save current...",{r.getRight()-282,y,115,25},templateSave);button(g,"New from template",{r.getRight()-161,y,150,25},templateStart,0,false,true);
    label(g,templateMessage+(templateQuery.isEmpty()?String():"  Filter: "+templateQuery),{x,y+32,r.getWidth()-20,20},11,muted);
    float leftWidth=jmin(290.f,r.getWidth()*.38f);templateListRect={x,y+59,leftWidth,r.getHeight()-104};g.setColour(Colour(0xff2c383d));g.fillRect(templateListRect);
    g.saveState();g.reduceClipRegion(templateListRect.toNearestInt());int row=0,skipped=0;
    for(int i=0;i<(int)templateRows.size();++i){auto& info=templateRows[(size_t)i];if(!templateMatches(info))continue;if(skipped++<templateScroll)continue;float top=templateListRect.getY()+row*48;if(top+47>templateListRect.getBottom())break;
        if(i==selectedTemplate){g.setColour(Colour(0xff536451));g.fillRect(Rectangle<float>(x+1,top,leftWidth-2,46));}label(g,info.name,{x+10,top+3,leftWidth-20,23},12,text,true);label(g,info.category+(info.builtIn?" / Built-in":" / Personal"),{x+10,top+25,leftWidth-20,17},10,muted);hits.push_back({{x,top,leftWidth,47},templateSelect,i});++row;
    }
    if(row==0)label(g,templateBusy?"Reading templates...":"No matching templates",templateListRect.reduced(10),12,muted);g.restoreState();
    float detailX=x+leftWidth+18,width=r.getRight()-detailX-12;
    if(selectedTemplate>=0&&selectedTemplate<(int)templateRows.size()){
        auto& info=templateRows[(size_t)selectedTemplate];label(g,info.name,{detailX,y+60,width,31},20,text,true);label(g,info.category,{detailX,y+94,width,20},12,accent);
        g.setColour(text);g.setFont(Font(FontOptions("Segoe UI",13,Font::plain)));g.drawFittedText(info.description,Rectangle<float>(detailX,y+126,width,jmax(60.f,r.getHeight()-285)).toNearestInt(),Justification::topLeft,8);
        float summaryY=r.getBottom()-140;label(g,String(info.bpm,1)+" BPM   |   "+String(info.tracks)+" channels",{detailX,summaryY,width,23},12,accent);
        label(g,String(info.patterns)+" patterns   |   "+String(info.notes)+" notes   |   "+String(info.clips)+" Playlist clips",{detailX,summaryY+27,width,22},11,muted);
        label(g,info.content==TemplateContent::channelsAndMixer?"Channels, instruments and mixer settings only":"Complete project content",{detailX,summaryY+53,width,21},11,muted);
        label(g,info.builtIn?"Uses original built-in sounds; no samples required.":"Personal template with its collected local samples.",{detailX,summaryY+80,width,24},11,muted);
    }
    label(g,"New from template backs up your current project. Ctrl+Z restores the previous session.",{x,r.getBottom()-27,r.getWidth()-20,20},10,muted);
}
bool Studio::performTemplateAction(int action,int data){
    if(action<templatesOpen||action>templateStart)return false;if(recording||recorder.isActive()){status="Finish the take before using project templates.";return true;}
    if(action==templatesOpen){showPane(templatesPane);refreshTemplates();return true;}
    if(action==templatesRefresh){refreshTemplates();return true;}
    if(action==templateSelect){if(data>=0&&data<(int)templateRows.size())selectedTemplate=data;return true;}
    if(action==templateFilter){templateGroup=jlimit(0,2,data);templateScroll=0;selectedTemplate=-1;for(int i=0;i<(int)templateRows.size();++i)if(templateMatches(templateRows[(size_t)i])){selectedTemplate=i;break;}return true;}
    if(action==templateFind){auto* dialog=new AlertWindow("Find a project template","Search names, descriptions and categories. Leave blank to show all.",MessageBoxIconType::NoIcon,this);dialog->addTextEditor("query",templateQuery);dialog->addButton("Find",1,KeyPress(KeyPress::returnKey));dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));auto safe=Component::SafePointer<Studio>(this);dialog->enterModalState(true,ModalCallbackFunction::create([safe,dialog](int r){if(!safe||r!=1)return;safe->templateQuery=dialog->getTextEditorContents("query").substring(0,160);safe->perform(templateFilter,safe->templateGroup);}),true);return true;}
    if(templateBusy){status="Wait for the current template operation.";return true;}
    if(action==templateSave)saveTemplateDialog();else if(action==templateStart){if(selectedTemplate>=0&&selectedTemplate<(int)templateRows.size())startFromTemplate(templateRows[(size_t)selectedTemplate]);else status="Choose a template first.";}return true;
}
}
