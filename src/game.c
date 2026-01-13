#include "game.h"
#include "constants.h"
#include <math.h>
#include <stdlib.h>

static void SpawnAsteroid(AppState *s, Vec2 pos, Vec2 vel_dir, float radius) {
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

static void SpawnExplosion(AppState *s, Vec2 pos, int count, float size_mult) {
  // Cap scaling so big asteroids don't fill the whole screen with one puff
  float capped_mult = fminf(1.2f, size_mult);
  
  int puff_count = count / EXPLOSION_PUFF_DIVISOR;
  for (int i = 0; i < puff_count; i++) {
    int idx = s->particle_next_idx;
    s->particles[idx].active = true;
    s->particles[idx].type = PARTICLE_PUFF;
    s->particles[idx].pos = pos;
    float angle = (float)(rand() % 360) * 0.0174533f;
    float speed = (float)(rand() % 100 + 20) * capped_mult;
    s->particles[idx].velocity.x = cosf(angle) * speed;
    s->particles[idx].velocity.y = sinf(angle) * speed;
    s->particles[idx].life = PARTICLE_LIFE_BASE;
    s->particles[idx].size = (float)(rand() % 100 + 50) * capped_mult;
    Uint8 v = (Uint8)(rand() % 50 + 100);
    s->particles[idx].color =
        (SDL_Color){v, (Uint8)(v * 0.9f), (Uint8)(v * 0.8f), 255};
    s->particle_next_idx = (s->particle_next_idx + 1) % MAX_PARTICLES;
  }

  // Debris fragments
  int debris_count = (int)(12 * capped_mult);
  for (int i = 0; i < debris_count; i++) {
      int idx = s->particle_next_idx;
      s->particles[idx].active = true;
      s->particles[idx].type = PARTICLE_DEBRIS;
      s->particles[idx].pos = pos;
      s->particles[idx].tex_idx = rand() % DEBRIS_COUNT;
      float angle = (float)(rand() % 360) * 0.0174533f;
      float speed = (float)(rand() % 300 + 100) * capped_mult;
      s->particles[idx].velocity.x = cosf(angle) * speed;
      s->particles[idx].velocity.y = sinf(angle) * speed;
      s->particles[idx].life = PARTICLE_LIFE_BASE * (0.8f + (float)rand()/(float)RAND_MAX);
      s->particles[idx].size = (float)(rand() % 40 + 20) * capped_mult;
      s->particles[idx].rotation = (float)(rand() % 360);
      s->particle_next_idx = (s->particle_next_idx + 1) % MAX_PARTICLES;
  }

  for (int i = 0; i < count; i++) {
    int idx = s->particle_next_idx;
    s->particles[idx].active = true;
    s->particles[idx].type = PARTICLE_SPARK;
    s->particles[idx].pos = pos;
    float angle = (float)(rand() % 360) * 0.0174533f;
    float speed = (float)(rand() % 300 + 100) * capped_mult;
    s->particles[idx].velocity.x = cosf(angle) * speed;
    s->particles[idx].velocity.y = sinf(angle) * speed;
    s->particles[idx].life = PARTICLE_LIFE_BASE;
    s->particles[idx].size = (float)(rand() % 6 + 2) * capped_mult;
    if (rand() % 2 == 0) {
      s->particles[idx].color =
          (SDL_Color){255, (Uint8)(rand() % 100 + 150), 50, 255};
    } else {
      s->particles[idx].color =
          (SDL_Color){(Uint8)(rand() % 50 + 150), (Uint8)(rand() % 50 + 150),
                      (Uint8)(rand() % 50 + 150), 255};
    }
    s->particle_next_idx = (s->particle_next_idx + 1) % MAX_PARTICLES;
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
    s->unit_count = 0;
    for (int i = 0; i < MAX_UNITS; i++) s->units[i].active = false;
    s->energy = INITIAL_ENERGY;
    int idx = s->unit_count++;
    Unit *u = &s->units[idx];
    u->active = true;
    u->type = UNIT_MOTHERSHIP;
    u->pos = (Vec2){0, 0};
    u->velocity = (Vec2){0, 0};
    u->rotation = 0;
    u->max_health = 1000.0f;
    u->health = 1000.0f;
    u->max_energy = 500.0f;
    u->energy = 500.0f;
    u->max_main_cannon_energy = MAIN_CANNON_MAX_ENERGY;
    u->main_cannon_energy = MAIN_CANNON_MAX_ENERGY;
    u->radius = MOTHERSHIP_RADIUS;
    u->command_count = 0;
    u->has_target = false;
    u->large_target_idx = -1;
    for(int c=0; c<4; c++) u->small_target_idx[c] = -1;
    s->selected_unit_idx = 0;
    s->unit_selected[0] = true;
}

static void FireWeapon(AppState *s, Unit *u, int asteroid_idx, float damage, float energy_cost, bool is_main_cannon) {
    if (is_main_cannon) {
        if (u->main_cannon_energy < energy_cost) return;
        u->main_cannon_energy -= energy_cost;
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
        SpawnExplosion(s, a->pos, 40, a->max_health / 1000.0f);
    }
    // Start position offset to Mothership radius in target direction
    float dx = a->pos.x - u->pos.x;
    float dy = a->pos.y - u->pos.y;
    float dist = sqrtf(dx * dx + dy * dy);
    Vec2 start_pos = u->pos;
    Vec2 impact_pos = a->pos;

    if (dist > 0.1f) {
        float unit_r = u->radius;
        float ast_r = a->radius * 0.2f; // Hit deep inside to avoid gap
        start_pos.x += (dx / dist) * unit_r;
        start_pos.y += (dy / dist) * unit_r;
        
        if (dist > ast_r) {
            impact_pos.x = u->pos.x + (dx / dist) * (dist - ast_r);
            impact_pos.y = u->pos.y + (dy / dist) * (dist - ast_r);
        }
    }

    // Impact effects
    SpawnExplosion(s, impact_pos, 8, 0.2f + (damage / 1000.0f));

    int p_idx = s->particle_next_idx;
    s->particles[p_idx].active = true;
    s->particles[p_idx].type = PARTICLE_TRACER;
    s->particles[p_idx].pos = start_pos;
    s->particles[p_idx].target_pos = impact_pos;
    s->particles[p_idx].life = 1.0f; 
    s->particles[p_idx].size = 1.0f + (damage / 50.0f);
    s->particles[p_idx].color = (SDL_Color){255, 255, 50, 255}; // Yellow
    s->particle_next_idx = (s->particle_next_idx + 1) % MAX_PARTICLES;
}

void Game_Update(AppState *s, float dt) {
  if (s->state == STATE_PAUSED) return;
  int win_w, win_h;
  SDL_GetRenderOutputSize(s->renderer, &win_w, &win_h);

  if (s->respawn_timer > 0) {
      s->respawn_timer -= dt;
      // Keep camera locked on death spot
      s->camera_pos.x = s->respawn_pos.x - (win_w / 2.0f) / s->zoom;
      s->camera_pos.y = s->respawn_pos.y - (win_h / 2.0f) / s->zoom;

      if (s->respawn_timer <= 0) {
          // Finalize respawn
          for (int i = 0; i < MAX_UNITS; i++) {
              if (s->units[i].type == UNIT_MOTHERSHIP) {
                  Unit *u = &s->units[i];
                  u->active = true;
                  u->health = u->max_health;
                  u->energy = u->max_energy;
                  u->main_cannon_energy = u->max_main_cannon_energy;
                  u->velocity = (Vec2){0, 0};
                  u->command_count = 0;
                  u->command_current_idx = 0;
                  u->has_target = false;

                  // Find a safe spot
                  int respawn_attempts = 0;
                  while (respawn_attempts < 10) {
                      float rx = (float)(rand() % 10000 - 5000);
                      float ry = (float)(rand() % 10000 - 5000);
                      bool spot_safe = true;
                      for (int j = 0; j < MAX_ASTEROIDS; j++) {
                          if (!s->asteroids[j].active) continue;
                          float adx = s->asteroids[j].pos.x - rx, ady = s->asteroids[j].pos.y - ry;
                          float adsq = adx * adx + ady * ady, amd = s->asteroids[j].radius + u->radius + 500.0f;
                          if (adsq < amd * amd) { spot_safe = false; break; }
                      }
                      if (spot_safe) { u->pos.x = rx; u->pos.y = ry; break; }
                      respawn_attempts++;
                  }
                  if (respawn_attempts == 10) { u->pos.x = (float)(rand() % 5000 - 2500); u->pos.y = (float)(rand() % 5000 - 2500); }

                  s->camera_pos.x = u->pos.x - (win_w / 2.0f) / s->zoom;
                  s->camera_pos.y = u->pos.y - (win_h / 2.0f) / s->zoom;
                  break;
              }
          }
      }
  }
  
  if (s->hold_flash_timer > 0) s->hold_flash_timer -= dt;

  Vec2 cam_center = {s->camera_pos.x + (win_w / 2.0f) / s->zoom, s->camera_pos.y + (win_h / 2.0f) / s->zoom};
  float move_speed = CAMERA_SPEED / s->zoom;
  if (s->mouse_pos.x < EDGE_SCROLL_THRESHOLD) s->camera_pos.x -= move_speed * dt;
  if (s->mouse_pos.x > (win_w - EDGE_SCROLL_THRESHOLD)) s->camera_pos.x += move_speed * dt;
  if (s->mouse_pos.y < EDGE_SCROLL_THRESHOLD) s->camera_pos.y -= move_speed * dt;
  if (s->mouse_pos.y > (win_h - EDGE_SCROLL_THRESHOLD)) s->camera_pos.y += move_speed * dt;
  UpdateSimAnchors(s, cam_center);
  s->energy += ENERGY_REGEN_RATE * dt;
  if (s->energy > INITIAL_ENERGY) s->energy = INITIAL_ENERGY;
  float local_density = GetAsteroidDensity(cam_center);
  int dynamic_target_count = (int)(MIN_DYNAMIC_ASTEROIDS + local_density * (MAX_DYNAMIC_ASTEROIDS - MIN_DYNAMIC_ASTEROIDS));
  int total_target_count = dynamic_target_count * s->sim_anchor_count;
  if (total_target_count > MAX_ASTEROIDS - 100) total_target_count = MAX_ASTEROIDS - 100;
  for (int i = 0; i < MAX_ASTEROIDS; i++) {
    if (!s->asteroids[i].active) continue;
    s->asteroids[i].targeted = false;
    bool in_range = false;
    for (int a = 0; a < s->sim_anchor_count; a++) {
      float dx = s->asteroids[i].pos.x - s->sim_anchors[a].pos.x, dy = s->asteroids[i].pos.y - s->sim_anchors[a].pos.y;
      if (dx * dx + dy * dy < DESPAWN_RANGE * DESPAWN_RANGE) { in_range = true; break; }
    }
    if (!in_range) { s->asteroids[i].active = false; s->asteroid_count--; }
  }
  int attempts = 0;
  while (s->asteroid_count < total_target_count && attempts < 100) {
    attempts++;
    int anchor_idx = rand() % s->sim_anchor_count;
    Vec2 target_center = s->sim_anchors[anchor_idx].pos;
    float angle = (float)(rand() % 360) * 0.0174533f;
    float min_spawn_dist = 3000.0f;
    float dist = min_spawn_dist + (float)(rand() % (int)(DESPAWN_RANGE * 0.8f - min_spawn_dist));
    Vec2 spawn_pos = {target_center.x + cosf(angle) * dist, target_center.y + sinf(angle) * dist};
    float p_density = GetAsteroidDensity(spawn_pos);
    if (((float)rand() / (float)RAND_MAX) < p_density) {
      float new_rad = ASTEROID_BASE_RADIUS_MIN + (rand() % (int)ASTEROID_BASE_RADIUS_VARIANCE);
      bool overlap = false;
      for (int j = 0; j < MAX_ASTEROIDS; j++) {
        if (!s->asteroids[j].active) continue;
        float dx = s->asteroids[j].pos.x - spawn_pos.x, dy = s->asteroids[j].pos.y - spawn_pos.y;
        float dist_sq = dx * dx + dy * dy, min_dist = (s->asteroids[j].radius + new_rad) * 0.3f + 200.0f;
        if (dist_sq < min_dist * min_dist) { overlap = true; break; }
      }
      if (!overlap) {
        float move_angle = (float)(rand() % 360) * 0.0174533f;
        SpawnAsteroid(s, spawn_pos, (Vec2){cosf(move_angle), sinf(move_angle)}, new_rad);
      }
    }
  }
  for (int i = 0; i < MAX_ASTEROIDS; i++) {
    if (!s->asteroids[i].active) continue;
    
    // Apply Orbital Gravity from Celestial Bodies
    int gx = (int)floorf(s->asteroids[i].pos.x / CELESTIAL_GRID_SIZE_F);
    int gy = (int)floorf(s->asteroids[i].pos.y / CELESTIAL_GRID_SIZE_F);
    for (int oy = -1; oy <= 1; oy++) {
        for (int ox = -1; ox <= 1; ox++) {
            Vec2 b_pos; float b_type, b_rad;
            if (GetCelestialBodyInfo(gx + ox, gy + oy, &b_pos, &b_type, &b_rad)) {
                float dx = b_pos.x - s->asteroids[i].pos.x, dy = b_pos.y - s->asteroids[i].pos.y;
                float dsq = dx * dx + dy * dy;
                if (dsq > 100.0f) {
                    float dist = sqrtf(dsq);
                    float force = (b_type > 0.95f ? 50000000.0f : 10000000.0f) / dsq;
                    if (force > 2000.0f) force = 2000.0f; // Cap force
                    s->asteroids[i].velocity.x += (dx / dist) * force * dt;
                    s->asteroids[i].velocity.y += (dy / dist) * force * dt;
                }
            }
        }
    }

    s->asteroids[i].pos.x += s->asteroids[i].velocity.x * dt;
    s->asteroids[i].pos.y += s->asteroids[i].velocity.y * dt;
    s->asteroids[i].rotation += s->asteroids[i].rot_speed * dt;
  }
  for (int i = 0; i < MAX_UNITS; i++) {
      if (!s->units[i].active) continue;
      Unit *u = &s->units[i];
      bool can_attack = s->attack_mode; 
      if (u->has_target) {
          Command *cur_cmd = &u->command_queue[u->command_current_idx];
          if (cur_cmd->type == CMD_HOLD) {
              u->has_target = false; u->command_count = 0; u->command_current_idx = 0;
          }

          if (u->has_target) {
              float dx = cur_cmd->pos.x - u->pos.x, dy = cur_cmd->pos.y - u->pos.y;
              float dist = sqrtf(dx * dx + dy * dy);
              
              if (dist > 50.0f) { 
                  // Steering Arrival Behavior
                  float speed = MOTHERSHIP_SPEED;
                  float braking_dist = 800.0f;
                  
                  // Only slow down if this is the final waypoint and we aren't patrolling
                  bool is_last_waypoint = (u->command_current_idx == u->command_count - 1);
                  if (!s->patrol_mode && is_last_waypoint && dist < braking_dist) {
                      speed *= (dist / braking_dist);
                  }
                  
                  float target_vx = (dx / dist) * speed;
                  float target_vy = (dy / dist) * speed;
                  
                  // Steering force
                  u->velocity.x += (target_vx - u->velocity.x) * 4.0f * dt;
                  u->velocity.y += (target_vy - u->velocity.y) * 4.0f * dt;
              }
              else {
                  if (cur_cmd->type == CMD_PATROL) u->command_current_idx = (u->command_current_idx + 1) % u->command_count;
                  else {
                      u->command_current_idx++;
                      if (u->command_current_idx >= u->command_count) { u->has_target = false; u->command_count = 0; u->command_current_idx = 0; }
                  }
              }
          }
      }
      
      // Apply Friction/Drag (always present to stabilize)
      u->velocity.x -= u->velocity.x * MOTHERSHIP_FRICTION * dt;
      u->velocity.y -= u->velocity.y * MOTHERSHIP_FRICTION * dt;

      u->pos.x += u->velocity.x * dt; u->pos.y += u->velocity.y * dt; u->rotation += 0.1f * dt;
      
      // Recharge Main Cannon Energy
      u->main_cannon_energy += MAIN_CANNON_REGEN_RATE * dt;
      if (u->main_cannon_energy > u->max_main_cannon_energy) u->main_cannon_energy = u->max_main_cannon_energy;

      if (u->large_cannon_cooldown > 0) u->large_cannon_cooldown -= dt;
      for (int c = 0; c < 4; c++) if (u->small_cannon_cooldown[c] > 0) u->small_cannon_cooldown[c] -= dt;
      
      // Targeting now handled in background thread!
      if (can_attack) {
          SDL_LockMutex(s->unit_fx_mutex);
          int l_target = u->large_target_idx;
          int s_targets[4]; for(int c=0; c<4; c++) s_targets[c] = u->small_target_idx[c];
          SDL_UnlockMutex(s->unit_fx_mutex);

          if (l_target != -1 && s->asteroids[l_target].active) {
              s->asteroids[l_target].targeted = true;
              if (u->large_cannon_cooldown <= 0) {
                  FireWeapon(s, u, l_target, LARGE_CANNON_DAMAGE, LARGE_CANNON_ENERGY_COST, true);
                  u->large_cannon_cooldown = LARGE_CANNON_COOLDOWN;
              }
          }
          for (int c = 0; c < 4; c++) {
              if (s_targets[c] != -1 && s->asteroids[s_targets[c]].active) {
                  s->asteroids[s_targets[c]].targeted = true;
                  if (u->small_cannon_cooldown[c] <= 0) {
                      FireWeapon(s, u, s_targets[c], SMALL_CANNON_DAMAGE, SMALL_CANNON_ENERGY_COST, false);
                      u->small_cannon_cooldown[c] = SMALL_CANNON_COOLDOWN;
                  }
              }
          }
      }
  }
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (!s->particles[i].active) continue;
    if (s->particles[i].type == PARTICLE_TRACER) s->particles[i].life -= dt * 2.0f;
    else { s->particles[i].pos.x += s->particles[i].velocity.x * dt; s->particles[i].pos.y += s->particles[i].velocity.y * dt; s->particles[i].life -= dt * PARTICLE_LIFE_DECAY; }
    if (s->particles[i].life <= 0) s->particles[i].active = false;
  }
  for (int i = 0; i < MAX_ASTEROIDS; i++) {
    if (!s->asteroids[i].active) continue;
    for (int j = i + 1; j < MAX_ASTEROIDS; j++) {
      if (!s->asteroids[j].active) continue;
      float dx = s->asteroids[j].pos.x - s->asteroids[i].pos.x, dy = s->asteroids[j].pos.y - s->asteroids[i].pos.y;
      float dist_sq = dx * dx + dy * dy;
      float r1 = s->asteroids[i].radius * 0.3f, r2 = s->asteroids[j].radius * 0.3f;
      float min_dist = r1 + r2;
      if (dist_sq < min_dist * min_dist) {
        Asteroid a = s->asteroids[i], b = s->asteroids[j];
        float collision_angle = atan2f(dy, dx);
        SpawnExplosion(s, (Vec2){a.pos.x + dx * 0.5f, a.pos.y + dy * 0.5f}, 40, (a.radius + b.radius) / 100.0f);

        int small_idx = -1, big_idx = -1;
        if (a.radius < b.radius * 0.7f) { small_idx = i; big_idx = j; }
        else if (b.radius < a.radius * 0.7f) { small_idx = j; big_idx = i; }

        if (small_idx != -1) {
            // Smaller one splits and is destroyed
            Asteroid sm = s->asteroids[small_idx];
            s->asteroids[small_idx].active = false; s->asteroid_count--;
            
            // Bigger one gets a bit smaller
            s->asteroids[big_idx].radius *= 0.9f;
            s->asteroids[big_idx].health *= 0.9f;
            
            // Spawn chip from smaller one (half size)
            float chip_rad = sm.radius * 0.5f;
            if (chip_rad >= ASTEROID_MIN_RADIUS) {
                float sa = collision_angle + (small_idx == i ? 3.14159f : 0.0f) + 0.8f;
                SpawnAsteroid(s, (Vec2){sm.pos.x + cosf(sa) * chip_rad, sm.pos.y + sinf(sa) * chip_rad}, (Vec2){cosf(sa), sinf(sa)}, chip_rad);
            }

            // Normal split for smaller one if large enough
            if (sm.radius > ASTEROID_COLLISION_SPLIT_THRESHOLD) {
                float child_rad = ASTEROID_SPLIT_MIN_RADIUS * powf(sm.radius / ASTEROID_SPLIT_MIN_RADIUS, ASTEROID_SPLIT_EXPONENT);
                if (child_rad >= ASTEROID_MIN_RADIUS) {
                    for (int k = 0; k < 2; k++) {
                        float ssa = collision_angle + (small_idx == i ? 3.14159f : 0.0f) + 1.5708f + (k == 0 ? 0.4f : -0.4f);
                        SpawnAsteroid(s, (Vec2){sm.pos.x + cosf(ssa) * child_rad, sm.pos.y + sinf(ssa) * child_rad}, (Vec2){cosf(ssa), sinf(ssa)}, child_rad);
                    }
                }
            }
        } else {
            // Both are similar size: both destroyed
            s->asteroids[i].active = false; s->asteroids[j].active = false; s->asteroid_count -= 2;
            
            // Split asteroid A
            if (a.radius > ASTEROID_COLLISION_SPLIT_THRESHOLD) {
              float child_rad = ASTEROID_SPLIT_MIN_RADIUS * powf(a.radius / ASTEROID_SPLIT_MIN_RADIUS, ASTEROID_SPLIT_EXPONENT);
              if (child_rad >= ASTEROID_MIN_RADIUS) {
                  for (int k = 0; k < 2; k++) {
                    float sa = collision_angle + 1.5708f + (k == 0 ? 0.4f : -0.4f);
                    SpawnAsteroid(s, (Vec2){a.pos.x + cosf(sa) * child_rad, a.pos.y + sinf(sa) * child_rad}, (Vec2){cosf(sa), sinf(sa)}, child_rad);
                  }
              }
            }
            // Split asteroid B
            if (b.radius > ASTEROID_COLLISION_SPLIT_THRESHOLD) {
              float child_rad = ASTEROID_SPLIT_MIN_RADIUS * powf(b.radius / ASTEROID_SPLIT_MIN_RADIUS, ASTEROID_SPLIT_EXPONENT);
              if (child_rad >= ASTEROID_MIN_RADIUS) {
                  for (int k = 0; k < 2; k++) {
                    float sa = collision_angle + 3.14159f + 1.5708f + (k == 0 ? 0.4f : -0.4f);
                    SpawnAsteroid(s, (Vec2){b.pos.x + cosf(sa) * child_rad, b.pos.y + sinf(sa) * child_rad}, (Vec2){cosf(sa), sinf(sa)}, child_rad);
                  }
              }
            }
        }
        break;
      }
    }
    for (int u = 0; u < MAX_UNITS; u++) {
        if (!s->units[u].active) continue;
        float dx = s->units[u].pos.x - s->asteroids[i].pos.x, dy = s->units[u].pos.y - s->asteroids[i].pos.y;
        float dsq = dx * dx + dy * dy, md = s->asteroids[i].radius * 0.3f + s->units[u].radius;
        if (dsq < md * md) {
            s->units[u].health -= s->asteroids[i].radius * 0.5f;
            SpawnExplosion(s, s->asteroids[i].pos, 20, s->asteroids[i].radius / 100.0f);
            s->asteroids[i].active = false; s->asteroid_count--;
            if (s->units[u].health <= 0) {
                SpawnExplosion(s, s->units[u].pos, 100, 2.0f);
                if (s->units[u].type == UNIT_MOTHERSHIP) {
                    s->units[u].active = false;
                    s->respawn_timer = 3.0f;
                    s->respawn_pos = s->units[u].pos;
                    
                    // Focus camera on explosion
                    int ww, wh; SDL_GetRenderOutputSize(s->renderer, &ww, &wh);
                    s->camera_pos.x = s->respawn_pos.x - (ww / 2.0f) / s->zoom;
                    s->camera_pos.y = s->respawn_pos.y - (wh / 2.0f) / s->zoom;
                } else {
                    s->units[u].active = false;
                }
            }
            break;
        }
    }
  }
}