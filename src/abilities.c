#include "abilities.h"
#include "constants.h"
#include "weapons.h"
#include "utils.h"

static void UpdateCooldowns(Unit *u, float dt) {
    if (u->large_cannon_cooldown > 0)
        u->large_cannon_cooldown -= dt;
    for (int c = 0; c < 4; c++)
        if (u->small_cannon_cooldown[c] > 0)
            u->small_cannon_cooldown[c] -= dt;
}

static void HandleManualMainCannon(AppState *s, Unit *u) {
    SDL_LockMutex(s->threads.unit_fx_mutex);
    int l_target = u->large_target_idx;
    SDL_UnlockMutex(s->threads.unit_fx_mutex);

    if (l_target == -1) return;

    Asteroid *a = &s->world.asteroids[l_target];
    if (!a->active) {
        u->large_target_idx = -1;
        return;
    }

    a->targeted = true;
    float dsq = Vector_DistanceSq(a->pos, u->pos);
    float max_d = u->stats->main_cannon_range + a->radius * ASTEROID_HITBOX_MULT;

    if (dsq <= max_d * max_d) {
        if (u->large_cannon_cooldown <= 0) {
            Weapons_Fire(s, u, l_target, u->stats->main_cannon_damage, 0.0f, true);
            u->large_cannon_cooldown = u->stats->main_cannon_cooldown;
            u->large_target_idx = -1;
        }
    } else {
        u->large_target_idx = -1;
    }
}

static void HandleAutoAttacks(AppState *s, Unit *u) {
    bool is_moving_normally = false;
    bool is_aggressive_cmd = false;
    
    if (u->has_target) {
        Command *cmd = &u->command_queue[u->command_current_idx];
        if (cmd->type == CMD_MOVE) is_moving_normally = true;
        if (cmd->type == CMD_ATTACK_MOVE || cmd->type == CMD_PATROL) is_aggressive_cmd = true;
    }

    // Skip auto-attack if passive or performing a standard move
    if (u->behavior == BEHAVIOR_PASSIVE || is_moving_normally) return;

    SDL_LockMutex(s->threads.unit_fx_mutex);
    int s_targets[4];
    for (int c = 0; c < 4; c++) s_targets[c] = u->small_target_idx[c];
    SDL_UnlockMutex(s->threads.unit_fx_mutex);

    for (int c = 0; c < 4; c++) {
        int t_idx = s_targets[c];
        if (t_idx == -1) continue;

        Asteroid *a = &s->world.asteroids[t_idx];
        if (!a->active) continue;

        a->targeted = true;
        
        // Determine effective range based on behavior and command context
        float range_mult = 1.0f;
        bool is_command_target = false;
        if (u->has_target) {
            Command *cmd = &u->command_queue[u->command_current_idx];
            if (cmd->type == CMD_ATTACK_MOVE && cmd->target_idx == t_idx) is_command_target = true;
        }

        // Behavior-based range restrictions only apply if NOT explicitly commanded to attack or in aggressive mode
        if (!is_command_target && !is_aggressive_cmd) {
            if (u->behavior == BEHAVIOR_HOLD_GROUND) continue; // No auto-fire in Hold Ground
            if (u->behavior == BEHAVIOR_DEFENSIVE) range_mult = WARNING_RANGE_NEAR / u->stats->small_cannon_range;
        }

        float dsq = Vector_DistanceSq(a->pos, u->pos);
        float max_d = (u->stats->small_cannon_range * range_mult) + a->radius * ASTEROID_HITBOX_MULT;

        if (dsq <= max_d * max_d && u->small_cannon_cooldown[c] <= 0) {
            Weapons_Fire(s, u, t_idx, u->stats->small_cannon_damage, u->stats->small_cannon_energy_cost, false);
            u->small_cannon_cooldown[c] = u->stats->small_cannon_cooldown;
        }
    }
}

void Abilities_Update(AppState *s, Unit *u, float dt) {
    UpdateCooldowns(u, dt);
    HandleManualMainCannon(s, u);
    HandleAutoAttacks(s, u);
}