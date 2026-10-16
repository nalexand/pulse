// SPDX-License-Identifier: AGPL-3.0-only
#include "WorkspaceLayouts.h"
#include <juce_cryptography/juce_cryptography.h>
#include <algorithm>
#include <cmath>
#include <set>
namespace pulse {
using namespace juce;
namespace {
bool number(const var& v){return (v.isInt()||v.isInt64()||v.isDouble())&&std::isfinite((double)v);}
bool integer(const var& v){return v.isInt()||v.isInt64();}
bool validName(const String& s){if(s.isEmpty()||s.length()>64||s!=s.trim())return false;for(auto c:s)if(c<32||c==127)return false;return true;}
Result writeJson(const File& file,const var& json,const String& expected={}){
    auto text=JSON::toString(json,false,12);if(text.getNumBytesAsUTF8()>maxWorkspaceLibraryBytes)return Result::fail("Workspace file exceeds 256 KiB.");
    auto result=file.getParentDirectory().createDirectory();if(result.failed())return result;
    TemporaryFile temp(file);if(!temp.getFile().replaceWithText(text))return Result::fail("Could not write temporary workspace settings.");
    if(expected.isNotEmpty()&&workspaceLibraryToken(file)!=expected)return Result::fail("Workspace library changed while saving. Reopen Workspaces.");
    if(!temp.overwriteTargetFileWithTemporary())return Result::fail("Could not save workspace settings. Existing file is unchanged.");return Result::ok();
}
Result readJson(const File& file,var& json){
    if(!file.existsAsFile()||file.getSize()>maxWorkspaceLibraryBytes)return Result::fail("Choose a workspace file no larger than 256 KiB.");
    auto result=JSON::parse(file.loadFileAsString(),json);return result.failed()?Result::fail("Cannot read workspace JSON: "+result.getErrorMessage()):result;
}
var presetJson(const WorkspacePreset& p){auto* o=new DynamicObject();o->setProperty("id",p.id);o->setProperty("name",p.name);o->setProperty("layout",workspaceToJson(p.layout));return var(o);}
Result parsePreset(const var& json,WorkspacePreset& p){
    if(!json.isObject()||!json["id"].isString()||!json["name"].isString())return Result::fail("Invalid workspace identity or name.");
    WorkspacePreset next;next.id=json["id"].toString();next.name=json["name"].toString();auto result=workspaceFromJson(json["layout"],next.layout);if(result.failed())return result;
    result=validateWorkspaceLibrary({next});if(result.wasOk())p=std::move(next);return result;
}
Result saveLibrary(const File& file,const WorkspaceLibrary& library,const String& expected){
    auto result=validateWorkspaceLibrary(library);if(result.failed())return result;
    if(workspaceLibraryToken(file)!=expected)return Result::fail("Workspace library changed. Reopen Workspaces.");
    auto* o=new DynamicObject();o->setProperty("schema",1);o->setProperty("application","Pulse Studio workspaces");Array<var> rows;for(const auto& p:library)rows.add(presetJson(p));o->setProperty("presets",rows);
    return writeJson(file,var(o),expected);
}
}
Result validateWorkspace(const WorkspaceLayout& s){
    if(s.width<800||s.width>32768||s.height<500||s.height>32768||!std::isfinite(s.browserWidth)||s.browserWidth<160||s.browserWidth>340)return Result::fail("Invalid workspace canvas or Browser size.");
    if(s.windows.size()<5||s.windows.size()>workspacePaneCount||s.order.size()!=s.windows.size()||s.active<0||s.active>=(int)s.windows.size())return Result::fail("Invalid workspace window count or focus.");
    std::set<int> seen;for(int i:s.order)if(i<0||i>=(int)s.windows.size()||!seen.insert(i).second)return Result::fail("Invalid or duplicate workspace stacking order.");
    for(const auto& r:s.windows)if(!std::isfinite(r.x)||!std::isfinite(r.y)||!std::isfinite(r.w)||!std::isfinite(r.h)||std::abs(r.x)>32768||std::abs(r.y)>32768||r.w<1||r.w>32768||r.h<1||r.h>32768)return Result::fail("Invalid workspace window bounds.");
    return Result::ok();
}
var workspaceToJson(const WorkspaceLayout& s){
    auto* o=new DynamicObject();o->setProperty("schema",1);o->setProperty("width",s.width);o->setProperty("height",s.height);o->setProperty("active",s.active);o->setProperty("browser",s.browser);o->setProperty("browserWidth",s.browserWidth);
    Array<var> windows,order;for(const auto& r:s.windows){auto* row=new DynamicObject();row->setProperty("x",r.x);row->setProperty("y",r.y);row->setProperty("w",r.w);row->setProperty("h",r.h);row->setProperty("visible",r.visible);row->setProperty("maximized",r.maximized);windows.add(var(row));}for(int i:s.order)order.add(i);o->setProperty("windows",windows);o->setProperty("order",order);return var(o);
}
Result workspaceFromJson(const var& json,WorkspaceLayout& output){
    if(!json.isObject()||!integer(json["schema"])||(int64)json["schema"]!=1||!integer(json["width"])||!integer(json["height"])||!json["browser"].isBool()||!number(json["browserWidth"])||!json["windows"].isArray()||!json["order"].isArray())return Result::fail("Invalid or unsupported workspace layout.");
    auto& rows=*json["windows"].getArray();auto& order=*json["order"].getArray();if(rows.size()<5||rows.size()>workspacePaneCount||order.size()!=rows.size())return Result::fail("Invalid workspace window count.");
    if((int64)json["width"]<800||(int64)json["width"]>32768||(int64)json["height"]<500||(int64)json["height"]>32768)return Result::fail("Invalid workspace canvas.");
    WorkspaceLayout next;next.width=(int)json["width"];next.height=(int)json["height"];next.browser=(bool)json["browser"];next.browserWidth=(float)json["browserWidth"];
    for(const auto& row:rows){if(!row.isObject()||!number(row["x"])||!number(row["y"])||!number(row["w"])||!number(row["h"])||!row["visible"].isBool()||!row["maximized"].isBool())return Result::fail("Invalid workspace window.");next.windows.push_back({(float)row["x"],(float)row["y"],(float)row["w"],(float)row["h"],(bool)row["visible"],(bool)row["maximized"]});}
    for(const auto& i:order){if(!integer(i)||(int64)i<0||(int64)i>=rows.size())return Result::fail("Invalid workspace order index.");next.order.push_back((int)i);}
    next.active=next.order.back();for(auto i=next.order.rbegin();i!=next.order.rend();++i)if(next.windows[(size_t)*i].visible){next.active=*i;break;}
    if(auto* o=json.getDynamicObject();o->hasProperty("active")){if(!integer(json["active"])||(int64)json["active"]<0||(int64)json["active"]>=rows.size())return Result::fail("Invalid workspace focus.");next.active=(int)json["active"];}
    auto result=validateWorkspace(next);if(result.wasOk())output=std::move(next);return result;
}
Result validateWorkspaceLibrary(const WorkspaceLibrary& library){
    if(library.size()>maxWorkspacePresets)return Result::fail("Up to 32 personal workspaces can be saved.");
    std::set<String> ids,names;for(const auto& p:library){if(!p.id.startsWith("user-")||p.id.length()>64||!p.id.containsOnly("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_")||!ids.insert(p.id).second||!validName(p.name)||!names.insert(p.name.toLowerCase()).second)return Result::fail("Use a unique workspace name (1-64 characters) and identity.");auto valid=validateWorkspace(p.layout);if(valid.failed())return valid;}return Result::ok();
}
String workspaceLibraryToken(const File& file){if(!file.exists())return "missing";if(!file.existsAsFile())return "not-file";if(file.getSize()>maxWorkspaceLibraryBytes)return "oversized";return SHA256(file).toHexString();}
Result loadWorkspaceLibrary(const File& file,WorkspaceLibrary& output){
    if(!file.exists()){output.clear();return Result::ok();}var json;auto result=readJson(file,json);if(result.failed())return result;
    if(!json.isObject()||!integer(json["schema"])||(int64)json["schema"]!=1||json["application"].toString()!="Pulse Studio workspaces"||!json["presets"].isArray()||json["presets"].getArray()->size()>maxWorkspacePresets)return Result::fail("Invalid or unsupported workspace library.");
    WorkspaceLibrary next;for(const auto& row:*json["presets"].getArray()){WorkspacePreset p;result=parsePreset(row,p);if(result.failed())return result;next.push_back(std::move(p));}result=validateWorkspaceLibrary(next);if(result.wasOk())output=std::move(next);return result;
}
Result putWorkspacePreset(const File& file,const WorkspacePreset& p,bool replace,const String& expected){
    if(workspaceLibraryToken(file)!=expected)return Result::fail("Workspace library changed. Reopen Workspaces.");WorkspaceLibrary library;auto result=loadWorkspaceLibrary(file,library);if(result.failed())return result;
    auto it=std::find_if(library.begin(),library.end(),[&](const WorkspacePreset& row){return row.id==p.id;});if(replace){if(it==library.end())return Result::fail("Workspace was removed. Reopen Workspaces.");*it=p;}else{if(it!=library.end())return Result::fail("Workspace identity already exists.");library.push_back(p);}return saveLibrary(file,library,expected);
}
Result removeWorkspacePreset(const File& file,const String& id,const String& expected){
    if(workspaceLibraryToken(file)!=expected)return Result::fail("Workspace library changed. Reopen Workspaces.");WorkspaceLibrary library;auto result=loadWorkspaceLibrary(file,library);if(result.failed())return result;
    auto it=std::find_if(library.begin(),library.end(),[&](const WorkspacePreset& row){return row.id==id;});if(it==library.end())return Result::fail("Workspace was already removed.");library.erase(it);return saveLibrary(file,library,expected);
}
Result saveWorkspacePreset(const File& file,const WorkspacePreset& p){auto valid=validateWorkspaceLibrary({p});if(valid.failed())return valid;auto* o=new DynamicObject();o->setProperty("schema",1);o->setProperty("application","Pulse Studio workspace");o->setProperty("preset",presetJson(p));return writeJson(file,var(o));}
Result loadWorkspacePreset(const File& file,WorkspacePreset& output){var json;auto valid=readJson(file,json);if(valid.failed())return valid;if(!json.isObject()||!integer(json["schema"])||(int64)json["schema"]!=1||json["application"].toString()!="Pulse Studio workspace")return Result::fail("Choose a .pulseworkspace exported by Pulse Studio.");return parsePreset(json["preset"],output);}
bool validInterfaceScale(int value){return std::find(std::begin(interfaceScales),std::end(interfaceScales),value)!=std::end(interfaceScales);}
Result loadInterfaceScale(const File& file,int& output){
    if(!file.exists()){output=100;return Result::ok();}var json;auto result=readJson(file,json);if(result.failed())return result;if(!json.isObject()||!integer(json["schema"])||(int64)json["schema"]!=1||json["application"].toString()!="Pulse Studio interface"||!integer(json["scale"])||(int64)json["scale"]<75||(int64)json["scale"]>200||!validInterfaceScale((int)json["scale"]))return Result::fail("Invalid local interface settings. Existing file was preserved.");output=(int)json["scale"];return Result::ok();
}
Result saveInterfaceScale(const File& file,int value){
    if(!validInterfaceScale(value))return Result::fail("Choose an interface scale between 75% and 200%.");int previous=100;auto result=loadInterfaceScale(file,previous);if(result.failed())return result;auto* o=new DynamicObject();o->setProperty("schema",1);o->setProperty("application","Pulse Studio interface");o->setProperty("scale",value);return writeJson(file,var(o));
}
}
