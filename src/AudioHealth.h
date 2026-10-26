// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <juce_core/juce_core.h>
#include <atomic>
#include <array>
namespace pulse {
// One host-serialized producer (prepare/release/callback), bounded atomic reads.
// Reset requests come from the UI; only the producer changes measurement data.
// There are no locks, allocations, filesystem calls or UI calls in begin/end.
class AudioHealth final {
public:
    explicit AudioHealth(double ticksPerSecond=juce::Time::getHighResolutionTicksPerSecond());
    struct Token {int64_t started=0;int frames=0;bool valid=false;};
    struct Snapshot {
        bool consistent=false,running=false,resetPending=false;
        uint64_t measurement=0,deviceStarts=0,deviceStops=0,callbacks=0,frames=0,deadlineMisses=0,lateStarts=0,estimatedMissingBlocks=0,invalidBlocks=0,clockErrors=0,silentBlocks=0;
        double rate=0,lastMs=0,maxMs=0,meanMs=0,meanUtilization=0,worstUtilization=0,maxStartGapMs=0;int nominalBlock=0;
    };
    void prepare(double rate,int nominalBlock);
    void stopped();
    Token begin(int frames,int64_t started);
    void end(Token,int64_t ended,bool outputAvailable=true);
    void requestReset(){requested.fetch_add(1,std::memory_order_release);}
    Snapshot snapshot()const;
private:
    enum Field {callbacks,frames,deadlineMisses,lateStarts,missing,invalid,clockErrors,lastTicks,maxTicks,totalTicks,maxGap,worstUtilization,silentBlocks,fieldCount};
    void clearMeasurement();void publish();
    const double frequency;
    double rate=0;int nominalBlock=0,previousFrames=0;int64_t previousStart=0;bool havePrevious=false;
    std::array<uint64_t,fieldCount> values{};
    uint64_t measurement=0,applied=0;
    std::atomic<uint64_t> requested{0},publishedReset{0},sequence{0},publishedMeasurement{0},starts{0},stops{0};
    std::array<std::atomic<uint64_t>,fieldCount> published{};
    std::atomic<double> publishedRate{0};std::atomic<int> publishedBlock{0};std::atomic<bool> active{false};
};
}
