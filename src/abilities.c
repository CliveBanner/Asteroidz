#include "abilities.h"
#include "constants.h"
#include "weapons.h"
#include "utils.h"

void Abilities_Update(AppState *s, Unit *u, float dt) {
    if (u->large_cannon_cooldown > 0)
      u->large_cannon_cooldown -= dt;
    for (int c = 0; c < 4; c++)
      if (u->small_cannon_cooldown[c] > 0)
        u->small_cannon_cooldown[c] -= dt;

    // 1. Manual Main Cannon (Active Ability)
    SDL_LockMutex(s->threads.unit_fx_mutex);
    int l_target = u->large_target_idx;
    SDL_UnlockMutex(s->threads.unit_fx_mutex);

    if (l_target != -1 && s->world.asteroids[l_target].active) {
      s->world.asteroids[l_target].targeted = true;
      float dsq = Vector_DistanceSq(s->world.asteroids[l_target].pos, u->pos);
      float max_d = u->stats->main_cannon_range +
                    s->world.asteroids[l_target].radius * ASTEROID_HITBOX_MULT;

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

    // 2. Auto-Attacks (Small Cannons)
    if (s->input.auto_attack_enabled) {
      SDL_LockMutex(s->threads.unit_fx_mutex);
      int s_targets[4];
      for (int c = 0; c < 4; c++)
        s_targets[c] = u->small_target_idx[c];
      SDL_UnlockMutex(s->threads.unit_fx_mutex);

      for (int c = 0; c < 4; c++) {
        if (s_targets[c] != -1 && s->world.asteroids[s_targets[c]].active) {
          float dsq = Vector_DistanceSq(s->world.asteroids[s_targets[c]].pos, u->pos);
          float max_d = u->stats->small_cannon_range +
                        s->world.asteroids[s_targets[c]].radius * ASTEROID_HITBOX_MULT;

          if (dsq <= max_d * max_d) {
            if (u->small_cannon_cooldown[c] <= 0) {
              Weapons_Fire(s, u, s_targets[c], u->stats->small_cannon_damage,
                           u->stats->small_cannon_energy_cost, false);
              u->small_cannon_cooldown[c] = u->stats->small_cannon_cooldown;
            }
          }
        }
      }
    }
}
