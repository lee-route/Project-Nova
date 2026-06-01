#pragma once

#include "combat/AttackData.h"
#include "combat/Hitbox.h"
#include "core/Vector2.h"

#include <functional>
#include <vector>

namespace nova {

class CombatSystem;
class ICombatant;

// The kinds of attack a boss can perform. Add new values here to extend the
// system; the controller switches on this enum to build the right hitbox.
enum class BossAttackPattern {
    None,
    MeleeCombo,        // a few quick forward swings
    DashAttack,        // lunge forward, hitbox sweeps with the boss
    RadialAttack,      // burst hitbox all around the boss (AoE)
    ProjectileAttack   // placeholder: hands off to a (future) projectile system
};

// Every executing pattern runs through these phases.
//   Idle    -> not attacking, ready to start a pattern
//   Windup  -> telegraph; no damage yet (gives players time to react)
//   Active  -> damage window; melee-style patterns have a live hitbox here
//   Recovery-> committed end-lag; then the pattern goes on cooldown
enum class BossPhase {
    Idle,
    Windup,
    Active,
    Recovery
};

// Pure DATA describing one boss pattern. Designers/teammates build a library of
// these (no code changes needed to tweak numbers). The pattern-specific knobs
// are simply ignored by the pattern types that don't use them.
struct BossPattern {
    BossAttackPattern type = BossAttackPattern::MeleeCombo;
    AttackData attack{};            // damage + base melee hitbox size

    // timing (seconds)
    float windupSeconds = 0.35f;
    float activeSeconds = 0.20f;
    float recoverySeconds = 0.35f;
    float cooldownSeconds = 2.0f;   // per-pattern reuse delay

    // selection: the distance-to-target band this pattern prefers
    // (close-range patterns use a small maxRange; long-range use a large one).
    float minRange = 0.0f;
    float maxRange = 64.0f;
    float selectionWeight = 1.0f;   // base preference used by the AI selector

    // pattern-specific knobs ------------------------------------------------
    int   comboHits = 3;            // MeleeCombo: number of strikes
    float comboInterval = 0.10f;    // MeleeCombo: time between strikes
    float dashSpeed = 600.0f;       // DashAttack: px/sec while Active
    float radialRadius = 96.0f;     // RadialAttack: AoE half-size
    int   projectileCount = 1;      // ProjectileAttack: how many to spawn
    float projectileSpeed = 300.0f; // ProjectileAttack: px/sec
};

// Info passed to the projectile hook. The boss system does NOT implement
// projectiles; it just describes what to spawn so a teammate's projectile
// module can create the real thing.
struct ProjectileSpawn {
    Vector2 origin;
    Vector2 direction;  // normalised
    float speed = 0.0f;
    float damage = 0.0f;
    int sourceId = -1;
};

// Everything a renderer needs to draw the radial AoE telegraph/blast.
// Pure data: no drawing happens in the controller. A renderer reads this and
// draws (e.g.) a hollow red ring while `warning`, then a filled circle while
// the blast is `active`.
struct AoeCircleDebug {
    bool visible = false; // true during warning + active phases
    bool warning = false; // true during the warning (telegraph) phase only
    bool active = false;  // true on the frame(s) the blast deals damage
    Vector2 center{};
    float radius = 0.0f;
};

// A read-only snapshot of one pattern, handed to the AI selector so it can
// decide WITHOUT being able to touch combat execution. The selector only sees
// type, range band, weight and whether the pattern is off cooldown.
struct BossPatternInfo {
    BossAttackPattern type = BossAttackPattern::None;
    float minRange = 0.0f;
    float maxRange = 0.0f;
    float weight = 1.0f;
    bool ready = false; // off cooldown right now
};

// EXECUTES a boss's attack patterns as a small, data-driven state machine.
//   - holds a library of BossPattern (add/replace freely)
//   - manages per-pattern cooldowns and the Windup/Active/Recovery phases
//   - routes all damage through the CombatSystem
//
// It does NOT decide which pattern to use; that is the job of a separate AI
// module (see BossPatternSelector). The controller only exposes a read-only
// view via patternInfos() and runs whatever tryStartPattern() asks for. This
// keeps AI decision logic cleanly separated from combat execution.
//
// It also knows nothing about rendering or input. A renderer reads phase()/
// currentPattern()/activeHitbox()/aoeDebug(); a movement module reads
// dashVelocity().
class BossPatternController {
public:
    explicit BossPatternController(int ownerId);

    // --- build the data-driven library ---
    void addPattern(const BossPattern& pattern);
    void clearPatterns();
    int patternCount() const { return static_cast<int>(patterns_.size()); }

    // Optional hook: set this to let a projectile module spawn real projectiles
    // when a ProjectileAttack fires. If unset, projectile strikes are no-ops.
    std::function<void(const ProjectileSpawn&)> onSpawnProjectile;

    // Per-frame tick: advances phases/cooldowns and applies Active-window damage.
    // Call once per frame, BEFORE asking to start a new pattern.
    void update(float deltaSeconds,
                CombatSystem& combat,
                const Vector2& bossCenter,
                const Vector2& facing,
                const std::vector<ICombatant*>& targets);

    // --- state queries (safe for AI / HUD) ---
    bool isBusy() const { return phase_ != BossPhase::Idle; }
    bool isReady() const { return phase_ == BossPhase::Idle; }
    BossPhase phase() const { return phase_; }
    BossAttackPattern currentPattern() const;
    bool isPatternReady(BossAttackPattern type) const;
    float cooldownRemaining(BossAttackPattern type) const;

    // Read-only snapshot of the pattern library (type/range/weight/ready) for
    // the AI selector to reason about. No execution happens here.
    std::vector<BossPatternInfo> patternInfos() const;

    // --- execution trigger (called by AI after it has decided) ---
    // Start a specific pattern. No-op (returns false) if busy or on cooldown.
    bool tryStartPattern(BossAttackPattern type,
                         const Vector2& bossCenter,
                         const Vector2& facing,
                         const Vector2& targetCenter);

    // --- rendering / debug (read-only) ---
    bool hasActiveHitbox() const { return hitboxLive_; }
    Hitbox activeHitbox() const { return currentHitbox_; }

    // Radial AoE telegraph/blast info for a debug or game renderer to draw.
    // The circle is visible during the warning phase (so players can dodge)
    // and the active phase. Drawing itself lives entirely in the renderer.
    AoeCircleDebug aoeDebug() const;

    // --- movement intent (applied by a movement module) ---
    bool isDashing() const;
    Vector2 dashVelocity() const; // {0,0} unless dashing this frame

private:
    int findPattern(BossAttackPattern type) const;
    void enterActive(const BossPattern& pattern, const Vector2& bossCenter);
    void runActive(float deltaSeconds,
                   const BossPattern& pattern,
                   CombatSystem& combat,
                   const Vector2& bossCenter,
                   const std::vector<ICombatant*>& targets);
    Hitbox buildHitbox(const BossPattern& pattern,
                       const Vector2& bossCenter) const;
    void applyMeleeDamage(const BossPattern& pattern,
                          CombatSystem& combat,
                          const std::vector<ICombatant*>& targets);
    void applyRadialDamage(const BossPattern& pattern,
                           CombatSystem& combat,
                           const std::vector<ICombatant*>& targets);

    int ownerId_;
    std::vector<BossPattern> patterns_;
    std::vector<float> cooldownRemaining_; // parallel to patterns_

    BossPhase phase_ = BossPhase::Idle;
    int currentIndex_ = -1;
    float phaseTimer_ = 0.0f;

    // Active-window strike bookkeeping.
    int strikesDone_ = 0;
    float strikeTimer_ = 0.0f;
    bool hitboxLive_ = false;
    Hitbox currentHitbox_{};
    std::vector<int> alreadyHitIds_;

    Vector2 attackFacing_{1.0f, 0.0f}; // captured when the pattern starts
    Vector2 aoeCenter_{};              // radial AoE centre, locked at warning start
};

} // namespace nova
