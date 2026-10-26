// SPDX-License-Identifier: AGPL-3.0-only
#include "AuxRouting.h"
#include <cmath>
namespace pulse {
using namespace juce;
var auxSendsToJson(const AuxSends& sends){Array<var> out;for(const auto& s:sends){auto* o=new DynamicObject();o->setProperty("target",s.target);o->setProperty("level",s.level);o->setProperty("preFader",s.preFader);out.add(var(o));}return out;}
Result auxSendsFromJson(const var& json,AuxSends& out){
    auto* values=json.getArray();if(!values||values->size()!=auxSendCount)return Result::fail("Auxiliary sends require exactly four slots.");AuxSends next;
    auto number=[](const var& v,double lo,double hi,bool integer=false){return (v.isInt()||v.isInt64()||v.isDouble())&&std::isfinite((double)v)&&(double)v>=lo&&(double)v<=hi&&(!integer||(double)v==std::floor((double)v));};
    for(int i=0;i<auxSendCount;++i){const auto& v=values->getReference(i);auto* o=v.getDynamicObject();if(!o||o->getProperties().size()!=3||!number(v["target"],-1,maxTracks-1,true)||!number(v["level"],0,1.5)||!v["preFader"].isBool())return Result::fail("Each auxiliary send needs target (-1 Off), level 0-1.5 and Boolean preFader only.");for(const auto& field:o->getProperties())if(field.name.toString()!="target"&&field.name.toString()!="level"&&field.name.toString()!="preFader")return Result::fail("Unknown auxiliary send field.");next[(size_t)i]={(int)v["target"],(float)(double)v["level"],(bool)v["preFader"]};}
    out=next;return Result::ok();
}
RoutingAudibility routingAudibility(const Project& p){
    RoutingAudibility out;std::array<int,maxTracks> order{};if(validateRouting(p,&order).failed())return out;int count=(int)p.tracks.size();bool solo=false,hasAux=false;std::array<bool,maxTracks> before{},after{};
    for(int i=0;i<count;++i){before[(size_t)i]=after[(size_t)i]=p.tracks[(size_t)i].solo;solo|=p.tracks[(size_t)i].solo;for(const auto& send:p.tracks[(size_t)i].auxSends)hasAux|=send.target>=0;}
    auto each=[&](int i,auto visit){const auto& t=p.tracks[(size_t)i];if(t.mixer.output>=0)visit(t.mixer.output);for(const auto& send:t.auxSends)if(send.target>=0)visit(send.target);};
    for(int n=count-1;n>=0;--n){int i=order[(size_t)n];each(i,[&](int dest){after[(size_t)i]|=after[(size_t)dest];});}
    for(int n=0;n<count;++n){int i=order[(size_t)n];each(i,[&](int dest){before[(size_t)dest]|=before[(size_t)i];});}
    for(int i=0;i<count;++i){const auto& t=p.tracks[(size_t)i];if(!solo||after[(size_t)i])out.inputs|=1u<<i;auto allowed=[&](int dest){return !solo||before[(size_t)i]||(dest>=0&&after[(size_t)dest]);};if(!hasAux||allowed(-1))out.edges[(size_t)i]|=32;if(allowed(t.mixer.output))out.edges[(size_t)i]|=1;for(int s=0;s<auxSendCount;++s)if(t.auxSends[(size_t)s].target>=0&&allowed(t.auxSends[(size_t)s].target))out.edges[(size_t)i]|=(uint8_t)(2u<<s);}
    return out;
}
bool auxTopologyChanged(const Project& a,const Project& b){if(a.tracks.size()!=b.tracks.size())return true;for(size_t i=0;i<a.tracks.size();++i)for(int s=0;s<auxSendCount;++s){const auto& x=a.tracks[i].auxSends[(size_t)s];const auto& y=b.tracks[i].auxSends[(size_t)s];if(x.target!=y.target||x.preFader!=y.preFader)return true;}return false;}
}
