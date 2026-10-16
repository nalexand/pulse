// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Studio.h"
#include "WindowSizing.h"
namespace pulse {
// Keep the familiar canvas usable while fitting the physical native window.
class StudioViewport final : public juce::Component {
public:
    StudioViewport(const juce::File& root,bool audio,bool resume):workstation(root,audio,resume){setOpaque(true);addAndMakeVisible(workstation);workstation.interfaceScaleChanged=[this]{resized();};}
    ~StudioViewport()override{workstation.interfaceScaleChanged={};}
    Studio& editor(){return workstation;}
    float canvasScale()const{return scale;}
    void paint(juce::Graphics& g)override{g.fillAll(juce::Colour(0xff343b3f));}
    void resized()override{auto size=fitCanvas(getWidth(),getHeight(),workstation.requestedInterfaceScale()/100.f);scale=size.scale;workstation.setEffectiveInterfaceScale(scale);workstation.setTransform(juce::AffineTransform::scale(scale));workstation.setBounds(0,0,size.width,size.height);}
private:
    Studio workstation;
    float scale=1;
};
}
