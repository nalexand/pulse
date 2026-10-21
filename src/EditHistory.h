// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Project.h"
#include <deque>
namespace pulse {
// Timestamp belongs to the transition AFTER an Undo snapshot and BEFORE a Redo
// snapshot. Moving a snapshot between stacks carries that transition's stamp.
struct HistorySnapshot : Project {
    juce::int64 editTime=0;
    HistorySnapshot(Project value,juce::int64 time=juce::Time::currentTimeMillis())
        :Project(std::move(value)),editTime(time){}
};
struct HistoryDescription {juce::String summary,detail;};
struct HistoryRow {
    int state=0; // Chronological state index, independent of search/filter order.
    juce::int64 time=0;
    juce::String summary,detail;
};
HistoryDescription describeHistoryEdit(const Project& before,const Project& after);
std::vector<HistoryRow> projectHistoryRows(const Project&,const std::deque<HistorySnapshot>& undo,const std::deque<HistorySnapshot>& redo);
// Validates the complete destination before touching either stack. No file IO,
// revision changes or intermediate playback publications happen here.
juce::Result moveProjectHistory(Project&,std::deque<HistorySnapshot>& undo,std::deque<HistorySnapshot>& redo,int state);
}
