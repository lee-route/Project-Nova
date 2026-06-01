#pragma once

#include "boss/BossPatternController.h" // BossAttackPattern + BossPatternInfo

#include <deque>
#include <random>
#include <vector>

namespace nova {

// Pure AI DECISION logic for boss attacks. It is deliberately a SEPARATE class
// from BossPatternController (which only EXECUTES patterns). The selector never
// touches the CombatSystem, hitboxes, or HP; it just answers the question
// "which pattern should the boss use next?".
//
// What it does, in order:
//   1. cooldown filter   -> skip patterns that are not ready
//   2. distance filter   -> keep only patterns whose range band fits the
//                           distance to the player (close vs long range)
//   3. anti-repeat       -> penalise patterns used in the recent history so the
//                           boss does not spam the same move
//   4. weighting + jitter-> pick the best score, with designer weights and a
//                           little randomness for variety
//
// Typical use (note: decision and execution stay separate):
//   if (boss.isReady()) {
//       auto choice = selector.select(boss.patternInfos(), distanceToPlayer);
//       if (choice != BossAttackPattern::None &&
//           boss.tryStartPattern(choice, pos, facing, target)) {
//           selector.recordChoice(choice);
//       }
//   }
class BossPatternSelector {
public:
    BossPatternSelector() : rng_(std::random_device{}()) {}
    explicit BossPatternSelector(unsigned seed) : rng_(seed) {}

    // --- tuning (all optional) ---
    void setHistorySize(int n) { historySize_ = n < 0 ? 0 : n; }
    void setRepeatPenalty(float penaltyPerRecentUse) { repeatPenalty_ = penaltyPerRecentUse; }
    void setRandomJitter(float jitter) { jitter_ = jitter < 0.0f ? 0.0f : jitter; }

    // Decide which pattern to use given the current options + player distance.
    // Returns BossAttackPattern::None if nothing is eligible. This is a pure
    // read of `options` plus the internal history; it starts nothing.
    BossAttackPattern select(const std::vector<BossPatternInfo>& options,
                             float distanceToTarget);

    // Call this AFTER a chosen pattern actually starts, so the anti-repeat
    // history has memory. Kept separate from select() so selection has no
    // hidden side effects.
    void recordChoice(BossAttackPattern chosen);

    void reset() { history_.clear(); }
    const std::deque<BossAttackPattern>& history() const { return history_; }

private:
    int countInHistory(BossAttackPattern type) const;

    int historySize_ = 4;        // how many recent picks to remember
    float repeatPenalty_ = 2.0f; // score lost per recent use of a pattern
    float jitter_ = 0.25f;       // random spice so equal options vary
    std::deque<BossAttackPattern> history_;
    std::mt19937 rng_;
};

} // namespace nova
