// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "ProjectPersistence.h"
#include <atomic>
namespace pulse {
struct RecoveryRetentionPolicy {
    int keepNewest=20,maxAgeDays=30,maxMiB=256;
    bool automatic=false;
    bool operator==(const RecoveryRetentionPolicy&)const=default;
};
struct RecoveryArchive {
    juce::File file;
    juce::String token;
    juce::int64 bytes=0,modified=0;
    bool operator==(const RecoveryArchive&)const=default;
};
struct RecoveryCleanupPlan {
    juce::File workspace;
    RecoveryRetentionPolicy policy;
    juce::int64 evaluatedAt=0,totalBytes=0,removeBytes=0;
    int excluded=0;
    std::vector<RecoveryArchive> archives;
    std::vector<int> remove;
};
struct RecoveryCleanupResult {int removed=0;juce::int64 bytes=0;};
juce::Result validateRecoveryRetention(const RecoveryRetentionPolicy&);
juce::Result loadRecoveryRetention(const juce::File&,RecoveryRetentionPolicy&);
juce::Result saveRecoveryRetention(const juce::File&,const RecoveryRetentionPolicy&);
juce::Result planRecoveryCleanup(const juce::File& workspace,const RecoveryRetentionPolicy&,RecoveryCleanupPlan&,const std::atomic<bool>* cancel=nullptr,juce::int64 now=0);
juce::Result applyRecoveryCleanup(const RecoveryCleanupPlan&,RecoveryCleanupResult&,const std::atomic<bool>* cancel=nullptr);
juce::String recoveryCleanupSummary(const RecoveryCleanupPlan&);
}
