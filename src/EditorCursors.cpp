// SPDX-License-Identifier: AGPL-3.0-only
#include "EditorCursors.h"
namespace pulse {
juce::MouseCursor editorToolCursor(int tool){
    using namespace juce;
    static const auto cursors=[] {
        std::array<MouseCursor,4> result;
        for(int mode=0;mode<4;++mode){Image image(Image::ARGB,28,28,true);Graphics g(image);
            // A small outlined arrow fixes the hotspot; the adjacent glyph identifies the tool.
            Path arrow;arrow.startNewSubPath(1,1);arrow.lineTo(1,15);arrow.lineTo(5,11);arrow.lineTo(9,19);arrow.lineTo(12,17);arrow.lineTo(8,10);arrow.lineTo(14,10);arrow.closeSubPath();g.setColour(Colours::black);g.strokePath(arrow,PathStrokeType(2.5f));g.setColour(Colours::white);g.fillPath(arrow);
            if(mode==3){g.setColour(Colour(0xffffc978));for(int x=13;x<26;x+=4){g.fillRect(x,14,2,2);g.fillRect(x,25,2,2);}for(int y=18;y<25;y+=4){g.fillRect(13,y,2,2);g.fillRect(25,y,2,2);}}
            else if(mode==2){g.setColour(Colours::black);g.drawLine(17,16,25,24,5);g.drawLine(25,16,17,24,5);g.setColour(Colour(0xffff7970));g.drawLine(17,16,25,24,2.5f);g.drawLine(25,16,17,24,2.5f);}
            else if(mode==1){g.setColour(Colours::black);g.fillRect(15,14,12,8);g.fillRect(19,20,4,7);g.setColour(Colour(0xffb7ce91));g.fillRect(16,15,10,5);g.fillRect(20,20,2,6);}
            else{g.setColour(Colours::black);g.drawLine(17,25,25,15,6);g.setColour(Colour(0xffffcd85));g.drawLine(17,25,25,15,3);g.setColour(Colours::white);g.fillEllipse(15,24,2,2);}
            result[(size_t)mode]=MouseCursor(image,1,1);
        }return result;
    }();return cursors[(size_t)juce::jlimit(0,3,tool)];
}
}
