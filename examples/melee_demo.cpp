// Simple melee attack example.
//
// Shows the full flow with the real Player damage-receiving logic:
//   AI -> Monster::tryAttack -> MonsterAttackController -> CombatSystem
//        -> Player::applyDamage (i-frames / hit reaction / death) -> HealthComponent
//
// It demonstrates:
//   - the monster's active window + cooldown
//   - the player's invulnerability frames swallowing some swings
//   - the player's hit reaction flag
//   - the player ignoring damage once dead

#include "combat/CombatSystem.h"
#include "combat/ICombatant.h"
#include "core/Vector2.h"
#include "monster/Monster.h"
#include "player/Player.h"

#include <iostream>
#include <vector>

using namespace nova;

static const char* phaseName(AttackPhase p) {
    switch (p) {
        case AttackPhase::Ready:    return "READY   ";
        case AttackPhase::Active:   return "ACTIVE  ";
        case AttackPhase::Cooldown: return "COOLDOWN";
    }
    return "?";
}

int main() {
    CombatSystem combat;

    AttackData goblinAttack;
    goblinAttack.damage = 12.0f;
    goblinAttack.range = 40.0f;
    goblinAttack.activeSeconds = 0.2f;
    goblinAttack.cooldownSeconds = 0.3f; // attacks quickly so i-frames matter
    goblinAttack.hitboxWidth = 30.0f;
    goblinAttack.hitboxHeight = 30.0f;

    Monster goblin(/*id*/ 1, Vector2{100.0f, 100.0f}, goblinAttack);

    Player player(/*id*/ 99, Vector2{120.0f, 100.0f}, /*maxHealth*/ 50.0f);
    player.setInvulnerabilityDuration(0.8f); // i-frames after each hit
    player.setHitReactionDuration(0.3f);

    std::vector<ICombatant*> targets{ &player };

    const float dt = 0.1f; // 10 frames per second
    for (int frame = 0; frame < 40; ++frame) {
        const float prevHp = player.health().current();

        // 1) Tick combat: monster state machine may apply damage this frame.
        goblin.update(dt, combat, targets);
        // 2) Tick the player: counts down i-frames and hit-reaction timers.
        player.update(dt);
        // 3) AI request (safe to call every frame).
        goblin.tryAttack(player.position());

        const float dealt = prevHp - player.health().current();

        std::cout << "t=" << frame * dt << "s  "
                  << phaseName(goblin.attackController().phase())
                  << "  HP=" << player.health().current()
                  << (dealt > 0.0f ? "  [HIT]" : "       ")
                  << (player.isInvulnerable() ? "  iFrames" : "         ")
                  << (player.isInHitReaction() ? "  reaction" : "          ")
                  << (player.isDead() ? "  DEAD" : "")
                  << "\n";

        if (player.isDead()) {
            std::cout << "Player died (killed by id " << player.lastHitBy()
                      << "). Further hits are ignored.\n";
            break;
        }
    }

    return 0;
}
