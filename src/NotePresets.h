// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "NoteTools.h"
#include "Rhythm.h"
#include "Arpeggiator.h"
namespace pulse {
struct NotePreset {
    juce::String id,name;
    NoteToolMode mode=NoteToolMode::quantize;
    NoteToolOptions notes;
    RhythmOptions rhythm;
    ArpOptions arp;
};
constexpr int maxNotePresets=128;
juce::String notePresetModeName(NoteToolMode);
juce::var notePresetToJson(const NotePreset&);
juce::Result notePresetFromJson(const juce::var&,NotePreset&);
juce::Result validateNotePreset(const NotePreset&);
juce::Result loadNotePresets(const juce::File&,std::vector<NotePreset>&);
juce::Result saveNotePresets(const juce::File&,const std::vector<NotePreset>&);
juce::Result putNotePreset(const juce::File&,const NotePreset&,bool replace=false);
juce::Result removeNotePreset(const juce::File&,const juce::String& id);
std::vector<NotePreset> factoryNotePresets(NoteToolMode);
}
