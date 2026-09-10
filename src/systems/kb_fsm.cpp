// kb_fsm.cpp - loads and advances knowledge-base enemy behavior FSMs.
// Owns: FSM JSON validation, state entry, transition rolls, and action emits.

#include "systems/kb_fsm.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <unordered_set>
#include <unordered_map>

using json = nlohmann::json;

namespace mmx {
namespace {

static const std::string EMPTY_STRING;

bool isKnownTriggerType(const std::string& type) {
    return type == "single_sample" ||
           type == "timer_expired" ||
           type == "timer_variable" ||
           type == "probabilistic_branch";
}

bool finiteNonNegative(float value) {
    return std::isfinite(value) && value >= 0.0f;
}

bool validProbability(float value) {
    return finiteNonNegative(value) && value <= 1.0f;
}

int frameThreshold(float frames) {
    if (!std::isfinite(frames) || frames <= 0.0f) return 1;
    return std::max(1, static_cast<int>(std::ceil(frames)));
}

bool failLoad(const std::string& jsonPath, const char* reason) {
    fprintf(stderr, "KBFSM: invalid schema in %s: %s\n", jsonPath.c_str(), reason);
    return false;
}

}

bool KBFSM::loadFromJson(const std::string& jsonPath) {
    std::ifstream file(jsonPath);
    if (!file.is_open()) return false;

    json j;
    try {
        file >> j;
    } catch (const json::parse_error& e) {
        fprintf(stderr, "KBFSM: parse error in %s: %s\n", jsonPath.c_str(), e.what());
        return false;
    }

    std::vector<KBState> parsedStates;
    std::vector<KBTransition> parsedTransitions;
    std::string parsedInitialState;

    try {
        if (!j.contains("states") || !j["states"].is_array()) {
            return failLoad(jsonPath, "states array is required");
        }
        if (!j.contains("transitions") || !j["transitions"].is_array()) {
            return failLoad(jsonPath, "transitions array is required");
        }

        std::unordered_set<std::string> stateIds;
        for (const auto& s : j["states"]) {
            KBState state;
            state.id = s.value("state_id", "");
            if (state.id.empty()) {
                return failLoad(jsonPath, "state_id must be non-empty");
            }
            if (!stateIds.insert(state.id).second) {
                return failLoad(jsonPath, "duplicate state_id");
            }
            if (s.contains("name") && s["name"].is_string()) {
                state.name = s["name"].get<std::string>();
            }
            if (state.name.empty()) {
                state.name = "state_" + state.id;
            }
            if (s.contains("duration_frames") && s["duration_frames"].is_object()) {
                state.meanDuration = s["duration_frames"].value("mean", 0.0f);
                state.durationStdev = s["duration_frames"].value("stdev", 0.0f);
            }
            if (!finiteNonNegative(state.meanDuration) ||
                !finiteNonNegative(state.durationStdev)) {
                return failLoad(jsonPath, "state durations must be finite and non-negative");
            }
            parsedStates.push_back(std::move(state));
        }

        if (parsedStates.empty()) {
            return failLoad(jsonPath, "at least one state is required");
        }
        parsedInitialState = parsedStates[0].id;

        std::unordered_map<std::string, float> probabilisticTotals;
        for (const auto& t : j["transitions"]) {
            KBTransition tr;
            tr.fromState = t.value("from_state", "");
            tr.toState = t.value("to_state", "");
            if (!stateIds.count(tr.fromState) || !stateIds.count(tr.toState)) {
                return failLoad(jsonPath, "transition endpoints must reference known states");
            }

            if (!t.contains("trigger") || !t["trigger"].is_object()) {
                return failLoad(jsonPath, "transition trigger object is required");
            }
            const auto& trigger = t["trigger"];
            tr.triggerType = trigger.value("type", "");
            if (!isKnownTriggerType(tr.triggerType)) {
                return failLoad(jsonPath, "unknown transition trigger type");
            }
            if (trigger.contains("frames")) {
                tr.timerFrames = trigger["frames"].get<float>();
            } else if (trigger.contains("duration")) {
                tr.timerFrames = trigger["duration"].get<float>();
            }
            tr.probability = trigger.value("probability", 1.0f);
            if (!finiteNonNegative(tr.timerFrames) || !validProbability(tr.probability)) {
                return failLoad(jsonPath, "trigger values must be finite and non-negative");
            }
            if (tr.triggerType == "probabilistic_branch") {
                probabilisticTotals[tr.fromState] += tr.probability;
            }
            parsedTransitions.push_back(std::move(tr));
        }
        for (const auto& [_, total] : probabilisticTotals) {
            if (total <= 0.0f) {
                return failLoad(jsonPath, "probabilistic branch group has zero total weight");
            }
        }
    } catch (const json::exception& e) {
        fprintf(stderr, "KBFSM: invalid schema in %s: %s\n", jsonPath.c_str(), e.what());
        return false;
    }

    states_ = std::move(parsedStates);
    transitions_ = std::move(parsedTransitions);
    initialState_ = std::move(parsedInitialState);
    reset();
    return true;
}

void KBFSM::setSeed(uint32_t seed) {
    rng_.seed(seed);
    if (!currentState_.empty()) {
        const KBState* s = findState(currentState_);
        currentDuration_ = s ? rollDuration(*s) : 1.0f;
    }
}

void KBFSM::reset() {
    currentState_ = initialState_;
    stateTimer_ = 0;
    transitioned_ = false;
    const KBState* s = findState(currentState_);
    currentDuration_ = s ? rollDuration(*s) : 1;
}

void KBFSM::tick() {
    transitioned_ = false;
    stateTimer_++;
    evaluateTransitions();
}

const std::string& KBFSM::currentStateName() const {
    const KBState* s = findState(currentState_);
    return s ? s->name : EMPTY_STRING;
}

const KBState* KBFSM::findState(const std::string& id) const {
    for (const auto& s : states_) {
        if (s.id == id) return &s;
    }
    return nullptr;
}

float KBFSM::rollDuration(const KBState& state) {
    if (state.durationStdev <= 0) return std::max(1.0f, state.meanDuration);
    std::normal_distribution<float> distribution(state.meanDuration, state.durationStdev);
    float result = distribution(rng_);
    float lo = state.meanDuration - 2 * state.durationStdev;
    float hi = state.meanDuration + 2 * state.durationStdev;
    return std::clamp(result, std::max(1.0f, lo), hi);
}

void KBFSM::enterState(const std::string& stateId) {
    currentState_ = stateId;
    stateTimer_ = 0;
    transitioned_ = true;
    const KBState* s = findState(currentState_);
    currentDuration_ = s ? rollDuration(*s) : 1.0f;
}

void KBFSM::evaluateTransitions() {
    // Collect all transitions from current state
    std::vector<const KBTransition*> candidates;
    for (const auto& t : transitions_) {
        if (t.fromState == currentState_) {
            candidates.push_back(&t);
        }
    }
    if (candidates.empty()) return;

    // Single-sample or timer-expired transitions fire when timer exceeds duration
    for (const auto* t : candidates) {
        if (t->triggerType == "single_sample" || t->triggerType == "timer_expired") {
            float threshold = (t->timerFrames > 0) ? t->timerFrames : currentDuration_;
            if (stateTimer_ >= frameThreshold(threshold)) {
                enterState(t->toState);
                return;
            }
        }
    }

    // Timer-variable: fire when state duration expires
    for (const auto* t : candidates) {
        if (t->triggerType == "timer_variable") {
            if (stateTimer_ >= frameThreshold(currentDuration_)) {
                enterState(t->toState);
                return;
            }
        }
    }

    // Probabilistic branches: fire when idle duration expires, pick by probability
    std::vector<const KBTransition*> probBranches;
    for (const auto* t : candidates) {
        if (t->triggerType == "probabilistic_branch") {
            probBranches.push_back(t);
        }
    }
    if (!probBranches.empty() && stateTimer_ >= frameThreshold(currentDuration_)) {
        float total = 0.0f;
        for (const auto* t : probBranches) {
            total += t->probability;
        }
        if (total <= 0.0f) return;

        std::uniform_real_distribution<float> distribution(0.0f, total);
        float roll = distribution(rng_);
        float cumulative = 0;
        for (const auto* t : probBranches) {
            cumulative += t->probability;
            if (roll < cumulative) {
                enterState(t->toState);
                return;
            }
        }
        // Fallback to last branch if floating-point rounding didn't trigger
        enterState(probBranches.back()->toState);
    }
}

} // namespace mmx
