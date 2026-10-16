// SPDX-License-Identifier: AGPL-3.0-only
#include "ControlMailbox.h"
namespace pulse {
using namespace juce;
namespace {
bool write(const File& file,const var& value){TemporaryFile tmp(file);return tmp.getFile().replaceWithText(JSON::toString(value,false,16))&&tmp.overwriteTargetFileWithTemporary();}
}
Result ControlMailbox::start(const File& workspace){
    stop();root=workspace.getChildFile("data/control");sessionId=Uuid().toString().removeCharacters("-");token=Uuid().toString()+Uuid().toString();folder=root.getChildFile(sessionId);auto result=folder.getChildFile("requests").createDirectory();if(result.wasOk())result=folder.getChildFile("responses").createDirectory();if(result.failed()){sessionId.clear();return result;}
    auto* metadata=new DynamicObject();metadata->setProperty("schema",1);metadata->setProperty("session",sessionId);metadata->setProperty("token",token);metadata->setProperty("application","Pulse Studio");metadata->setProperty("version","0.37.0");metadata->setProperty("startedAt",Time::currentTimeMillis());if(!write(root.getChildFile("session.json"),var(metadata))){sessionId.clear();return Result::fail("Could not publish the local MCP session.");}return Result::ok();
}
void ControlMailbox::stop(){if(sessionId.isNotEmpty()){auto file=root.getChildFile("session.json");auto metadata=JSON::parse(file.loadFileAsString());if(metadata["session"].toString()==sessionId)file.deleteFile();}sessionId.clear();token.clear();seen.clear();order.clear();}
void ControlMailbox::poll(const std::function<Result(const String&,const var&,var&)>& handler){
    if(!enabled())return;auto requests=folder.getChildFile("requests").findChildFiles(File::findFiles,false,"*.json");int processed=0;
    for(const auto& file:requests){if(processed++>=2)break;auto id=file.getFileNameWithoutExtension();if(id.length()!=32||!id.containsOnly("0123456789abcdef")){file.deleteFile();continue;}if(seen.count(id)){file.deleteFile();continue;}
        auto response=folder.getChildFile("responses").getChildFile(id+".json");seen.insert(id);order.push_back(id);if(order.size()>4096){seen.erase(order.front());order.pop_front();}
        var value;Result result=Result::fail("Invalid local control request.");var request;if(file.getSize()<=2*1024*1024)result=JSON::parse(file.loadFileAsString(),request);file.deleteFile();
        if(result.wasOk()){
            auto now=Time::currentTimeMillis();auto expires=request["expiresAt"];auto created=request["createdAt"];
            if(!request.isObject()||!request["schema"].isInt()||(int)request["schema"]!=1||request["id"].toString()!=id||request["session"].toString()!=sessionId||request["token"].toString()!=token)result=Result::fail("The local control session changed or is unauthorized.");
            else if(!(expires.isInt64()||expires.isInt())||!(created.isInt64()||created.isInt())||(int64)expires<now||(int64)created>now+1000||(int64)created<now-20000||(int64)expires>(int64)created+20000)result=Result::fail("The local control request expired.");
            else if(!request["command"].isString()||!request["arguments"].isObject())result=Result::fail("Invalid command or arguments.");
            else result=handler(request["command"].toString(),request["arguments"],value);
        }
        auto* reply=new DynamicObject();reply->setProperty("schema",1);reply->setProperty("id",id);reply->setProperty("ok",result.wasOk());if(result.wasOk())reply->setProperty("result",value);else reply->setProperty("error",result.getErrorMessage());write(response,var(reply));
    }
}
}
