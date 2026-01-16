#include "ai.h"
#include "constants.h"
#include "utils.h"
#include <math.h>

int AI_UnitTargetingThread(void *data) {
  AppState *s = (AppState *)data;
  while (SDL_GetAtomicInt(&s->threads.bg_should_quit) == 0) {
    for (int i = 0; i < MAX_UNITS; i++) {
        if (!s->world.units.active[i]) continue;
        int best_s[4] = {-1, -1, -1, -1};
        int manual_target = -1;
        if (s->world.units.has_target[i]) {
            Command *cur = &s->world.units.command_queue[i][s->world.units.command_current_idx[i]];
            if (cur->type == CMD_ATTACK_MOVE && cur->target_idx != -1 && s->world.asteroids.active[cur->target_idx]) {
                float dx = s->world.asteroids.pos[cur->target_idx].x - s->world.units.pos[i].x, dy = s->world.asteroids.pos[cur->target_idx].y - s->world.units.pos[i].y;
                if (dx*dx + dy*dy < WARNING_RANGE_FAR * WARNING_RANGE_FAR) manual_target = cur->target_idx;
            }
        }
        if (manual_target != -1) { for(int c=0; c<4; c++) best_s[c] = manual_target; }
        else if (s->world.units.behavior[i] != BEHAVIOR_PASSIVE) {
            bool is_aggressive_cmd = false;
            if (s->world.units.has_target[i]) {
                CommandType ct = s->world.units.command_queue[i][s->world.units.command_current_idx[i]].type;
                if (ct == CMD_ATTACK_MOVE || ct == CMD_PATROL) is_aggressive_cmd = true;
            }

            float max_search_range = s->world.units.stats[i]->small_cannon_range;
            if (!is_aggressive_cmd) {
                if (s->world.units.behavior[i] == BEHAVIOR_DEFENSIVE) max_search_range = WARNING_RANGE_NEAR;
                else if (s->world.units.behavior[i] == BEHAVIOR_HOLD_GROUND) max_search_range = 0;
            }

            if (max_search_range > 0) {
                int best_target_idx = -1; float best_score = 1e15f;
                SDL_LockMutex(s->threads.unit_fx_mutex); int prev_targets[4]; for(int c=0; c<4; c++) prev_targets[c] = s->world.units.small_target_idx[i][c]; SDL_UnlockMutex(s->threads.unit_fx_mutex);
                for (int a = 0; a < MAX_ASTEROIDS; a++) {
                    if (!s->world.asteroids.active[a]) continue;
                    float dx = s->world.asteroids.pos[a].x - s->world.units.pos[i].x, dy = s->world.asteroids.pos[a].y - s->world.units.pos[i].y, dist = sqrtf(dx*dx + dy*dy), rad = s->world.asteroids.radius[a], surface_dist = fmaxf(0.0f, dist - rad);
                    if (surface_dist <= max_search_range) {
                        float score = surface_dist - (rad * 0.15f);
                        for(int c=0; c<4; c++) if (a == prev_targets[c]) { score *= 0.8f; break; }
                        if (score < best_score) { best_score = score; best_target_idx = a; }
                    }
                }
                if (best_target_idx != -1) for(int c=0; c<4; c++) best_s[c] = best_target_idx;
            }
        }
        SDL_LockMutex(s->threads.unit_fx_mutex); for(int c=0; c<4; c++) s->world.units.small_target_idx[i][c] = best_s[c]; SDL_UnlockMutex(s->threads.unit_fx_mutex);
    }
    SDL_Delay(16); 
  }
  return 0;
}

void AI_StartThreads(AppState *s) {
  s->threads.radar_thread = SDL_CreateThread(AI_UnitTargetingThread, "Targeting", s);
}
