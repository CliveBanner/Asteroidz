#include "abilities.h"
#include "constants.h"
#include "weapons.h"
#include "utils.h"

static void UpdateCooldowns(AppState *s, int idx, float dt) {
    if (s->world.units.large_cannon_cooldown[idx] > 0)
        s->world.units.large_cannon_cooldown[idx] -= dt;
    for (int c = 0; c < 4; c++)
        if (s->world.units.small_cannon_cooldown[idx][c] > 0)
            s->world.units.small_cannon_cooldown[idx][c] -= dt;
}

static void HandleManualMainCannon(AppState *s, int idx) {
    SDL_LockMutex(s->threads.unit_fx_mutex);
    int l_target = s->world.units.large_target_idx[idx];
    SDL_UnlockMutex(s->threads.unit_fx_mutex);

    if (l_target == -1) return;

    if (!s->world.asteroids.active[l_target]) {
        s->world.units.large_target_idx[idx] = -1;
        return;
    }

    s->world.asteroids.targeted[l_target] = true;
    float dsq = Vector_DistanceSq(s->world.asteroids.pos[l_target], s->world.units.pos[idx]);
    float max_d = s->world.units.stats[idx]->main_cannon_range + s->world.asteroids.radius[l_target] * ASTEROID_HITBOX_MULT;

    if (dsq <= max_d * max_d) {
        if (s->world.units.large_cannon_cooldown[idx] <= 0) {
            Weapons_Fire(s, idx, l_target, s->world.units.stats[idx]->main_cannon_damage, 0.0f, true);
            s->world.units.large_cannon_cooldown[idx] = s->world.units.stats[idx]->main_cannon_cooldown;
            s->world.units.large_target_idx[idx] = -1;
        }
    } else {
        s->world.units.large_target_idx[idx] = -1;
    }
}

static void HandleAutoAttacks(AppState *s, int idx) {
    bool is_moving_normally = false;
    bool is_aggressive_cmd = false;
    
    if (s->world.units.has_target[idx]) {
        Command *cmd = &s->world.units.command_queue[idx][s->world.units.command_current_idx[idx]];
        if (cmd->type == CMD_MOVE) is_moving_normally = true;
        if (cmd->type == CMD_ATTACK_MOVE || cmd->type == CMD_PATROL) is_aggressive_cmd = true;
    }

    if (s->world.units.behavior[idx] == BEHAVIOR_PASSIVE || is_moving_normally) return;

    SDL_LockMutex(s->threads.unit_fx_mutex);
    int s_targets[4];
    for (int c = 0; c < 4; c++) s_targets[c] = s->world.units.small_target_idx[idx][c];
    SDL_UnlockMutex(s->threads.unit_fx_mutex);

    for (int c = 0; c < 4; c++) {
        int t_idx = s_targets[c];
        if (t_idx == -1) continue;

        if (!s->world.asteroids.active[t_idx]) continue;

        s->world.asteroids.targeted[t_idx] = true;
        
        float range_mult = 1.0f;
        bool is_command_target = false;
        if (s->world.units.has_target[idx]) {
            Command *cmd = &s->world.units.command_queue[idx][s->world.units.command_current_idx[idx]];
            if (cmd->type == CMD_ATTACK_MOVE && cmd->target_idx == t_idx) is_command_target = true;
        }

        if (!is_command_target && !is_aggressive_cmd) {
            if (s->world.units.behavior[idx] == BEHAVIOR_HOLD_GROUND) continue;
            if (s->world.units.behavior[idx] == BEHAVIOR_DEFENSIVE) range_mult = WARNING_RANGE_NEAR / s->world.units.stats[idx]->small_cannon_range;
        }

        float dsq = Vector_DistanceSq(s->world.asteroids.pos[t_idx], s->world.units.pos[idx]);
        float max_d = (s->world.units.stats[idx]->small_cannon_range * range_mult) + s->world.asteroids.radius[t_idx] * ASTEROID_HITBOX_MULT;

        if (dsq <= max_d * max_d && s->world.units.small_cannon_cooldown[idx][c] <= 0) {
            Weapons_Fire(s, idx, t_idx, s->world.units.stats[idx]->small_cannon_damage, s->world.units.stats[idx]->small_cannon_energy_cost, false);
            s->world.units.small_cannon_cooldown[idx][c] = s->world.units.stats[idx]->small_cannon_cooldown;
        }
    }
}

void Abilities_Update(AppState *s, int idx, float dt) {
    UpdateCooldowns(s, idx, dt);
    HandleManualMainCannon(s, idx);
    HandleAutoAttacks(s, idx);
}