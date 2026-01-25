#include "ai.h"
#include "abilities.h"
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
        
        // --- Behavioral Target Prioritization ---
        Vec2 search_origin = s->world.units.pos[i];
        float behavior_search_range = s->world.units.stats[i]->small_cannon_range;
        
        if (s->world.units.type[i] == UNIT_FIGHTER) {
            if (s->world.units.behavior[i] == BEHAVIOR_DEFENSIVE) {
                // Protect Mothership: search near mothership
                for (int u = 0; u < MAX_UNITS; u++) {
                    if (s->world.units.active[u] && s->world.units.type[u] == UNIT_MOTHERSHIP) {
                        search_origin = s->world.units.pos[u];
                        behavior_search_range = 3000.0f;
                        break;
                    }
                }
            } else if (s->world.units.behavior[i] == BEHAVIOR_HOLD_GROUND) {
                // Protect nearest Miner
                float min_dsq = 1e15f; int best_miner = -1;
                for (int u = 0; u < MAX_UNITS; u++) {
                    if (s->world.units.active[u] && s->world.units.type[u] == UNIT_MINER) {
                        float dsq = Vector_DistanceSq(s->world.units.pos[i], s->world.units.pos[u]);
                        if (dsq < min_dsq) { min_dsq = dsq; best_miner = u; }
                    }
                }
                if (best_miner != -1) {
                    search_origin = s->world.units.pos[best_miner];
                    behavior_search_range = 2500.0f;
                }
            }
        }
        // ----------------------------------------

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

            float max_search_range = behavior_search_range;
            if (!is_aggressive_cmd) {
                if (s->world.units.behavior[i] == BEHAVIOR_DEFENSIVE) {
                    // Already set search_origin/range above for fighters
                    // For others, stay close logic is in movement, targeting remains same
                }
                else if (s->world.units.behavior[i] == BEHAVIOR_HOLD_GROUND) {
                    // Act like a turret: still shoot at anything in range
                    if (s->world.units.type[i] == UNIT_MINER) max_search_range = 0; // Miners focus on mining
                    else max_search_range = s->world.units.stats[i]->small_cannon_range;
                }
            }

            if (max_search_range > 0) {
                int best_target_idx = -1; float best_score = 1e15f;
                SDL_LockMutex(s->threads.unit_fx_mutex); int prev_targets[4]; for(int c=0; c<4; c++) prev_targets[c] = s->world.units.small_target_idx[i][c]; SDL_UnlockMutex(s->threads.unit_fx_mutex);
                
                for (int a = 0; a < MAX_ASTEROIDS; a++) {
                    if (!s->world.asteroids.active[a]) continue;
                    float dx = s->world.asteroids.pos[a].x - search_origin.x, dy = s->world.asteroids.pos[a].y - search_origin.y, dist = sqrtf(dx*dx + dy*dy), rad = s->world.asteroids.radius[a], surface_dist = fmaxf(0.0f, dist - rad);
                    
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

void AI_UpdateUnitMovement(AppState *s, int i, float dt) {
    if (!s->world.units.active[i]) return;

    // --- Behavioral Overrides (if idle) ---
    if (!s->world.units.has_target[i]) {
        int m_idx = -1;
        for (int u = 0; u < MAX_UNITS; u++) if (s->world.units.active[u] && s->world.units.type[u] == UNIT_MOTHERSHIP) { m_idx = u; break; }

        if (s->world.units.type[i] == UNIT_MINER) {
            if (s->world.units.behavior[i] == BEHAVIOR_DEFENSIVE && m_idx != -1) {
                // Stay close to Mothership and repair - spread out into rings
                float dsq = Vector_DistanceSq(s->world.units.pos[i], s->world.units.pos[m_idx]);
                float shell_idx = (float)(i % 8);
                float shell_depth = (float)(i / 8);
                float orbit_dist = 600.0f + (shell_depth * 300.0f);
                float angle = shell_idx * (2.0f * SDL_PI_F / 8.0f) + s->current_time * 0.1f;
                
                Vec2 target_pos = {
                    s->world.units.pos[m_idx].x + cosf(angle) * orbit_dist,
                    s->world.units.pos[m_idx].y + sinf(angle) * orbit_dist
                };

                float dist_to_target_sq = Vector_DistanceSq(s->world.units.pos[i], target_pos);
                if (dist_to_target_sq > 100.0f * 100.0f) {
                    s->world.units.command_queue[i][0] = (Command){.type = CMD_MOVE, .pos = target_pos};
                    s->world.units.command_count[i] = 1;
                    s->world.units.command_current_idx[i] = 0;
                    s->world.units.has_target[i] = true;
                } else {
                    Abilities_Repair(s, i, m_idx, dt);
                }
            } else if (s->world.units.behavior[i] == BEHAVIOR_HOLD_GROUND) {
                // Spread out to mine (original logic)
                if (s->world.units.current_cargo[i] >= s->world.units.stats[i]->max_cargo) {
                    s->world.units.command_queue[i][0] = (Command){.type = CMD_RETURN_CARGO};
                    s->world.units.command_count[i] = 1;
                    s->world.units.command_current_idx[i] = 0;
                    s->world.units.has_target[i] = true;
                } else {
                    int best_c = -1; float min_dsq = 1e15f;
                    for (int r = 0; r < MAX_RESOURCES; r++) if (s->world.resources.active[r]) {
                        float dsq = Vector_DistanceSq(s->world.units.pos[i], s->world.resources.pos[r]);
                        if (dsq < min_dsq) { min_dsq = dsq; best_c = r; }
                    }
                    if (best_c != -1 && min_dsq < 8000.0f * 8000.0f) {
                        s->world.units.command_queue[i][0] = (Command){.type = CMD_GATHER, .target_idx = best_c, .pos = s->world.resources.pos[best_c]};
                        s->world.units.command_count[i] = 1;
                        s->world.units.command_current_idx[i] = 0;
                        s->world.units.has_target[i] = true;
                    }
                }
            }
        } else if (s->world.units.type[i] == UNIT_FIGHTER) {
            int target_u = -1;
            if (s->world.units.behavior[i] == BEHAVIOR_DEFENSIVE && m_idx != -1) {
                target_u = m_idx; // Protect Mothership
            } else if (s->world.units.behavior[i] == BEHAVIOR_HOLD_GROUND) {
                // Protect nearest Miner
                float min_dsq = 1e15f;
                for (int u = 0; u < MAX_UNITS; u++) if (s->world.units.active[u] && s->world.units.type[u] == UNIT_MINER) {
                    float dsq = Vector_DistanceSq(s->world.units.pos[i], s->world.units.pos[u]);
                    if (dsq < min_dsq) { min_dsq = dsq; target_u = u; }
                }
            }

            if (target_u != -1) {
                float shell_idx = (float)(i % 8);
                float shell_depth = (float)(i / 8);
                float base_dist = (target_u == m_idx) ? 500.0f : 250.0f;
                float orbit_dist = base_dist + (shell_depth * 250.0f);
                float angle = shell_idx * (2.0f * SDL_PI_F / 8.0f) + s->current_time * 0.3f;

                Vec2 offset = { cosf(angle) * orbit_dist, sinf(angle) * orbit_dist };
                s->world.units.command_queue[i][0] = (Command){.type = CMD_MOVE, .pos = Vector_Add(s->world.units.pos[target_u], offset)};
                s->world.units.command_count[i] = 1;
                s->world.units.command_current_idx[i] = 0;
                s->world.units.has_target[i] = true;
            }
        }
    } else if (s->world.units.command_count[i] == 1 && s->world.units.command_queue[i][0].type == CMD_MOVE) {
        // Update following positions for idle behaviors
        int m_idx = -1;
        for (int u = 0; u < MAX_UNITS; u++) if (s->world.units.active[u] && s->world.units.type[u] == UNIT_MOTHERSHIP) { m_idx = u; break; }

        if (s->world.units.type[i] == UNIT_FIGHTER) {
            int target_u = -1;
            if (s->world.units.behavior[i] == BEHAVIOR_DEFENSIVE) target_u = m_idx;
            else if (s->world.units.behavior[i] == BEHAVIOR_HOLD_GROUND) {
                float min_dsq = 1e15f;
                for (int u = 0; u < MAX_UNITS; u++) if (s->world.units.active[u] && s->world.units.type[u] == UNIT_MINER) {
                    float dsq = Vector_DistanceSq(s->world.units.pos[i], s->world.units.pos[u]);
                    if (dsq < min_dsq) { min_dsq = dsq; target_u = u; }
                }
            }
            if (target_u != -1) {
                float shell_idx = (float)(i % 8);
                float shell_depth = (float)(i / 8);
                float base_dist = (target_u == m_idx) ? 500.0f : 250.0f;
                float orbit_dist = base_dist + (shell_depth * 250.0f);
                float angle = shell_idx * (2.0f * SDL_PI_F / 8.0f) + s->current_time * 0.3f;

                Vec2 offset = { cosf(angle) * orbit_dist, sinf(angle) * orbit_dist };
                s->world.units.command_queue[i][0].pos = Vector_Add(s->world.units.pos[target_u], offset);
            }
        } else if (s->world.units.type[i] == UNIT_MINER && s->world.units.behavior[i] == BEHAVIOR_DEFENSIVE && m_idx != -1) {
            float shell_idx = (float)(i % 8);
            float shell_depth = (float)(i / 8);
            float orbit_dist = 600.0f + (shell_depth * 300.0f);
            float angle = shell_idx * (2.0f * SDL_PI_F / 8.0f) + s->current_time * 0.1f;
            
            Vec2 target_pos = {
                s->world.units.pos[m_idx].x + cosf(angle) * orbit_dist,
                s->world.units.pos[m_idx].y + sinf(angle) * orbit_dist
            };
            s->world.units.command_queue[i][0].pos = target_pos;
        }
    }
    // ------------------------------------

    if (s->world.units.has_target[i]) {
      Command *cur_cmd = &s->world.units.command_queue[i][s->world.units.command_current_idx[i]];

      // Auto-advance if target-based command target is dead
      if (cur_cmd->type == CMD_ATTACK_MOVE && cur_cmd->target_idx != -1) {
          int ti = cur_cmd->target_idx;
          if (!s->world.asteroids.active[ti]) {
            s->world.units.command_current_idx[i]++;
            if (s->world.units.command_current_idx[i] >= s->world.units.command_count[i]) {
              s->world.units.has_target[i] = false;
              s->world.units.command_count[i] = 0;
              s->world.units.command_current_idx[i] = 0;
            }
            return; // Skip this frame
          }
          cur_cmd->pos = s->world.asteroids.pos[ti];
      }
      
      if (cur_cmd->type == CMD_GATHER && cur_cmd->target_idx != -1) {
          int ti = cur_cmd->target_idx;
          if (!s->world.resources.active[ti]) {
            s->world.units.command_current_idx[i]++;
            if (s->world.units.command_current_idx[i] >= s->world.units.command_count[i]) {
              s->world.units.has_target[i] = false;
              s->world.units.command_count[i] = 0;
              s->world.units.command_current_idx[i] = 0;
            }
            return;
          }
          cur_cmd->pos = s->world.resources.pos[ti];
      }
      
      if (cur_cmd->type == CMD_RETURN_CARGO) {
          // Find Mothership
          int m_idx = -1;
          for (int u = 0; u < MAX_UNITS; u++) {
              if (s->world.units.active[u] && s->world.units.type[u] == UNIT_MOTHERSHIP) { m_idx = u; break; }
          }
          if (m_idx != -1) cur_cmd->pos = s->world.units.pos[m_idx];
          else {
              // No mothership? cancel
              s->world.units.has_target[i] = false; return;
          }
      }

      if (s->world.units.has_target[i]) {
        float dsq = Vector_DistanceSq(cur_cmd->pos, s->world.units.pos[i]);
        float stop_dist = UNIT_STOP_DIST;
        
        if (cur_cmd->type == CMD_ATTACK_MOVE && cur_cmd->target_idx != -1) {
            int ti = cur_cmd->target_idx;
            stop_dist = s->world.units.stats[i]->small_cannon_range + s->world.asteroids.radius[ti] * ASTEROID_HITBOX_MULT;
            stop_dist *= 0.95f;
        }
        
        if (cur_cmd->type == CMD_GATHER && cur_cmd->target_idx != -1) {
            int ti = cur_cmd->target_idx;
            stop_dist = (s->world.units.stats[i]->small_cannon_range * 0.8f) + s->world.resources.radius[ti] * CRYSTAL_VISUAL_SCALE * 0.5f;
            stop_dist *= 0.95f;
        }
        
        if (cur_cmd->type == CMD_RETURN_CARGO) {
            stop_dist = s->world.unit_stats[UNIT_MOTHERSHIP].radius + s->world.units.stats[i]->radius + 20.0f;
        }

        if (dsq > stop_dist * stop_dist) {
          float dist = sqrtf(dsq), speed = s->world.units.stats[i]->speed;
          if (cur_cmd->type != CMD_PATROL &&
              (s->world.units.command_current_idx[i] == s->world.units.command_count[i] - 1) &&
              dist < UNIT_BRAKING_DIST)
            speed *= (dist / UNIT_BRAKING_DIST);
          Vec2 target_v = Vector_Scale(
              Vector_Normalize(Vector_Sub(cur_cmd->pos, s->world.units.pos[i])), speed);
          s->world.units.velocity[i].x +=
              (target_v.x - s->world.units.velocity[i].x) * UNIT_STEERING_FORCE * dt;
          s->world.units.velocity[i].y +=
              (target_v.y - s->world.units.velocity[i].y) * UNIT_STEERING_FORCE * dt;
        } else {
          bool should_advance = true;
          if (cur_cmd->type == CMD_ATTACK_MOVE && cur_cmd->target_idx != -1) {
              should_advance = false;
              s->world.units.velocity[i] = (Vec2){0,0};
          }
          if (cur_cmd->type == CMD_GATHER && cur_cmd->target_idx != -1) {
              should_advance = false;
              s->world.units.velocity[i] = (Vec2){0,0};
          }
          if (cur_cmd->type == CMD_RETURN_CARGO) {
              // Wait until empty
              if (s->world.units.current_cargo[i] <= 0) {
                  should_advance = true;
              } else {
                  should_advance = false;
                  s->world.units.velocity[i] = (Vec2){0,0};
              }
          }

          if (should_advance) {
              if (cur_cmd->type == CMD_PATROL) {
                if (s->world.units.command_current_idx[i] == s->world.units.command_count[i] - 1) {
                  int first_patrol = s->world.units.command_current_idx[i];
                  while (first_patrol > 0 &&
                         s->world.units.command_queue[i][first_patrol - 1].type == CMD_PATROL) {
                    first_patrol--;
                  }
                  s->world.units.command_current_idx[i] = first_patrol;
                } else {
                  s->world.units.command_current_idx[i]++;
                }
              } else {
                s->world.units.command_current_idx[i]++;
                if (s->world.units.command_current_idx[i] >= s->world.units.command_count[i]) {
                  s->world.units.has_target[i] = false;
                  s->world.units.command_count[i] = 0;
                  s->world.units.command_current_idx[i] = 0;
                }
              }
          }
        }
      }
    }

    // --- Avoidance Logic ---
    Vec2 avoidance = {0,0};
    
    // Asteroid Avoidance
    for (int a = 0; a < MAX_ASTEROIDS; a++) {
        if (!s->world.asteroids.active[a]) continue;
        float dist_sq = Vector_DistanceSq(s->world.units.pos[i], s->world.asteroids.pos[a]);
        float safe_dist = s->world.units.stats[i]->radius + s->world.asteroids.radius[a] + 150.0f;
        if (dist_sq < safe_dist * safe_dist) {
            float dist = sqrtf(dist_sq);
            Vec2 away = Vector_Normalize(Vector_Sub(s->world.units.pos[i], s->world.asteroids.pos[a]));
            avoidance = Vector_Add(avoidance, Vector_Scale(away, (safe_dist - dist) * 10.0f));
        }
    }

    // Unit Separation
    for (int u = 0; u < MAX_UNITS; u++) {
        if (i == u || !s->world.units.active[u]) continue;
        float dist_sq = Vector_DistanceSq(s->world.units.pos[i], s->world.units.pos[u]);
        float safe_dist = s->world.units.stats[i]->radius + s->world.units.stats[u]->radius + 40.0f;
        if (dist_sq < safe_dist * safe_dist) {
            float dist = sqrtf(dist_sq);
            if (dist < 0.1f) dist = 0.1f;
            Vec2 away = Vector_Normalize(Vector_Sub(s->world.units.pos[i], s->world.units.pos[u]));
            avoidance = Vector_Add(avoidance, Vector_Scale(away, (safe_dist - dist) * 5.0f));
        }
    }
    
    s->world.units.velocity[i].x += avoidance.x * dt;
    s->world.units.velocity[i].y += avoidance.y * dt;
    // -----------------------

    s->world.units.velocity[i] = Vector_Sub(
        s->world.units.velocity[i], Vector_Scale(s->world.units.velocity[i], s->world.units.stats[i]->friction * dt));
    s->world.units.pos[i] = Vector_Add(s->world.units.pos[i], Vector_Scale(s->world.units.velocity[i], dt));

    float speed_sq =
        s->world.units.velocity[i].x * s->world.units.velocity[i].x + s->world.units.velocity[i].y * s->world.units.velocity[i].y;
    bool should_rotate = false;
    float target_rot = 0.0f;

    if (speed_sq > 100.0f) {
      target_rot = atan2f(s->world.units.velocity[i].y, s->world.units.velocity[i].x) * (180.0f / SDL_PI_F) + 90.0f;
      should_rotate = true;
    } else if (s->world.units.has_target[i]) {
      Command *cur_cmd = &s->world.units.command_queue[i][s->world.units.command_current_idx[i]];
      Vec2 dir = Vector_Sub(cur_cmd->pos, s->world.units.pos[i]);
      if (Vector_Length(dir) > 0.1f) {
          target_rot = atan2f(dir.y, dir.x) * (180.0f / SDL_PI_F) + 90.0f;
          should_rotate = true;
      }
    }

    if (should_rotate) {
      float diff = target_rot - s->world.units.rotation[i];
      while (diff < -180.0f) diff += 360.0f;
      while (diff > 180.0f) diff -= 360.0f;
      s->world.units.rotation[i] += diff * UNIT_STEERING_FORCE * dt;
    }
}