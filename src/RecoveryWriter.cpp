// SPDX-License-Identifier: AGPL-3.0-only
#include "RecoveryWriter.h"
#include <exception>
namespace pulse {
RecoveryWriter::~RecoveryWriter() { wait(); }
bool RecoveryWriter::start(Project snapshot, const juce::File& target, uint64_t revision, const ProjectSaveControl* control) {
    if (active()) return false;
    completed.store(false);
    completion = {revision, juce::Result::ok()};
    // Copy the verification seam too; never retain a caller's control pointer.
    auto checkpoints = control ? std::optional<ProjectSaveControl>(*control) : std::nullopt;
    try {
        worker = std::thread([this, snapshot=std::move(snapshot), target, checkpoints=std::move(checkpoints)] {
            try { completion.result = saveProjectChecked(snapshot, target, checkpoints ? &*checkpoints : nullptr); }
            catch (const std::exception& error) { completion.result = juce::Result::fail(juce::String("Recovery writer: ") + error.what()); }
            catch (...) { completion.result = juce::Result::fail("Recovery writer failed unexpectedly."); }
            completed.store(true, std::memory_order_release);
        });
    } catch (const std::exception& error) {
        // Leave a pollable failure, even when the OS cannot create a thread.
        completion.result = juce::Result::fail(juce::String("Cannot start recovery writer: ") + error.what());
        completed.store(true, std::memory_order_release);
    }
    return true;
}
std::optional<RecoveryCompletion> RecoveryWriter::poll() {
    if (!completed.load(std::memory_order_acquire)) return std::nullopt;
    return wait();
}
std::optional<RecoveryCompletion> RecoveryWriter::wait() {
    if (worker.joinable()) worker.join();
    if (!completed.exchange(false, std::memory_order_acquire)) return std::nullopt;
    return completion;
}
}
