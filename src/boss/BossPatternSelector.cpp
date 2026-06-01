#include "boss/BossPatternSelector.h"

#include <limits>

namespace nova {

int BossPatternSelector::countInHistory(BossAttackPattern type) const {
    int count = 0;
    for (BossAttackPattern p : history_) {
        if (p == type) {
            ++count;
        }
    }
    return count;
}

BossAttackPattern BossPatternSelector::select(
    const std::vector<BossPatternInfo>& options,
    float distanceToTarget) {
    BossAttackPattern best = BossAttackPattern::None;
    float bestScore = -std::numeric_limits<float>::max();

    std::uniform_real_distribution<float> jitterDist(0.0f, jitter_);

    for (const BossPatternInfo& opt : options) {
        // 1) cooldown filter: only consider patterns that are ready.
        if (!opt.ready) {
            continue;
        }
        // 2) distance filter: the pattern's range band must contain the
        //    current distance (this is what makes close/long range work).
        if (distanceToTarget < opt.minRange || distanceToTarget > opt.maxRange) {
            continue;
        }

        // 3) anti-repeat: subtract a penalty for each recent use, so a pattern
        //    that was just used scores lower and the boss varies its attacks.
        float score = opt.weight -
                      repeatPenalty_ * static_cast<float>(countInHistory(opt.type));

        // 4) small random jitter breaks ties differently each time.
        if (jitter_ > 0.0f) {
            score += jitterDist(rng_);
        }

        if (score > bestScore) {
            bestScore = score;
            best = opt.type;
        }
    }

    // Note: if exactly one pattern is eligible it is always returned, even if
    // it was just used. Anti-repeat is a preference, never a deadlock.
    return best;
}

void BossPatternSelector::recordChoice(BossAttackPattern chosen) {
    if (chosen == BossAttackPattern::None || historySize_ == 0) {
        return;
    }
    history_.push_back(chosen);
    while (static_cast<int>(history_.size()) > historySize_) {
        history_.pop_front();
    }
}

} // namespace nova
