#include "game.h"
#include "constants.h"
#include "physics.h"
#include "particles.h"
#include "utils.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

void SpawnAsteroid(AppState *s, Vec2 pos, Vec2 vel_dir, float radius) {
  if (s->asteroid_count >= MAX_ASTEROIDS)
    return;
  for (int i = 0; i < MAX_ASTEROIDS; i++) {
    if (!s->asteroids[i].active) {
      s->asteroids[i].pos = pos;
      float speed = ASTEROID_SPEED_FACTOR / radius;
      s->asteroids[i].velocity.x = vel_dir.x * speed;
      s->asteroids[i].velocity.y = vel_dir.y * speed;
      s->asteroids[i].radius = radius;
      s->asteroids[i].rotation = (float)(rand() % 360);
      s->asteroids[i].rot_speed = ((float)(rand() % 100) / 50.0f - 1.0f) *
                                  (ASTEROID_ROTATION_SPEED_FACTOR / radius);
      s->asteroids[i].tex_idx = rand() % ASTEROID_TYPE_COUNT;
      s->asteroids[i].active = true;
      s->asteroids[i].max_health = radius * ASTEROID_HEALTH_MULT;
      s->asteroids[i].health = s->asteroids[i].max_health;
      s->asteroids[i].targeted = false;
      s->asteroid_count++;
      break;
    }
  }
}

static void UpdateSimAnchors(AppState *s, Vec2 cam_center) {
  s->sim_anchor_count = 0;
  s->sim_anchors[s->sim_anchor_count++].pos = cam_center;
  for (int i = 0; i < MAX_UNITS; i++) {
      if (!s->units[i].active) continue;
      if (s->sim_anchor_count >= MAX_SIM_ANCHORS) break;
      bool covered = false;
      for (int a = 0; a < s->sim_anchor_count; a++) {
          float dx = s->units[i].pos.x - s->sim_anchors[a].pos.x;
          float dy = s->units[i].pos.y - s->sim_anchors[a].pos.y;
          if (dx * dx + dy * dy < (DESPAWN_RANGE * 0.5f) * (DESPAWN_RANGE * 0.5f)) {
              covered = true; break;
          }
      }
      if (!covered) s->sim_anchors[s->sim_anchor_count++].pos = s->units[i].pos;
  }
}

void Game_Init(AppState *s) {
    // Mothership Stats
    s->unit_stats[UNIT_MOTHERSHIP] = (UnitStats){
        .max_health = 1000.0f,
        .max_energy = 500.0f,
        .speed = MOTHERSHIP_SPEED,
        .friction = MOTHERSHIP_FRICTION,
        .radius = MOTHERSHIP_RADIUS,
        .radar_range = MOTHERSHIP_RADAR_RANGE,
        .main_cannon_damage = LARGE_CANNON_DAMAGE,
        .main_cannon_range = LARGE_CANNON_RANGE,
        .main_cannon_cooldown = LARGE_CANNON_COOLDOWN,
        .small_cannon_damage = SMALL_CANNON_DAMAGE,
        .small_cannon_range = SMALL_CANNON_RANGE,
        .small_cannon_cooldown = SMALL_CANNON_COOLDOWN,
        .small_cannon_energy_cost = SMALL_CANNON_ENERGY_COST
    };

    // Scout Stats
    s->unit_stats[UNIT_SCOUT] = (UnitStats){
        .max_health = 200.0f,
        .max_energy = 100.0f,
        .speed = 1800.0f,
        .friction = 3.0f,
        .radius = 60.0f,
        .radar_range = 4000.0f,
        .main_cannon_damage = 0,
        .main_cannon_range = 0,
        .small_cannon_damage = 200.0f,
        .small_cannon_range = 1500.0f,
        .small_cannon_cooldown = 0.5f,
        .small_cannon_energy_cost = 5.0f
    };

    for (int i = 0; i < MAX_UNITS; i++) s->units[i].active = false;
    s->unit_count = 0;
    s->energy = INITIAL_ENERGY;

    // Spawn Initial Mothership
    int idx = s->unit_count++;
    Unit *u = &s->units[idx];
    u->active = true;
    u->type = UNIT_MOTHERSHIP;
    u->stats = &s->unit_stats[UNIT_MOTHERSHIP];
    u->pos = (Vec2){0, 0};
    u->velocity = (Vec2){0, 0};
    u->rotation = 0;
    u->health = u->stats->max_health;
    u->energy = u->stats->max_energy;
    u->command_count = 0;
    u->has_target = false;
    u->large_target_idx = -1;
    for(int c=0; c<4; c++) u->small_target_idx[c] = -1;
    
    s->selected_unit_idx = 0;
    SDL_memset(s->unit_selected, 0, sizeof(s->unit_selected));
    s->unit_selected[0] = true;
}

static void FireWeapon(AppState *s, Unit *u, int asteroid_idx, float damage, float energy_cost, bool is_main_cannon) {
    if (is_main_cannon) {
        // Main cannon is free (uses cooldown)
    } else {
        if (s->energy < energy_cost) return;
        s->energy -= energy_cost;
    }
    Asteroid *a = &s->asteroids[asteroid_idx];
    a->health -= damage;
    a->radius -= (damage / ASTEROID_HEALTH_MULT) * 0.2f;
    if (a->health <= 0 || a->radius < ASTEROID_MIN_RADIUS) {
        a->active = false;
        s->asteroid_count--;
        Particles_SpawnExplosion(s, a->pos, 40, a->max_health / 1000.0f, EXPLOSION_COLLISION, a->tex_idx);
    }
    
    float dx = a->pos.x - u->pos.x;
    float dy = a->pos.y - u->pos.y;
    float dist = sqrtf(dx * dx + dy * dy);
    Vec2 start_pos = u->pos;
    Vec2 impact_pos = a->pos;

    if (dist > 0.1f) {
        float unit_r = u->stats->radius * MOTHERSHIP_VISUAL_SCALE * 0.6f; // Moved further out from center
        float ast_r = a->radius * ASTEROID_CORE_SCALE * 0.5f; // Less deep penetration
        start_pos.x += (dx / dist) * unit_r;
        start_pos.y += (dy / dist) * unit_r;
        impact_pos.x -= (dx / dist) * ast_r;
        impact_pos.y -= (dy / dist) * ast_r;
    }

    int p_idx = s->particle_next_idx;
    s->particles[p_idx].active = true;
    s->particles[p_idx].type = PARTICLE_TRACER;
    s->particles[p_idx].pos = start_pos;
    s->particles[p_idx].target_pos = impact_pos;
    s->particles[p_idx].life = 1.0f; 
    s->particles[p_idx].size = 1.0f + (damage / 50.0f);
    s->particles[p_idx].color = (SDL_Color)COLOR_LASER_RED; 
    s->particle_next_idx = (s->particle_next_idx + 1) % MAX_PARTICLES;

    // Muzzle flash
    Particles_SpawnLaserFlash(s, start_pos, s->particles[p_idx].size, false);

    // Impact Effect
    float impact_scale = 0.5f + (damage / 2000.0f);
    Particles_SpawnExplosion(s, impact_pos, 15, impact_scale, EXPLOSION_IMPACT, a->tex_idx); 
    
    // Impact flash
    Particles_SpawnLaserFlash(s, impact_pos, s->particles[p_idx].size, true);
}

static void HandleRespawn(AppState *s, float dt, int win_w, int win_h) {
    if (s->respawn_timer <= 0) return;
    
    s->respawn_timer -= dt;
    s->camera_pos.x = s->respawn_pos.x - (win_w / 2.0f) / s->zoom;
    s->camera_pos.y = s->respawn_pos.y - (win_h / 2.0f) / s->zoom;

    if (s->respawn_timer <= 0) {
        for (int i = 0; i < MAX_UNITS; i++) {
            if (s->units[i].type == UNIT_MOTHERSHIP) {
                Unit *u = &s->units[i];
                u->active = true;
                u->health = u->stats->max_health;
                u->energy = u->stats->max_energy;
                u->velocity = (Vec2){0, 0};
                u->command_count = 0;
                u->command_current_idx = 0;
                u->has_target = false;

                int attempts = 0;
                while (attempts < RESPAWN_ATTEMPTS) {
                    float rx = (float)(rand() % (int)(RESPAWN_RANGE * 2) - RESPAWN_RANGE), ry = (float)(rand() % (int)(RESPAWN_RANGE * 2) - RESPAWN_RANGE);
                    bool safe = true;
                    for (int j = 0; j < MAX_ASTEROIDS; j++) {
                        if (!s->asteroids[j].active) continue;
                        if (Vector_DistanceSq((Vec2){rx, ry}, s->asteroids[j].pos) < powf(s->asteroids[j].radius + u->stats->radius + RESPAWN_BUFFER, 2)) { safe = false; break; }
                    }
                    if (safe) { u->pos = (Vec2){rx, ry}; break; }
                    attempts++;
                }
                s->camera_pos.x = u->pos.x - (win_w / 2.0f) / s->zoom;
                s->camera_pos.y = u->pos.y - (win_h / 2.0f) / s->zoom;
                break;
            }
        }
    }
}

static void UpdateSpawning(AppState *s, Vec2 cam_center) {
    float local_density = GetAsteroidDensity(cam_center);
    int total_target_count = (int)(local_density * MAX_DYNAMIC_ASTEROIDS);
    if (total_target_count > 200) total_target_count = 200; 

    for (int i = 0; i < MAX_ASTEROIDS; i++) {
        if (!s->asteroids[i].active) continue;
        s->asteroids[i].targeted = false;
        bool in_range = false;
        for (int a = 0; a < s->sim_anchor_count; a++) {
            if (Vector_DistanceSq(s->asteroids[i].pos, s->sim_anchors[a].pos) < DESPAWN_RANGE * DESPAWN_RANGE) { in_range = true; break; }
        }
        if (!in_range) { s->asteroids[i].active = false; s->asteroid_count--; }
    }

    int attempts = 0;
    while (s->asteroid_count < total_target_count && attempts < SPAWN_ATTEMPTS) {
        attempts++;
        Vec2 target_center = s->sim_anchors[rand() % s->sim_anchor_count].pos;
        float angle = (float)(rand() % 360) * 0.0174533f;
        float dist = SPAWN_MIN_DIST + (float)(rand() % (int)(DESPAWN_RANGE * 0.8f - SPAWN_MIN_DIST));
        Vec2 spawn_pos = {target_center.x + cosf(angle) * dist, target_center.y + sinf(angle) * dist};
        
        if (((float)rand() / (float)RAND_MAX) < GetAsteroidDensity(spawn_pos)) {
            float new_rad = ASTEROID_BASE_RADIUS_MIN + (rand() % (int)ASTEROID_BASE_RADIUS_VARIANCE);
            bool overlap = false;
            for (int j = 0; j < MAX_ASTEROIDS; j++) {
                if (s->asteroids[j].active && Vector_DistanceSq(s->asteroids[j].pos, spawn_pos) < powf((s->asteroids[j].radius + new_rad) * ASTEROID_HITBOX_MULT + SPAWN_BUFFER, 2)) { overlap = true; break; }
            }
            if (!overlap) {
                float move_angle = (float)(rand() % 360) * 0.0174533f;
                SpawnAsteroid(s, spawn_pos, (Vec2){cosf(move_angle), sinf(move_angle)}, new_rad);
            }
        }
    }
}

static void UpdateRadar(AppState *s) {
    Vec2 m_pos = {0, 0}; bool found = false;
    for (int i = 0; i < MAX_UNITS; i++) if (s->units[i].active && s->units[i].type == UNIT_MOTHERSHIP) { m_pos = s->units[i].pos; found = true; break; }
    if (found && SDL_GetAtomicInt(&s->radar_request_update) == 0) {
        SDL_LockMutex(s->radar_mutex); s->radar_mothership_pos = m_pos; SDL_UnlockMutex(s->radar_mutex);
        SDL_SetAtomicInt(&s->radar_request_update, 1);
    }
}

void Game_Update(AppState *s, float dt) {
  if (s->state == STATE_PAUSED) return;
  int win_w, win_h; SDL_GetRenderOutputSize(s->renderer, &win_w, &win_h);

  HandleRespawn(s, dt, win_w, win_h);
  if (s->hold_flash_timer > 0) s->hold_flash_timer -= dt;
  if (s->auto_attack_flash_timer > 0) s->auto_attack_flash_timer -= dt;
  if (s->ui_error_timer > 0) s->ui_error_timer -= dt;

  Vec2 cam_center = {s->camera_pos.x + (win_w / 2.0f) / s->zoom, s->camera_pos.y + (win_h / 2.0f) / s->zoom};
  float move_speed = CAMERA_SPEED / s->zoom;
  if (s->mouse_pos.x < EDGE_SCROLL_THRESHOLD) s->camera_pos.x -= move_speed * dt;
  if (s->mouse_pos.x > (win_w - EDGE_SCROLL_THRESHOLD)) s->camera_pos.x += move_speed * dt;
  if (s->mouse_pos.y < EDGE_SCROLL_THRESHOLD) s->camera_pos.y -= move_speed * dt;
  if (s->mouse_pos.y > (win_h - EDGE_SCROLL_THRESHOLD)) s->camera_pos.y += move_speed * dt;
  
  UpdateSimAnchors(s, cam_center);
  s->energy = fminf(INITIAL_ENERGY, s->energy + ENERGY_REGEN_RATE * dt);
  UpdateRadar(s);

  // Update Mouse Over Asteroid
  float wx = s->camera_pos.x + s->mouse_pos.x / s->zoom;
  float wy = s->camera_pos.y + s->mouse_pos.y / s->zoom;
  s->hover_asteroid_idx = -1;
  for (int a = 0; a < MAX_ASTEROIDS; a++) {
      if (!s->asteroids[a].active) continue;
      float dx = s->asteroids[a].pos.x - wx, dy = s->asteroids[a].pos.y - wy;
      if (dx*dx + dy*dy < s->asteroids[a].radius * s->asteroids[a].radius) {
          s->hover_asteroid_idx = a;
          break;
      }
  }

  UpdateSpawning(s, cam_center);
  
  Physics_UpdateAsteroids(s, dt);
  Physics_HandleCollisions(s, dt);

  for (int i = 0; i < MAX_UNITS; i++) {
      if (!s->units[i].active) continue;
      Unit *u = &s->units[i];
      if (u->has_target) {
          Command *cur_cmd = &u->command_queue[u->command_current_idx];
          
          // Auto-advance if target-based command target is dead
          if (cur_cmd->type == CMD_ATTACK_MOVE && cur_cmd->target_idx != -1 && !s->asteroids[cur_cmd->target_idx].active) {
              u->command_current_idx++;
              if (u->command_current_idx >= u->command_count) {
                  u->has_target = false; u->command_count = 0; u->command_current_idx = 0;
              }
              // Skip movement update this frame to re-evaluate next cmd in next frame
              continue; 
          }

          if (u->has_target) {
              float dsq = Vector_DistanceSq(cur_cmd->pos, u->pos);
              if (dsq > UNIT_STOP_DIST * UNIT_STOP_DIST) { 
                  float dist = sqrtf(dsq), speed = u->stats->speed;
                  if (!s->patrol_mode && (u->command_current_idx == u->command_count - 1) && dist < UNIT_BRAKING_DIST) speed *= (dist / UNIT_BRAKING_DIST);
                  Vec2 target_v = Vector_Scale(Vector_Normalize(Vector_Sub(cur_cmd->pos, u->pos)), speed);
                  u->velocity.x += (target_v.x - u->velocity.x) * UNIT_STEERING_FORCE * dt;
                  u->velocity.y += (target_v.y - u->velocity.y) * UNIT_STEERING_FORCE * dt;
              } else {
                  // Reached waypoint
                  if (cur_cmd->type == CMD_PATROL) {
                      // If we are at the last point, loop back to the FIRST consecutive patrol point
                      if (u->command_current_idx == u->command_count - 1) {
                          int first_patrol = u->command_current_idx;
                          while (first_patrol > 0 && u->command_queue[first_patrol - 1].type == CMD_PATROL) {
                              first_patrol--;
                          }
                          u->command_current_idx = first_patrol;
                      } else {
                          u->command_current_idx++;
                      }
                  } else {
                      u->command_current_idx++;
                      if (u->command_current_idx >= u->command_count) {
                          u->has_target = false;
                          u->command_count = 0;
                          u->command_current_idx = 0;
                      }
                  }
              }
          }
      }
      u->velocity = Vector_Sub(u->velocity, Vector_Scale(u->velocity, u->stats->friction * dt));
      u->pos = Vector_Add(u->pos, Vector_Scale(u->velocity, dt));
      
      // Rotate to face movement direction
      float speed_sq = u->velocity.x * u->velocity.x + u->velocity.y * u->velocity.y;
      if (speed_sq > 100.0f) {
          float target_rot = atan2f(u->velocity.y, u->velocity.x) * (180.0f / SDL_PI_F) + 90.0f; // +90 because sprite is front-up
          
          // Smooth rotation (LerpAngle equivalent)
          float diff = target_rot - u->rotation;
          while (diff < -180.0f) diff += 360.0f;
          while (diff > 180.0f) diff -= 360.0f;
          u->rotation += diff * UNIT_STEERING_FORCE * dt;
      }

      if (u->large_cannon_cooldown > 0) u->large_cannon_cooldown -= dt;
      for (int c = 0; c < 4; c++) if (u->small_cannon_cooldown[c] > 0) u->small_cannon_cooldown[c] -= dt;
      
      // 1. Manual Main Cannon (Active Ability)
      SDL_LockMutex(s->unit_fx_mutex);
      int l_target = u->large_target_idx;
      SDL_UnlockMutex(s->unit_fx_mutex);

                          if (l_target != -1 && s->asteroids[l_target].active) {

                              s->asteroids[l_target].targeted = true;

                              float dsq = Vector_DistanceSq(s->asteroids[l_target].pos, u->pos);

                              float max_d = u->stats->main_cannon_range + s->asteroids[l_target].radius * ASTEROID_HITBOX_MULT;

                              if (dsq <= max_d * max_d) {

                                  if (u->large_cannon_cooldown <= 0) {

                                      FireWeapon(s, u, l_target, u->stats->main_cannon_damage, 0.0f, true);

                                      u->large_cannon_cooldown = u->stats->main_cannon_cooldown;

                                      u->large_target_idx = -1; 

                                  }

                              } else {

                                  u->large_target_idx = -1;

                              }

                          }

                

                          if (s->auto_attack_enabled) {

                              SDL_LockMutex(s->unit_fx_mutex);

                              int s_targets[4]; for(int c=0; c<4; c++) s_targets[c] = u->small_target_idx[c];

                              SDL_UnlockMutex(s->unit_fx_mutex);

                

                              for (int c = 0; c < 4; c++) if (s_targets[c] != -1 && s->asteroids[s_targets[c]].active) {

                                  s->asteroids[s_targets[c]].targeted = true;

                                  float dsq = Vector_DistanceSq(s->asteroids[s_targets[c]].pos, u->pos);

                                  float max_d = u->stats->small_cannon_range + s->asteroids[s_targets[c]].radius * ASTEROID_HITBOX_MULT;

                                  if (u->small_cannon_cooldown[c] <= 0 && dsq <= max_d * max_d) {
                  FireWeapon(s, u, s_targets[c], u->stats->small_cannon_damage, u->stats->small_cannon_energy_cost, false);
                  u->small_cannon_cooldown[c] = u->stats->small_cannon_cooldown;
              }
          }
      }
  }
  
  Particles_Update(s, dt);
}