#include "ai.h"
#include "constants.h"
#include "utils.h"
#include <math.h>

int AI_UnitTargetingThread(void *data) {
  AppState *s = (AppState *)data;
  while (SDL_GetAtomicInt(&s->threads.bg_should_quit) == 0) {
    for (int i = 0; i < MAX_UNITS; i++) {
        if (!s->world.units[i].active) continue;
        Unit *u = &s->world.units[i];
        int best_s[4] = {-1, -1, -1, -1};
        int manual_target = -1;
        if (u->has_target) {
            Command *cur = &u->command_queue[u->command_current_idx];
            if (cur->type == CMD_ATTACK_MOVE && cur->target_idx != -1 && s->world.asteroids.active[cur->target_idx]) {
                float dx = s->world.asteroids.pos[cur->target_idx].x - u->pos.x, dy = s->world.asteroids.pos[cur->target_idx].y - u->pos.y;
                if (dx*dx + dy*dy < WARNING_RANGE_FAR * WARNING_RANGE_FAR) manual_target = cur->target_idx;
            }
        }
        if (manual_target != -1) { for(int c=0; c<4; c++) best_s[c] = manual_target; }
        else if (u->behavior != BEHAVIOR_PASSIVE) {
            bool is_aggressive_cmd = false;
            if (u->has_target) {
                CommandType ct = u->command_queue[u->command_current_idx].type;
                if (ct == CMD_ATTACK_MOVE || ct == CMD_PATROL) is_aggressive_cmd = true;
            }

            float max_search_range = u->stats->small_cannon_range;
            if (!is_aggressive_cmd) {
                if (u->behavior == BEHAVIOR_DEFENSIVE) max_search_range = WARNING_RANGE_NEAR;
                else if (u->behavior == BEHAVIOR_HOLD_GROUND) max_search_range = 0; // Don't auto-acquire in Hold Ground
            }

            if (max_search_range > 0) {
                int best_target_idx = -1; float best_score = 1e15f;
                SDL_LockMutex(s->threads.unit_fx_mutex); int prev_targets[4]; for(int c=0; c<4; c++) prev_targets[c] = u->small_target_idx[c]; SDL_UnlockMutex(s->threads.unit_fx_mutex);
                for (int a = 0; a < MAX_ASTEROIDS; a++) {
                    if (!s->world.asteroids.active[a]) continue;
                    float dx = s->world.asteroids.pos[a].x - u->pos.x, dy = s->world.asteroids.pos[a].y - u->pos.y, dist = sqrtf(dx*dx + dy*dy), rad = s->world.asteroids.radius[a], surface_dist = fmaxf(0.0f, dist - rad);
                    if (surface_dist <= max_search_range) {
                        float score = surface_dist - (rad * 0.15f);
                        for(int c=0; c<4; c++) if (a == prev_targets[c]) { score *= 0.8f; break; } // Stickiness
                        if (score < best_score) { best_score = score; best_target_idx = a; }
                    }
                }
                if (best_target_idx != -1) for(int c=0; c<4; c++) best_s[c] = best_target_idx;
            }
        }
        SDL_LockMutex(s->threads.unit_fx_mutex); for(int c=0; c<4; c++) u->small_target_idx[c] = best_s[c]; SDL_UnlockMutex(s->threads.unit_fx_mutex);
    }
    SDL_Delay(16); 
  }
  return 0;
}

void AI_StartThreads(AppState *s) {
  s->threads.radar_thread = SDL_CreateThread(AI_UnitTargetingThread, "Targeting", s);
}