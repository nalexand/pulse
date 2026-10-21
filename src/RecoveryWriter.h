// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "ProjectPersistence.h"
#include <atomic>
#include <optional>
#include <thread>

namespace pulse {
struct RecoveryCompletion {
    uint64_t revision = 0;
    juce::Result result = juce::Result::ok();
};
// Single owner (the message thread), single outstanding immutable snapshot.
// poll() never waits for I/O. wait() is the barrier used before any other
// recovery read/write/retirement or session replacement and at destruction.
class RecoveryWriter final {
public:
    ~RecoveryWriter();
    bool start(Project, const juce::File&, uint64_t, const ProjectSaveControl* = nullptr);
    bool active() const { return worker.joinable() || completed.load(std::memory_order_acquire); }
    std::optional<RecoveryCompletion> poll();
    std::optional<RecoveryCompletion> wait();
private:
    std::thread worker;
    std::atomic<bool> completed{false};
    RecoveryCompletion completion;
};
}
