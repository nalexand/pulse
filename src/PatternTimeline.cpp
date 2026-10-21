// SPDX-License-Identifier: AGPL-3.0-only
#include "PatternTimeline.h"
#include <algorithm>
#include <cmath>
namespace pulse {
std::vector<double> patternPlaybackPositions(const Project& p,int track,double beat){
    std::vector<double> result;
    if(!std::isfinite(beat)||beat<0||p.activePattern<0||p.activePattern>=(int)p.patterns.size()||track<0||track>=(int)p.tracks.size()||p.tracks[(size_t)track].isBus)return result;
    double length=p.patterns[(size_t)p.activePattern].length;if(!std::isfinite(length)||length<=0)return result;
    if(!p.songMode){result.push_back(std::fmod(beat,length));return result;}
    for(const auto& c:p.clips)if(!c.isAudio()&&!c.isAutomation()&&c.pattern==p.activePattern&&c.track==track&&beat>=c.beat&&beat<c.beat+c.length)result.push_back(std::fmod(beat-c.beat,length));
    std::sort(result.begin(),result.end());result.erase(std::unique(result.begin(),result.end(),[](double a,double b){return std::abs(a-b)<1.e-7;}),result.end());return result;
}
}
