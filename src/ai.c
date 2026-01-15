#include "ai.h"
#include "game.h"
#include "constants.h"
#include <math.h>

static int SDLCALL AI_UnitTargetingThread(void *data) {
  AppState *s = (AppState *)data;
  while (SDL_GetAtomicInt(&s->threads.unit_fx_should_quit) == 0) {
    for (int i = 0; i < MAX_UNITS; i++) {
        if (!s->world.units[i].active) continue;
        Unit *u = &s->world.units[i];
        int best_s[4] = {-1, -1, -1, -1};
        int manual_target = -1;
        if (u->has_target) {
            Command *cur = &u->command_queue[u->command_current_idx];
            if (cur->type == CMD_ATTACK_MOVE && cur->target_idx != -1 && s->world.asteroids[cur->target_idx].active) {
                float dx = s->world.asteroids[cur->target_idx].pos.x - u->pos.x, dy = s->world.asteroids[cur->target_idx].pos.y - u->pos.y;
                if (dx*dx + dy*dy < WARNING_RANGE_FAR * WARNING_RANGE_FAR) manual_target = cur->target_idx;
            }
        }
        if (manual_target != -1) { for(int c=0; c<4; c++) best_s[c] = manual_target; }
        else if (u->behavior == BEHAVIOR_OFFENSIVE || u->behavior == BEHAVIOR_DEFENSIVE) {
            float max_search_range = u->stats->small_cannon_range;
            if (u->behavior == BEHAVIOR_DEFENSIVE) max_search_range = WARNING_RANGE_NEAR;

            int best_target_idx = -1; float best_score = 1e15f;
            SDL_LockMutex(s->threads.unit_fx_mutex); int prev_targets[4]; for(int c=0; c<4; c++) prev_targets[c] = u->small_target_idx[c]; SDL_UnlockMutex(s->threads.unit_fx_mutex);
            for (int a = 0; a < MAX_ASTEROIDS; a++) {
                if (!s->world.asteroids[a].active) continue;
                float dx = s->world.asteroids[a].pos.x - u->pos.x, dy = s->world.asteroids[a].pos.y - u->pos.y, dist = sqrtf(dx*dx + dy*dy), rad = s->world.asteroids[a].radius, surface_dist = fmaxf(0.0f, dist - rad);
                if (surface_dist <= max_search_range) {
                    float score = surface_dist - (rad * 0.15f);
                    for(int c=0; c<4; c++) if (a == prev_targets[c]) { score *= 0.8f; break; } // Stickiness
                    if (score < best_score) { best_score = score; best_target_idx = a; }
                }
            }
            if (best_target_idx != -1) for(int c=0; c<4; c++) best_s[c] = best_target_idx;
        }
        SDL_LockMutex(s->threads.unit_fx_mutex); for(int c=0; c<4; c++) u->small_target_idx[c] = best_s[c]; SDL_UnlockMutex(s->threads.unit_fx_mutex);
    }
    SDL_Delay(16); 
  }
  return 0;
}

static int SDLCALL AI_RadarThread(void *data) {
  AppState *s = (AppState *)data;
  while (SDL_GetAtomicInt(&s->threads.radar_should_quit) == 0) {
    if (SDL_GetAtomicInt(&s->threads.radar_request_update) == 1) {
      SDL_LockMutex(s->threads.radar_mutex); Vec2 m_pos = s->threads.radar_mothership_pos; SDL_UnlockMutex(s->threads.radar_mutex); int count = 0;
      for (int i = 0; i < MAX_ASTEROIDS; i++) { if (s->world.asteroids[i].active && count < MAX_RADAR_BLIPS) { float dx = s->world.asteroids[i].pos.x - m_pos.x, dy = s->world.asteroids[i].pos.y - m_pos.y; if (dx * dx + dy * dy < MOTHERSHIP_RADAR_RANGE * MOTHERSHIP_RADAR_RANGE) { s->threads.radar_blips[count++].pos = s->world.asteroids[i].pos; } } }
      SDL_LockMutex(s->threads.radar_mutex); s->threads.radar_blip_count = count; SDL_UnlockMutex(s->threads.radar_mutex); SDL_SetAtomicInt(&s->threads.radar_request_update, 0);
    } else SDL_Delay(33);
  }
  return 0;
}

void AI_StartThreads(AppState *s) {
    s->threads.unit_fx_thread = SDL_CreateThread(AI_UnitTargetingThread, "AI_UnitTargeting", s);
    s->threads.radar_thread = SDL_CreateThread(AI_RadarThread, "AI_Radar", s);
}
