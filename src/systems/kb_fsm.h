// kb_fsm.h - declares knowledge-base FSM states, transitions, and runtime.
// Owns: behavior state metadata, trigger rules, and emitted action records.

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <random>

namespace mmx {

struct KBState {
    std::string id;
    std::string name;
    float meanDuration = 0;
    float durationStdev = 0;
};

struct KBTransition {
    std::string fromState;
    std::string toState;
    std::string triggerType;
    float timerFrames = 0;
    float probability = 1.0f;
};

class KBFSM {
public:
    bool loadFromJson(const std::string& jsonPath);

    void setSeed(uint32_t seed);
    void reset();
    void tick();
    const std::string& currentStateId() const { return currentState_; }
    const std::string& currentStateName() const;
    bool transitioned() const { return transitioned_; }

private:
    std::vector<KBState> states_;
    std::vector<KBTransition> transitions_;
    std::string initialState_;
    std::string currentState_;
    int stateTimer_ = 0;
    float currentDuration_ = 0;
    bool transitioned_ = false;
    std::mt19937 rng_{0x58464d4dU};

    const KBState* findState(const std::string& id) const;
    float rollDuration(const KBState& state);
    void evaluateTransitions();
    void enterState(const std::string& stateId);
};

} // namespace mmx
