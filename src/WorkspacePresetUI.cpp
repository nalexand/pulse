// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <cmath>
namespace pulse {
using namespace juce;
WorkspaceLayout Studio::captureWorkspace()const{
    WorkspaceLayout s;s.width=getWidth();s.height=getHeight();s.active=activePane;s.browser=browserVisible;s.browserWidth=browserWidth;s.order=paneOrder;
    for(const auto& p:panes){auto r=p.maximized?p.restoreBounds:p.bounds;s.windows.push_back({r.getX(),r.getY(),r.getWidth(),r.getHeight(),p.visible,p.maximized});}return s;
}
String Studio::workspaceSourceToken()const{return JSON::toString(workspaceToJson(captureWorkspace()),true,12);}
bool Studio::applyWorkspace(const WorkspaceLayout& s,const String& name){
    if(recording){status="Finish the take before changing workspace.";return false;}
    auto valid=validateWorkspace(s);if(valid.failed()){status=valid.getErrorMessage();return false;}
    bool hidesTools=s.windows.size()<=noteToolsPane||!s.windows[noteToolsPane].visible;
    if(pendingNoteTool()&&hidesTools){status="Apply or cancel the note preview before loading a workspace that hides Note tools.";return false;}
    if(noteToolBound&&hidesTools)cancelNoteTool();
    browserVisible=s.browser;browserWidth=s.browserWidth;
    for(int i=0;i<paneCount;++i){auto& p=panes[(size_t)i];if(i<(int)s.windows.size()){auto& r=s.windows[(size_t)i];p.bounds=boundedPane({r.x*getWidth()/s.width,r.y*getHeight()/s.height,r.w*getWidth()/s.width,r.h*getHeight()/s.height},i);p.restoreBounds=p.bounds;p.visible=r.visible;p.maximized=r.maximized;}else{p.bounds=boundedPane(p.bounds,i);p.restoreBounds=p.bounds;p.maximized=false;p.visible=false;}}
    paneOrder=s.order;for(int i=(int)s.windows.size();i<paneCount;++i)paneOrder.push_back(i);
    activePane=s.active;if(!panes[(size_t)activePane].visible)for(auto i=paneOrder.rbegin();i!=paneOrder.rend();++i)if(panes[(size_t)*i].visible){activePane=*i;break;}
    // Focused windows must also be on top; musical selections stay independent.
    if(panes[(size_t)activePane].visible){paneOrder.erase(std::remove(paneOrder.begin(),paneOrder.end(),activePane),paneOrder.end());paneOrder.push_back(activePane);}
    layoutReady=true;saveLayout();status="Workspace: "+name+".";repaint();return true;
}
bool Studio::applyFactoryWorkspace(int index){
    if(index<0||index>2)return false;
    auto s=captureWorkspace();s.browser=true;s.browserWidth=216;for(auto& w:s.windows){w.visible=false;w.maximized=false;}
    float x=220,y=99,w=(float)getWidth()-224,h=(float)getHeight()-121;
    const char* names[]={"Composing","Arranging","Mixing"};
    if(index==0){s.windows[rackPane]={x,y,w,270,true,false};s.windows[pianoPane]={x,y+274,w,h-274,true,false};s.active=pianoPane;}
    if(index==1){s.windows[playlistPane]={x,y,w,h-330,true,false};s.windows[mixerPane]={x,y+h-326,w,326,true,false};s.active=playlistPane;}
    if(index==2){s.windows[mixerPane]={x,y,w,h,true,true};s.active=mixerPane;}
    bool applied=applyWorkspace(s,names[index]);if(applied)selectedWorkspace="factory-"+String(index);return applied;
}
bool Studio::recallWorkspace(const String& id,const String& expected){
    if(workspaceLibraryToken(workspaceLibraryFile())!=expected){status="Workspace library changed. Reopen Workspaces.";return false;}
    WorkspaceLibrary library;auto result=loadWorkspaceLibrary(workspaceLibraryFile(),library);if(result.failed()){status=result.getErrorMessage();return false;}
    for(const auto& p:library)if(p.id==id){if(!applyWorkspace(p.layout,p.name))return false;selectedWorkspace=id;return true;}
    status="Workspace was removed. Reopen Workspaces.";return false;
}
bool Studio::storeWorkspace(WorkspacePreset p,bool replace,const String& expected,const String& source){
    if(recording){status="Finish the take before saving a workspace.";return false;}
    if(source.isNotEmpty()&&source!=workspaceSourceToken()){status="Workspace changed while naming it. Save again to capture the current panels.";return false;}
    auto result=putWorkspacePreset(workspaceLibraryFile(),p,replace,expected);status=result.wasOk()?"Saved local workspace: "+p.name+".":result.getErrorMessage();if(result.wasOk())selectedWorkspace=p.id;repaint();return result.wasOk();
}
void Studio::nameWorkspace(WorkspacePreset p,bool replace,const String& expected,const String& source){
    auto* dialog=new AlertWindow(replace?"Rename workspace":"Save workspace", "Panel positions, visibility, stacking and Browser size only. Choose a unique name (1-64 characters). Interface scaling is a separate preference.",MessageBoxIconType::NoIcon,this);
    dialog->addTextEditor("name",p.name);dialog->addButton(replace?"Rename":"Save",1,KeyPress(KeyPress::returnKey));dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));auto safe=Component::SafePointer<Studio>(this);
    dialog->enterModalState(true,ModalCallbackFunction::create([safe,dialog,p,replace,expected,source](int choice)mutable{if(!safe||choice!=1)return;p.name=dialog->getTextEditorContents("name").trim();safe->storeWorkspace(p,replace,expected,source);}),true);
}
void Studio::confirmWorkspaceDelete(const WorkspacePreset& p,const String& expected){
    auto* dialog=new AlertWindow("Delete workspace?","Delete \""+p.name+"\" from this computer? The current panel arrangement stays in place.",MessageBoxIconType::QuestionIcon,this);
    dialog->addButton("Delete",1);dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));auto safe=Component::SafePointer<Studio>(this);
    dialog->enterModalState(true,ModalCallbackFunction::create([safe,p,expected](int choice){if(!safe||choice!=1||safe->recording)return;auto result=removeWorkspacePreset(safe->workspaceLibraryFile(),p.id,expected);safe->status=result.wasOk()?"Workspace deleted. Current panels are unchanged.":result.getErrorMessage();if(result.wasOk()&&safe->selectedWorkspace==p.id)safe->selectedWorkspace.clear();safe->repaint();}),true);
}
void Studio::showWorkspaceMenu(){
    WorkspaceLibrary library;auto token=workspaceLibraryToken(workspaceLibraryFile());auto loaded=loadWorkspaceLibrary(workspaceLibraryFile(),library);if(loaded.failed())status=loaded.getErrorMessage();
    auto current=std::find_if(library.begin(),library.end(),[&](const WorkspacePreset& p){return p.id==selectedWorkspace;});bool selected=current!=library.end();WorkspacePreset selection=selected?*current:WorkspacePreset{};
    PopupMenu menu,factory,personal;const char* names[]={"Composing","Arranging","Mixing"};for(int i=0;i<3;++i)factory.addItem(100+i,names[i],true,selectedWorkspace=="factory-"+String(i));menu.addSubMenu("Factory",factory);
    for(int i=0;i<(int)library.size();++i)personal.addItem(1000+i,library[(size_t)i].name,true,selectedWorkspace==library[(size_t)i].id);if(library.empty())personal.addItem(999,"No personal workspaces yet",false);menu.addSubMenu("Personal",personal);
    menu.addSeparator();menu.addItem(1,"Save workspace as...",loaded.wasOk()&&library.size()<maxWorkspacePresets);menu.addItem(2,"Update selected with current panels",selected);menu.addItem(3,"Rename selected...",selected);menu.addItem(4,"Delete selected...",selected);menu.addSeparator();menu.addItem(5,"Import workspace...",loaded.wasOk()&&library.size()<maxWorkspacePresets);menu.addItem(6,"Export selected workspace...",selected);if(loaded.failed())menu.addItem(7,"Library unavailable - see status",false);
    auto safe=Component::SafePointer<Studio>(this);auto source=workspaceSourceToken();
    menu.showMenuAsync(menuOptions(menuOpen,4),[safe,library,token,source,selection,selected](int choice){
        if(!safe||!choice)return;if(safe->recording){safe->status="Finish the take before changing workspace.";return;}
        if(choice>=100&&choice<103){safe->applyFactoryWorkspace(choice-100);return;}
        if(workspaceLibraryToken(safe->workspaceLibraryFile())!=token){safe->status="Workspace library changed. Reopen Workspaces.";return;}
        if(choice>=1000&&choice<1000+(int)library.size()){safe->recallWorkspace(library[(size_t)(choice-1000)].id,token);return;}
        if(choice==1){safe->nameWorkspace({"user-"+Uuid().toString(),{},safe->captureWorkspace()},false,token,safe->workspaceSourceToken());return;}
        if(choice==2&&selected){if(source!=safe->workspaceSourceToken()){safe->status="Workspace changed. Reopen Workspaces before updating.";return;}auto p=selection;p.layout=safe->captureWorkspace();safe->storeWorkspace(p,true,token,source);return;}
        if(choice==3&&selected){safe->nameWorkspace(selection,true,token,{});return;}
        if(choice==4&&selected){safe->confirmWorkspaceDelete(selection,token);return;}
        if(choice!=5&&choice!=6)return;bool exporting=choice==6;if(exporting&&!selected)return;
        safe->chooser=std::make_unique<FileChooser>(exporting?"Export workspace":"Import workspace",exporting?safe->root.getChildFile("workspace.pulseworkspace"):safe->root,"*.pulseworkspace",true);
        int flags=exporting?(FileBrowserComponent::saveMode|FileBrowserComponent::canSelectFiles|FileBrowserComponent::warnAboutOverwriting):(FileBrowserComponent::openMode|FileBrowserComponent::canSelectFiles);
        safe->chooser->launchAsync(flags,[safe,selection,exporting,token](const FileChooser& chooser){if(!safe||safe->recording)return;auto file=chooser.getResult();if(file==File())return;if(workspaceLibraryToken(safe->workspaceLibraryFile())!=token){safe->status="Workspace library changed. Reopen Workspaces.";return;}if(exporting){auto result=saveWorkspacePreset(file,selection);safe->status=result.wasOk()?"Exported workspace: "+file.getFileName():result.getErrorMessage();safe->repaint();return;}WorkspacePreset p;auto result=loadWorkspacePreset(file,p);if(result.failed()){safe->status=result.getErrorMessage();safe->repaint();return;}p.id="user-"+Uuid().toString();safe->nameWorkspace(p,false,token,{});});
    });
}
bool Studio::setInterfaceScale(int value){
    if(recording){status="Finish the take before changing interface scaling.";return false;}
    if(!validInterfaceScale(value)){status="Choose a listed interface scale.";return false;}
    auto result=saveInterfaceScale(root.getChildFile("data/interface.json"),value);if(result.failed()){status=result.getErrorMessage();return false;}
    interfaceScalePercent=value;if(interfaceScaleChanged)interfaceScaleChanged();saveLayout();status="Interface scale: "+String(value)+"% requested, "+String((int)std::round(effectiveInterfaceScale*100))+"% fitted to this window.";repaint();return true;
}
void Studio::showInterfaceScaleMenu(){
    PopupMenu menu;menu.addSectionHeader("Interface scale (fitted to window)");for(int v:interfaceScales)menu.addItem(v,String(v)+"%"+(v==100?" (default)":""),true,interfaceScalePercent==v);
    menu.addSectionHeader("Current fitted scale: "+String((int)std::round(effectiveInterfaceScale*100))+"%");auto safe=Component::SafePointer<Studio>(this);menu.showMenuAsync(menuOptions(menuOpen,4),[safe](int choice){if(safe&&choice)safe->setInterfaceScale(choice);});
}
bool Studio::performWorkspaceAction(int action,int data){
    if(action!=workspaceMenu&&action!=workspaceSave&&action!=workspaceFactory&&action!=workspaceRecall&&action!=interfaceScaleMenu&&action!=interfaceScale)return false;
    if(recording){status="Finish the take before changing workspace or interface scaling.";return true;}
    if(action==workspaceMenu)showWorkspaceMenu();
    if(action==workspaceSave){WorkspaceLibrary library;auto result=loadWorkspaceLibrary(workspaceLibraryFile(),library);if(result.failed())status=result.getErrorMessage();else if(library.size()>=maxWorkspacePresets)status="Up to 32 personal workspaces can be saved.";else nameWorkspace({"user-"+Uuid().toString(),{},captureWorkspace()},false,workspaceLibraryToken(workspaceLibraryFile()),workspaceSourceToken());}
    if(action==workspaceFactory)applyFactoryWorkspace(data);
    if(action==workspaceRecall){WorkspaceLibrary library;auto token=workspaceLibraryToken(workspaceLibraryFile());auto result=loadWorkspaceLibrary(workspaceLibraryFile(),library);if(result.failed())status=result.getErrorMessage();else if(data>=0&&data<(int)library.size())recallWorkspace(library[(size_t)data].id,token);}
    if(action==interfaceScaleMenu)showInterfaceScaleMenu();if(action==interfaceScale)setInterfaceScale(data);return true;
}
}
