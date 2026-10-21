// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <juce_core/juce_core.h>
namespace pulse {
struct SamplePreviewOptions {
    bool sync=false,repeat=false;
    double sourceBpm=120;
    bool operator==(const SamplePreviewOptions&)const=default;
};
juce::Result validateSamplePreview(const SamplePreviewOptions&);
juce::String samplePreviewStamp(const juce::File&);
juce::String samplePreviewToken(const juce::File&);
juce::Result loadSamplePreview(const juce::File& catalog,const juce::File& source,SamplePreviewOptions&);
juce::Result saveSamplePreview(const juce::File& catalog,const juce::File& source,const SamplePreviewOptions&,
                              const juce::String& expectedCatalog,const juce::String& expectedSource);
}
