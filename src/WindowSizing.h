// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
namespace pulse {
constexpr int minimumCanvasWidth=1180,minimumCanvasHeight=800;
struct CanvasSize {int width=minimumCanvasWidth,height=minimumCanvasHeight;float scale=1;};
inline CanvasSize fitCanvas(int width,int height,float requestedScale=1){
    width=juce::jmax(1,width);height=juce::jmax(1,height);double preferred=std::isfinite(requestedScale)?juce::jlimit(.75,2.,(double)requestedScale):1.;double scale=juce::jmin(preferred,width/(double)minimumCanvasWidth,height/(double)minimumCanvasHeight);
    return {juce::jmax(minimumCanvasWidth,(int)std::ceil(width/scale)),juce::jmax(minimumCanvasHeight,(int)std::ceil(height/scale)),(float)scale};
}
inline juce::Rectangle<int> availableClientArea(juce::Rectangle<int> workArea,juce::BorderSize<int> frame){
    int margin=juce::jmin(8,juce::jmax(0,juce::jmin(workArea.getWidth(),workArea.getHeight())/40));auto area=frame.subtractedFrom(workArea.reduced(margin));return area.withSize(juce::jmax(1,area.getWidth()),juce::jmax(1,area.getHeight()));
}
inline juce::Rectangle<int> fitClientToWorkArea(juce::Rectangle<int> desired,juce::Rectangle<int> workArea,juce::BorderSize<int> frame){
    auto area=availableClientArea(workArea,frame);int width=juce::jlimit(1,area.getWidth(),desired.getWidth()),height=juce::jlimit(1,area.getHeight(),desired.getHeight());return {juce::jlimit(area.getX(),area.getRight()-width,desired.getX()),juce::jlimit(area.getY(),area.getBottom()-height,desired.getY()),width,height};
}
}
