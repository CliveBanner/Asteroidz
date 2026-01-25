#include "abilities.h"
#include "constants.h"
#include "weapons.h"
#include "utils.h"
#include <math.h>

static void UpdateCooldowns(AppState *s, int idx, float dt) {
    if (s->world.units.large_cannon_cooldown[idx] > 0)
        s->world.units.large_cannon_cooldown[idx] -= dt;
    for (int c = 0; c < 4; c++)
        if (s->world.units.small_cannon_cooldown[idx][c] > 0)
            s->world.units.small_cannon_cooldown[idx][c] -= dt;
    if (s->world.units.mining_cooldown[idx] > 0)
        s->world.units.mining_cooldown[idx] -= dt;
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
    bool is_gathering = false;
    
    if (s->world.units.has_target[idx]) {
        Command *cmd = &s->world.units.command_queue[idx][s->world.units.command_current_idx[idx]];
        if (cmd->type == CMD_MOVE) is_moving_normally = true;
        if (cmd->type == CMD_ATTACK_MOVE || cmd->type == CMD_PATROL) is_aggressive_cmd = true;
        if (cmd->type == CMD_GATHER || cmd->type == CMD_RETURN_CARGO) is_gathering = true;
    }

    if (s->world.units.behavior[idx] == BEHAVIOR_PASSIVE || is_moving_normally) return;
    
    // Most units stop firing while gathering/returning to focus energy, but Mothership can do both.
    if (is_gathering && s->world.units.type[idx] != UNIT_MOTHERSHIP) return;

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
            if (s->world.units.behavior[idx] == BEHAVIOR_HOLD_GROUND) {
                if (s->world.units.type[idx] == UNIT_MINER) continue; // Miners focus on mining
                range_mult = 1.0f; // Others act as turrets
            }
            else if (s->world.units.behavior[idx] == BEHAVIOR_DEFENSIVE) range_mult = WARNING_RANGE_NEAR / s->world.units.stats[idx]->small_cannon_range;
        }

        float dsq = Vector_DistanceSq(s->world.asteroids.pos[t_idx], s->world.units.pos[idx]);
        float max_d = (s->world.units.stats[idx]->small_cannon_range * range_mult) + s->world.asteroids.radius[t_idx] * ASTEROID_HITBOX_MULT;

        if (dsq <= max_d * max_d && s->world.units.small_cannon_cooldown[idx][c] <= 0) {
            Weapons_Fire(s, idx, t_idx, s->world.units.stats[idx]->small_cannon_damage, s->world.units.stats[idx]->small_cannon_energy_cost, false);
            s->world.units.small_cannon_cooldown[idx][c] = s->world.units.stats[idx]->small_cannon_cooldown;
        }
    }
}

void Abilities_Mine(AppState *s, int idx, int resource_idx, float dt) {
    if (!s->world.resources.active[resource_idx]) return;

    if (s->world.units.type[idx] == UNIT_MOTHERSHIP || s->world.units.current_cargo[idx] < s->world.units.stats[idx]->max_cargo) {
        float mining_rate = 100.0f; 
        float amount = mining_rate * dt;
        if (s->world.units.type[idx] != UNIT_MOTHERSHIP && s->world.units.current_cargo[idx] + amount > s->world.units.stats[idx]->max_cargo) {
            amount = s->world.units.stats[idx]->max_cargo - s->world.units.current_cargo[idx];
        }
        if (amount > s->world.resources.health[resource_idx]) {
            amount = s->world.resources.health[resource_idx];
        }
        if (amount > 0) {
            Weapons_MineCrystal(s, idx, resource_idx, amount);
            if (s->world.units.type[idx] == UNIT_MOTHERSHIP) {
                s->world.stored_resources += amount;
                s->ui.resource_accumulator += amount;
            } else {
                s->world.units.current_cargo[idx] += amount;
            }
        }
    }
}

void Abilities_Repair(AppState *s, int idx, int target_idx, float dt) {
    if (!s->world.units.active[target_idx]) return;
    
    float repair_rate = 50.0f; // 50 HP per second
    float amount = repair_rate * dt;
    
    if (s->world.units.health[target_idx] < s->world.units.stats[target_idx]->max_health) {
        s->world.units.health[target_idx] += amount;
        if (s->world.units.health[target_idx] > s->world.units.stats[target_idx]->max_health) {
            s->world.units.health[target_idx] = s->world.units.stats[target_idx]->max_health;
        }
        
        // VFX: Green beam
        float dx = s->world.units.pos[target_idx].x - s->world.units.pos[idx].x;
        float dy = s->world.units.pos[target_idx].y - s->world.units.pos[idx].y;
        float dist = sqrtf(dx*dx + dy*dy);
        if (dist > 0.1f) {
            int p_idx = s->world.particle_next_idx;
            s->world.particles.active[p_idx] = true;
            s->world.particles.type[p_idx] = PARTICLE_TRACER;
            s->world.particles.pos[p_idx] = s->world.units.pos[idx];
            s->world.particles.target_pos[p_idx] = s->world.units.pos[target_idx];
            s->world.particles.unit_idx[p_idx] = idx;
            s->world.particles.life[p_idx] = 0.3f;
            s->world.particles.size[p_idx] = 4.0f;
            s->world.particles.color[p_idx] = (SDL_Color){100, 255, 100, 255}; // Green
            s->world.particle_next_idx = (s->world.particle_next_idx + 1) % MAX_PARTICLES;
        }
    }
}

void Abilities_Update(AppState *s, int idx, float dt) {
    UpdateCooldowns(s, idx, dt);
    HandleManualMainCannon(s, idx);
    HandleAutoAttacks(s, idx);
    
    // Unload cargo if near Mothership
    if (s->world.units.current_cargo[idx] > 0 && s->world.units.type[idx] != UNIT_MOTHERSHIP) {
        int mothership_idx = -1;
        for (int i = 0; i < MAX_UNITS; i++) {
            if (s->world.units.active[i] && s->world.units.type[i] == UNIT_MOTHERSHIP) { mothership_idx = i; break; }
        }
        if (mothership_idx != -1) {
            float dsq = Vector_DistanceSq(s->world.units.pos[idx], s->world.units.pos[mothership_idx]);
            float unload_range = s->world.unit_stats[UNIT_MOTHERSHIP].radius + s->world.units.stats[idx]->radius + 50.0f;
            if (dsq <= unload_range * unload_range) {
                float unload_rate = 500.0f; 
                float amount = fminf(unload_rate * dt, s->world.units.current_cargo[idx]);
                s->world.units.current_cargo[idx] -= amount;
                s->world.stored_resources += amount;
                s->ui.resource_accumulator += amount;
            }
        }
    }

    if (s->world.units.has_target[idx]) {
        Command *cmd = &s->world.units.command_queue[idx][s->world.units.command_current_idx[idx]];
        if (cmd->type == CMD_GATHER && cmd->target_idx != -1) {
            if (s->world.units.current_cargo[idx] >= s->world.units.stats[idx]->max_cargo) {
                cmd->type = CMD_RETURN_CARGO;
            } else {
                float dsq = Vector_DistanceSq(s->world.resources.pos[cmd->target_idx], s->world.units.pos[idx]);
                // Must match stop_dist logic in game.c: (range * 0.8) + crystal_radius
                float range = (s->world.units.stats[idx]->small_cannon_range * 0.8f) + s->world.resources.radius[cmd->target_idx] * CRYSTAL_VISUAL_SCALE * 0.5f;
                if (dsq <= range * range) Abilities_Mine(s, idx, cmd->target_idx, dt);
            }
        } else if (cmd->type == CMD_RETURN_CARGO) {
            if (s->world.units.current_cargo[idx] <= 0) {
                if (cmd->target_idx != -1 && s->world.resources.active[cmd->target_idx]) {
                    cmd->type = CMD_GATHER;
                } else {
                    // Resource gone, just stop
                    s->world.units.has_target[idx] = false;
                }
            }
        }
    }
}