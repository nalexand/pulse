// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <array>
#include <algorithm>
#include <cmath>
namespace pulse {
class TapTempo {
public:
    void reset(){last=-1;count=0;next=0;}
    double tap(double milliseconds){
        if(!std::isfinite(milliseconds)||milliseconds<0)return 0;
        if(last<0||milliseconds-last>2000){reset();last=milliseconds;return 0;}
        double interval=milliseconds-last;if(interval<200)return 0;last=milliseconds;intervals[(size_t)next]=interval;next=(next+1)%8;count=std::min(8,count+1);
        auto sorted=intervals;std::sort(sorted.begin(),sorted.begin()+count);double median=count%2?sorted[(size_t)count/2]:(sorted[(size_t)count/2-1]+sorted[(size_t)count/2])*.5;return std::round(600000/median)/10;
    }
private:
    double last=-1;
    int count=0,next=0;
    std::array<double,8> intervals{};
};
}
