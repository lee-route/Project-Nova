// Simple boss pattern loop example.
//
// Shows the data-driven boss system with SEPARATED decision/execution:
//   AI decision  -> BossPatternSelector::select (distance + cooldown + anti-repeat)
//   AI execution -> BossPatternController::tryStartPattern
//        -> Windup -> Active (CombatSystem damage / projectile hook) -> Recovery
//        -> per-pattern cooldown.
//
// The boss owns no rendering; we just print phase, pattern, distance and the
// player's HP. The DashAttack also moves the boss (movement applied here, as a
// movement module would).

#include "boss/BossPatternController.h"
#include "boss/BossPatternSelector.h"
#include "combat/CombatSystem.h"
#include "combat/ICombatant.h"
#include "core/Vector2.h"
#include "player/Player.h"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

using namespace nova;

static const char* patternName(BossAttackPattern p) {
    switch (p) {
        case BossAttackPattern::None:             return "None";
        case BossAttackPattern::MeleeCombo:       return "MeleeCombo";
        case BossAttackPattern::DashAttack:       return "DashAttack";
        case BossAttackPattern::RadialAttack:     return "RadialAttack";
        case BossAttackPattern::ProjectileAttack: return "Projectile";
    }
    return "?";
}

static const char* phaseName(BossPhase p) {
    switch (p) {
        case BossPhase::Idle:     return "Idle    ";
        case BossPhase::Windup:   return "Windup  ";
        case BossPhase::Active:   return "Active  ";
        case BossPhase::Recovery: return "Recovery";
    }
    return "?";
}

static float distance(const Vector2& a, const Vector2& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

int main() {
    CombatSystem combat;

    BossPatternController boss(/*ownerId*/ 7);

    // --- data-driven pattern library (priority = insertion order) ---
    BossPattern melee;
    melee.type = BossAttackPattern::MeleeCombo;
    melee.attack.damage = 8.0f;
    melee.attack.range = 60.0f;
    melee.attack.hitboxWidth = 50.0f;
    melee.attack.hitboxHeight = 50.0f;
    melee.minRange = 0.0f;
    melee.maxRange = 70.0f;
    melee.comboHits = 3;
    melee.comboInterval = 0.12f;
    melee.cooldownSeconds = 1.5f;
    boss.addPattern(melee);

    BossPattern radial;
    radial.type = BossAttackPattern::RadialAttack;
    radial.attack.damage = 15.0f;
    radial.radialRadius = 90.0f;
    radial.minRange = 0.0f;
    radial.maxRange = 90.0f;
    radial.windupSeconds = 0.5f;     // big telegraph
    radial.cooldownSeconds = 3.0f;
    boss.addPattern(radial);

    BossPattern dash;
    dash.type = BossAttackPattern::DashAttack;
    dash.attack.damage = 12.0f;
    dash.attack.range = 50.0f;
    dash.attack.hitboxWidth = 45.0f;
    dash.attack.hitboxHeight = 45.0f;
    dash.dashSpeed = 350.0f;
    dash.minRange = 120.0f;
    dash.maxRange = 320.0f;
    dash.cooldownSeconds = 2.0f;
    boss.addPattern(dash);

    BossPattern projectile;
    projectile.type = BossAttackPattern::ProjectileAttack;
    projectile.attack.damage = 6.0f;
    projectile.projectileCount = 3;
    projectile.projectileSpeed = 300.0f;
    projectile.comboInterval = 0.1f;
    projectile.minRange = 200.0f;
    projectile.maxRange = 1000.0f;
    projectile.cooldownSeconds = 2.5f;
    boss.addPattern(projectile);

    // Placeholder hook: a real projectile module would create an entity here.
    boss.onSpawnProjectile = [](const ProjectileSpawn& s) {
        std::cout << "    [projectile spawned] dir=(" << s.direction.x << ","
                  << s.direction.y << ") dmg=" << s.damage << "\n";
    };

    // The AI decision module, kept entirely separate from the controller.
    // Fixed seed so this example prints the same sequence every run.
    BossPatternSelector selector(/*seed*/ 1234);
    selector.setHistorySize(3);     // remember the last 3 picks
    selector.setRepeatPenalty(2.5f); // strongly discourage repeats

    Vector2 bossPos{100.0f, 100.0f};
    const Vector2 bossFacing{1.0f, 0.0f}; // facing +x toward the player

    Player player(/*id*/ 99, Vector2{400.0f, 100.0f}, /*maxHealth*/ 120.0f);
    player.setInvulnerabilityDuration(0.4f);
    std::vector<ICombatant*> targets{ &player };

    const float dt = 0.1f;
    for (int frame = 0; frame < 80 && !player.isDead(); ++frame) {
        const float prevHp = player.health().current();

        // 1) tick boss + player
        boss.update(dt, combat, bossPos, bossFacing, targets);
        player.update(dt);

        // 2) movement module: apply the boss's dash intent
        const Vector2 dv = boss.dashVelocity();
        bossPos.x += dv.x * dt;
        bossPos.y += dv.y * dt;

        // 3) AI: when free, DECIDE (selector) then EXECUTE (controller).
        if (boss.isReady()) {
            const float dist = distance(bossPos, player.position());
            const BossAttackPattern choice =
                selector.select(boss.patternInfos(), dist);
            if (choice != BossAttackPattern::None &&
                boss.tryStartPattern(choice, bossPos, bossFacing,
                                     player.position())) {
                selector.recordChoice(choice); // feed anti-repeat memory
            }
        }

        // Debug-render hook for the radial AoE. A real renderer would draw a
        // ring/circle here; we just describe it as text. Note rendering reads
        // this data and NEVER feeds back into the combat logic.
        std::string aoeText;
        const AoeCircleDebug aoe = boss.aoeDebug();
        if (aoe.visible) {
            aoeText = aoe.warning ? "  AoE[WARNING]" : "  AoE[BLAST]";
            aoeText += " r=" + std::to_string(static_cast<int>(aoe.radius));
        }

        const float dealt = prevHp - player.health().current();
        std::cout << "t=" << frame * dt << "s  "
                  << phaseName(boss.phase()) << " "
                  << patternName(boss.currentPattern())
                  << "  dist=" << distance(bossPos, player.position())
                  << "  HP=" << player.health().current()
                  << (dealt > 0.0f ? "  <-- HIT" : "")
                  << aoeText
                  << "\n";
    }

    std::cout << "Final player HP: " << player.health().current()
              << (player.isDead() ? " (DEAD)" : "") << "\n";
    return 0;
}
