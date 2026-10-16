// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "SynthPresets.h"
namespace pulse {
constexpr int maxPersonalSounds=128,maxSoundLibraryBytes=1048576;
struct PersonalSound {juce::String id;SynthPreset sound;};
struct SoundLibrary {std::vector<PersonalSound> personal;juce::StringArray favorites;};
juce::String factorySoundId(const SynthPreset&);
const PersonalSound* findPersonalSound(const SoundLibrary&,const juce::String&);
bool resolveLibrarySound(const SoundLibrary&,const juce::String&,SynthPreset&);
juce::Result validateSoundLibrary(const SoundLibrary&);
juce::var soundLibraryToJson(const SoundLibrary&);
juce::Result soundLibraryFromJson(const juce::var&,SoundLibrary&);
juce::String soundLibraryToken(const juce::File&);
juce::Result loadSoundLibrary(const juce::File&,SoundLibrary&);
juce::Result saveSoundLibrary(const juce::File&,const SoundLibrary&,const juce::String& expectedToken={});
juce::Result putPersonalSound(const juce::File&,const PersonalSound&,bool replace=false,const juce::String& expectedToken={});
juce::Result removePersonalSound(const juce::File&,const juce::String& id,const juce::String& expectedToken={});
juce::Result setSoundFavorite(const juce::File&,const juce::String& id,bool favorite,const juce::String& expectedToken={});
}
