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

static void SpawnExplosion(AppState *s, Vec2 pos, int count, float size_mult, ExplosionType type, int asteroid_tex_idx) {
  // Balanced scaling
  float capped_mult = powf(size_mult, 0.6f); 
  float count_mult = powf(size_mult, 0.35f);   // Fewer pieces for large asteroids
  float chunky_mult = powf(size_mult, 0.75f);  // Reduced from 0.95f
  
  // Theme color lookup - Even darker for a solid rocky look
  float a_theme = DeterministicHash(asteroid_tex_idx * 789, 123);
  SDL_Color base_col;
  if (a_theme > 0.7f) base_col = (SDL_Color){30, 18, 12, 255};      // Deep Red
  else if (a_theme > 0.4f) base_col = (SDL_Color){15, 20, 35, 255}; // Deep Blue
  else base_col = (SDL_Color){18, 18, 22, 255};                      // Deep Grey

  if (type == EXPLOSION_IMPACT) {
      // Fast, sharp sparks
      int spark_count = (int)(count * 2.5f * count_mult); 
      for (int i = 0; i < spark_count; i++) {
        int idx = s->particle_next_idx;
        s->particles[idx].active = true;
        s->particles[idx].type = PARTICLE_SPARK;
        s->particles[idx].pos = pos;
        float angle = (float)(rand() % 360) * 0.0174533f;
        float speed = (float)(rand() % 800 + 400) * capped_mult; 
        s->particles[idx].velocity.x = cosf(angle) * speed;
        s->particles[idx].velocity.y = sinf(angle) * speed;
        s->particles[idx].life = PARTICLE_LIFE_BASE * 0.5f;
        s->particles[idx].size = (float)(rand() % 12 + 6) * capped_mult; // Increased from 8+4
        s->particles[idx].color = (SDL_Color){(Uint8)((base_col.r + 255)/2), (Uint8)((base_col.g + 220)/2), (Uint8)((base_col.b + 150)/2), 255};
        s->particle_next_idx = (s->particle_next_idx + 1) % MAX_PARTICLES;
      }
      // Small puffs
      int puff_count = (int)(count * 1.2f * count_mult); 
      for (int i = 0; i < puff_count; i++) {
        int idx = s->particle_next_idx;
        s->particles[idx].active = true;
        s->particles[idx].type = PARTICLE_PUFF;
        s->particles[idx].pos = pos;
        float angle = (float)(rand() % 360) * 0.0174533f;
        float speed = (float)(rand() % 200 + 80) * capped_mult; 
        s->particles[idx].velocity.x = cosf(angle) * speed;
        s->particles[idx].velocity.y = sinf(angle) * speed;
        s->particles[idx].life = PARTICLE_LIFE_BASE * 0.5f; 
        s->particles[idx].size = (float)(rand() % 120 + 60) * capped_mult; // Increased from 80+40
        Uint8 v = (Uint8)(rand() % 40 + 80); 
        s->particles[idx].color = (SDL_Color){v, (Uint8)(v * 0.8f), (Uint8)(v * 0.6f), 255};
        s->particle_next_idx = (s->particle_next_idx + 1) % MAX_PARTICLES;
      }
      
      // Fine Debris
      int fine_debris_count = (int)(10 * count_mult); 
      for (int i = 0; i < fine_debris_count; i++) {
          int idx = s->particle_next_idx;
          s->particles[idx].active = true;
          s->particles[idx].type = PARTICLE_DEBRIS;
          s->particles[idx].asteroid_tex_idx = asteroid_tex_idx;
          s->particles[idx].pos = pos;
          s->particles[idx].tex_idx = rand() % DEBRIS_COUNT;
          float angle = (float)(rand() % 360) * 0.0174533f;
          float speed = (float)(rand() % 300 + 100) * capped_mult; 
          s->particles[idx].velocity.x = cosf(angle) * speed;
          s->particles[idx].velocity.y = sinf(angle) * speed;
          s->particles[idx].life = PARTICLE_LIFE_BASE * 0.35f; 
          s->particles[idx].size = (float)(rand() % 18 + 12) * chunky_mult; // Increased from 12+8
          s->particles[idx].rotation = (float)(rand() % 360);
          s->particles[idx].color = base_col;
          s->particle_next_idx = (s->particle_next_idx + 1) % MAX_PARTICLES;
      }

      // Main Debris
      int debris_count = (int)(3 * count_mult); 
      for (int i = 0; i < debris_count; i++) {
          int idx = s->particle_next_idx;
          s->particles[idx].active = true;
          s->particles[idx].type = PARTICLE_DEBRIS;
          s->particles[idx].asteroid_tex_idx = asteroid_tex_idx;
          s->particles[idx].pos = pos;
          s->particles[idx].tex_idx = rand() % DEBRIS_COUNT;
          float angle = (float)(rand() % 360) * 0.0174533f;
          float speed = (float)(rand() % 150 + 60) * capped_mult; 
          s->particles[idx].velocity.x = cosf(angle) * speed;
          s->particles[idx].velocity.y = sinf(angle) * speed;
          s->particles[idx].life = PARTICLE_LIFE_BASE * 0.45f; 
          s->particles[idx].size = (float)(rand() % 45 + 35) * chunky_mult; // Increased from 30+20
          s->particles[idx].rotation = (float)(rand() % 360);
          s->particles[idx].color = base_col;
          s->particle_next_idx = (s->particle_next_idx + 1) % MAX_PARTICLES;
      }

      // Energetic shockwave for impact
      int sw_idx = s->particle_next_idx;
      s->particles[sw_idx].active = true;
      s->particles[sw_idx].type = PARTICLE_SHOCKWAVE;
      s->particles[sw_idx].pos = pos;
      s->particles[sw_idx].velocity = (Vec2){0,0};
      s->particles[sw_idx].life = 0.3f; 
      s->particles[sw_idx].size = 55.0f * capped_mult; // Increased from 40
      s->particles[sw_idx].color = (SDL_Color){255, 255, 255, 200}; 
      s->particle_next_idx = (s->particle_next_idx + 1) % MAX_PARTICLES;


  } else {
      // EXPLOSION_COLLISION
      int puff_count = (int)(count * 0.5f * count_mult); 
      for (int i = 0; i < puff_count; i++) {
        int idx = s->particle_next_idx;
        s->particles[idx].active = true;
        s->particles[idx].type = PARTICLE_PUFF;
        s->particles[idx].pos = pos;
        float angle = (float)(rand() % 360) * 0.0174533f;
        float speed = (float)(rand() % 150 + 20) * capped_mult; 
        s->particles[idx].velocity.x = cosf(angle) * speed;
        s->particles[idx].velocity.y = sinf(angle) * speed;
        s->particles[idx].life = PARTICLE_LIFE_BASE * (0.8f + 0.4f * (float)rand()/(float)RAND_MAX); // Middle ground
        s->particles[idx].size = (float)(rand() % 120 + 60) * capped_mult; // Slightly smaller
        Uint8 v = (Uint8)(rand() % 30 + 40); // Dark smoke
        s->particles[idx].color = (SDL_Color){v, (Uint8)(v * 0.95f), (Uint8)(v * 0.9f), 255};
        s->particle_next_idx = (s->particle_next_idx + 1) % MAX_PARTICLES;
      }

      int debris_count = (int)(5 * count_mult); // Reduced from 10
      for (int i = 0; i < debris_count; i++) {
          int idx = s->particle_next_idx;
          s->particles[idx].active = true;
          s->particles[idx].type = PARTICLE_DEBRIS;
          s->particles[idx].asteroid_tex_idx = asteroid_tex_idx;
          s->particles[idx].pos = pos;
          s->particles[idx].tex_idx = rand() % DEBRIS_COUNT;
          float angle = (float)(rand() % 360) * 0.0174533f;
          float speed = (float)(rand() % 100 + 40) * capped_mult; 
          s->particles[idx].velocity.x = cosf(angle) * speed;
          s->particles[idx].velocity.y = sinf(angle) * speed;
          s->particles[idx].life = PARTICLE_LIFE_BASE * 0.7f; 
          s->particles[idx].size = (float)(rand() % 40 + 20) * chunky_mult; // Toned down
          s->particles[idx].rotation = (float)(rand() % 360);
          s->particles[idx].color = base_col;
          s->particle_next_idx = (s->particle_next_idx + 1) % MAX_PARTICLES;
      }
      
      // Shockwave (Toned down size)
      int sw_idx = s->particle_next_idx;
      s->particles[sw_idx].active = true;
      s->particles[sw_idx].type = PARTICLE_SHOCKWAVE;
      s->particles[sw_idx].pos = pos;
      s->particles[sw_idx].velocity = (Vec2){0,0};
      s->particles[sw_idx].life = 0.5f;
      // Reduced initial size from 60.0f
      s->particles[sw_idx].size = (float)(40.0f * capped_mult); 
      s->particles[sw_idx].color = (SDL_Color){255, 255, 255, 100};
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
        .main_cannon_energy_cost = LARGE_CANNON_ENERGY_COST,
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
    u->main_cannon_energy = MAIN_CANNON_MAX_ENERGY;
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
        SpawnExplosion(s, a->pos, 40, a->max_health / 1000.0f, EXPLOSION_COLLISION, a->tex_idx);
    }
    // Start position offset to Mothership radius in target direction
    float dx = a->pos.x - u->pos.x;
    float dy = a->pos.y - u->pos.y;
    float dist = sqrtf(dx * dx + dy * dy);
    Vec2 start_pos = u->pos;
    Vec2 impact_pos = a->pos;

    if (dist > 0.1f) {
        float unit_r = u->stats->radius * LASER_START_OFFSET_MULT; // Offset to the edge of the organic hull
        float ast_r = a->radius * 0.32f; // Hit visual surface (matches ASTEROID_HITBOX_MULT)
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

    // Muzzle flash effect
    int m_idx = s->particle_next_idx;
    s->particles[m_idx].active = true;
    s->particles[m_idx].type = PARTICLE_GLOW; 
    s->particles[m_idx].pos = start_pos;
    s->particles[m_idx].velocity = (Vec2){0, 0};
    s->particles[m_idx].life = MUZZLE_FLASH_LIFE;
    s->particles[m_idx].size = s->particles[p_idx].size * MUZZLE_FLASH_SIZE_MULT;
    s->particles[m_idx].color = (SDL_Color)COLOR_MUZZLE_FLASH;
    s->particle_next_idx = (s->particle_next_idx + 1) % MAX_PARTICLES;

    // Small Impact Explosion (Puff + Sparks + Debris)
    float impact_scale = 0.5f + (damage / 2000.0f);
    SpawnExplosion(s, impact_pos, 15, impact_scale, EXPLOSION_IMPACT, a->tex_idx); 

    // Extra Bright Glow at impact point
    int i_g_idx = s->particle_next_idx;
    s->particles[i_g_idx].active = true;
    s->particles[i_g_idx].type = PARTICLE_GLOW;
    s->particles[i_g_idx].pos = impact_pos;
    s->particles[i_g_idx].velocity = (Vec2){0, 0};
    s->particles[i_g_idx].life = 0.25f;
    s->particles[i_g_idx].size = s->particles[p_idx].size * 8.0f;
    s->particles[i_g_idx].color = (SDL_Color){255, 220, 200, 255};
    s->particle_next_idx = (s->particle_next_idx + 1) % MAX_PARTICLES;
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
                u->main_cannon_energy = MAIN_CANNON_MAX_ENERGY;
                u->velocity = (Vec2){0, 0};
                u->command_count = 0;
                u->command_current_idx = 0;
                u->has_target = false;

                int attempts = 0;
                while (attempts < 10) {
                    float rx = (float)(rand() % 10000 - 5000), ry = (float)(rand() % 10000 - 5000);
                    bool safe = true;
                    for (int j = 0; j < MAX_ASTEROIDS; j++) {
                        if (!s->asteroids[j].active) continue;
                        if (Vector_DistanceSq((Vec2){rx, ry}, s->asteroids[j].pos) < powf(s->asteroids[j].radius + u->stats->radius + 500.0f, 2)) { safe = false; break; }
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
    while (s->asteroid_count < total_target_count && attempts < 20) {
        attempts++;
        Vec2 target_center = s->sim_anchors[rand() % s->sim_anchor_count].pos;
        float angle = (float)(rand() % 360) * 0.0174533f;
        float dist = 3000.0f + (float)(rand() % (int)(DESPAWN_RANGE * 0.8f - 3000.0f));
        Vec2 spawn_pos = {target_center.x + cosf(angle) * dist, target_center.y + sinf(angle) * dist};
        
        if (((float)rand() / (float)RAND_MAX) < GetAsteroidDensity(spawn_pos)) {
            float new_rad = ASTEROID_BASE_RADIUS_MIN + (rand() % (int)ASTEROID_BASE_RADIUS_VARIANCE);
            bool overlap = false;
            for (int j = 0; j < MAX_ASTEROIDS; j++) {
                if (s->asteroids[j].active && Vector_DistanceSq(s->asteroids[j].pos, spawn_pos) < powf((s->asteroids[j].radius + new_rad) * ASTEROID_HITBOX_MULT + 1000.0f, 2)) { overlap = true; break; }
            }
            if (!overlap) {
                float move_angle = (float)(rand() % 360) * 0.0174533f;
                SpawnAsteroid(s, spawn_pos, (Vec2){cosf(move_angle), sinf(move_angle)}, new_rad);
            }
        }
    }
}

static void UpdateAsteroidPhysics(AppState *s, float dt) {
    for (int i = 0; i < MAX_ASTEROIDS; i++) {
        if (!s->asteroids[i].active) continue;
        int gx = (int)floorf(s->asteroids[i].pos.x / CELESTIAL_GRID_SIZE_F), gy = (int)floorf(s->asteroids[i].pos.y / CELESTIAL_GRID_SIZE_F);
        for (int oy = -1; oy <= 1; oy++) for (int ox = -1; ox <= 1; ox++) {
            Vec2 b_pos; float b_type, b_rad;
            if (GetCelestialBodyInfo(gx + ox, gy + oy, &b_pos, &b_type, &b_rad)) {
                float dsq = Vector_DistanceSq(b_pos, s->asteroids[i].pos);
                if (dsq > 100.0f) {
                    float force = (b_type > 0.95f ? 50000000.0f : 10000000.0f) / dsq;
                    Vec2 dir = Vector_Normalize(Vector_Sub(b_pos, s->asteroids[i].pos));
                    s->asteroids[i].velocity = Vector_Add(s->asteroids[i].velocity, Vector_Scale(dir, force * dt));
                }
            }
        }
        s->asteroids[i].pos = Vector_Add(s->asteroids[i].pos, Vector_Scale(s->asteroids[i].velocity, dt));
        s->asteroids[i].rotation += s->asteroids[i].rot_speed * dt;
    }
}

void Game_Update(AppState *s, float dt) {
  if (s->state == STATE_PAUSED) return;
  int win_w, win_h; SDL_GetRenderOutputSize(s->renderer, &win_w, &win_h);

  HandleRespawn(s, dt, win_w, win_h);
  if (s->hold_flash_timer > 0) s->hold_flash_timer -= dt;

  // Camera & Energy
  Vec2 cam_center = {s->camera_pos.x + (win_w / 2.0f) / s->zoom, s->camera_pos.y + (win_h / 2.0f) / s->zoom};
  float move_speed = CAMERA_SPEED / s->zoom;
  if (s->mouse_pos.x < EDGE_SCROLL_THRESHOLD) s->camera_pos.x -= move_speed * dt;
  if (s->mouse_pos.x > (win_w - EDGE_SCROLL_THRESHOLD)) s->camera_pos.x += move_speed * dt;
  if (s->mouse_pos.y < EDGE_SCROLL_THRESHOLD) s->camera_pos.y -= move_speed * dt;
  if (s->mouse_pos.y > (win_h - EDGE_SCROLL_THRESHOLD)) s->camera_pos.y += move_speed * dt;
  
  UpdateSimAnchors(s, cam_center);
  s->energy = fminf(INITIAL_ENERGY, s->energy + ENERGY_REGEN_RATE * dt);

  UpdateSpawning(s, cam_center);
  UpdateAsteroidPhysics(s, dt);

  for (int i = 0; i < MAX_UNITS; i++) {
      if (!s->units[i].active) continue;
      Unit *u = &s->units[i];
      if (u->has_target) {
          Command *cur_cmd = &u->command_queue[u->command_current_idx];
          if (cur_cmd->type == CMD_HOLD) { u->has_target = false; u->command_count = 0; u->command_current_idx = 0; }
          if (u->has_target) {
              float dsq = Vector_DistanceSq(cur_cmd->pos, u->pos);
              if (dsq > UNIT_STOP_DIST * UNIT_STOP_DIST) { 
                  float dist = sqrtf(dsq), speed = u->stats->speed;
                  if (!s->patrol_mode && (u->command_current_idx == u->command_count - 1) && dist < UNIT_BRAKING_DIST) speed *= (dist / UNIT_BRAKING_DIST);
                  Vec2 target_v = Vector_Scale(Vector_Normalize(Vector_Sub(cur_cmd->pos, u->pos)), speed);
                  u->velocity.x += (target_v.x - u->velocity.x) * UNIT_STEERING_FORCE * dt;
                  u->velocity.y += (target_v.y - u->velocity.y) * UNIT_STEERING_FORCE * dt;
              } else {
                  if (cur_cmd->type == CMD_PATROL) u->command_current_idx = (u->command_current_idx + 1) % u->command_count;
                  else { u->command_current_idx++; if (u->command_current_idx >= u->command_count) { u->has_target = false; u->command_count = 0; u->command_current_idx = 0; } }
              }
          }
      }
      u->velocity = Vector_Sub(u->velocity, Vector_Scale(u->velocity, u->stats->friction * dt));
      u->pos = Vector_Add(u->pos, Vector_Scale(u->velocity, dt));
      u->rotation += UNIT_ROTATION_SPEED * dt;
      u->main_cannon_energy = fminf(MAIN_CANNON_MAX_ENERGY, u->main_cannon_energy + MAIN_CANNON_REGEN_RATE * dt);

      if (u->large_cannon_cooldown > 0) u->large_cannon_cooldown -= dt;
      for (int c = 0; c < 4; c++) if (u->small_cannon_cooldown[c] > 0) u->small_cannon_cooldown[c] -= dt;
      
      if (s->attack_mode) {
          SDL_LockMutex(s->unit_fx_mutex);
          int l_target = u->large_target_idx, s_targets[4]; for(int c=0; c<4; c++) s_targets[c] = u->small_target_idx[c];
          SDL_UnlockMutex(s->unit_fx_mutex);

          if (l_target != -1 && s->asteroids[l_target].active) {
              s->asteroids[l_target].targeted = true;
              if (u->large_cannon_cooldown <= 0 && Vector_DistanceSq(s->asteroids[l_target].pos, u->pos) <= powf(u->stats->main_cannon_range * WEAPON_FIRING_RANGE_MULT, 2)) {
                  FireWeapon(s, u, l_target, u->stats->main_cannon_damage, u->stats->main_cannon_energy_cost, true);
                  u->large_cannon_cooldown = u->stats->main_cannon_cooldown;
              }
          }
          for (int c = 0; c < 4; c++) if (s_targets[c] != -1 && s->asteroids[s_targets[c]].active) {
              s->asteroids[s_targets[c]].targeted = true;
              if (u->small_cannon_cooldown[c] <= 0 && Vector_DistanceSq(s->asteroids[s_targets[c]].pos, u->pos) <= powf(u->stats->small_cannon_range * WEAPON_FIRING_RANGE_MULT, 2)) {
                  FireWeapon(s, u, s_targets[c], u->stats->small_cannon_damage, u->stats->small_cannon_energy_cost, false);
                  u->small_cannon_cooldown[c] = u->stats->small_cannon_cooldown;
              }
          }
      }
  }
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (!s->particles[i].active) continue;
    if (s->particles[i].type == PARTICLE_TRACER) s->particles[i].life -= dt * 2.0f;
    else if (s->particles[i].type == PARTICLE_SHOCKWAVE) {
        s->particles[i].life -= dt * 2.0f;
        s->particles[i].size += dt * 1200.0f; // Slower expansion
    }
    else { s->particles[i].pos = Vector_Add(s->particles[i].pos, Vector_Scale(s->particles[i].velocity, dt)); s->particles[i].life -= dt * PARTICLE_LIFE_DECAY; }
    if (s->particles[i].life <= 0) s->particles[i].active = false;
  }
  for (int i = 0; i < MAX_ASTEROIDS; i++) {
    if (!s->asteroids[i].active) continue;
    for (int j = i + 1; j < MAX_ASTEROIDS; j++) {
      if (!s->asteroids[j].active) continue;
      float dsq = Vector_DistanceSq(s->asteroids[j].pos, s->asteroids[i].pos);
      float r1 = s->asteroids[i].radius * ASTEROID_HITBOX_MULT, r2 = s->asteroids[j].radius * ASTEROID_HITBOX_MULT;
                  if (dsq < (r1 + r2) * (r1 + r2)) {
                    Asteroid a = s->asteroids[i], b = s->asteroids[j];
                    SpawnExplosion(s, Vector_Add(a.pos, Vector_Scale(Vector_Sub(b.pos, a.pos), 0.5f)), 40, (a.radius + b.radius) / 100.0f, EXPLOSION_COLLISION, a.tex_idx);
            
                    int sm_i = -1, bg_i = -1;        if (a.radius < b.radius * 0.7f) { sm_i = i; bg_i = j; } else if (b.radius < a.radius * 0.7f) { sm_i = j; bg_i = i; }
        
        // Calculate collision normal (from A to B)
        Vec2 normal = Vector_Normalize(Vector_Sub(b.pos, a.pos));

        if (sm_i != -1) {
            Asteroid sm = s->asteroids[sm_i];
            
            // Calculate reflection for the smaller asteroid
            // R = V - 2(V.N)N
            // Normal needs to point towards the incoming asteroid (sm)
            Vec2 n_surf = (sm_i == i) ? Vector_Scale(normal, -1.0f) : normal;
            Vec2 v_inc = sm.velocity;
            if (Vector_Length(v_inc) < 10.0f) v_inc = Vector_Normalize(Vector_Sub(sm.pos, s->asteroids[bg_i].pos));
            
            float dot = v_inc.x * n_surf.x + v_inc.y * n_surf.y;
            Vec2 reflect = Vector_Sub(v_inc, Vector_Scale(n_surf, 2.0f * dot));
            float reflect_angle = atan2f(reflect.y, reflect.x);

            s->asteroids[sm_i].active = false; s->asteroid_count--;
            s->asteroids[bg_i].radius *= 0.9f; s->asteroids[bg_i].health *= 0.9f;
            
            float cr = sm.radius * 0.5f;
            if (cr >= ASTEROID_MIN_RADIUS) {
                float sa = reflect_angle + ((float)(rand() % 100) / 100.0f - 0.5f) * 0.5f;
                SpawnAsteroid(s, Vector_Add(sm.pos, (Vec2){cosf(sa)*cr, sinf(sa)*cr}), (Vec2){cosf(sa), sinf(sa)}, cr);
            }
            if (sm.radius > ASTEROID_COLLISION_SPLIT_THRESHOLD) {
                float chr = ASTEROID_SPLIT_MIN_RADIUS * powf(sm.radius / ASTEROID_SPLIT_MIN_RADIUS, ASTEROID_SPLIT_EXPONENT);
                if (chr >= ASTEROID_MIN_RADIUS) for (int k = 0; k < 2; k++) {
                    float ssa = reflect_angle + (k == 0 ? 0.3f : -0.3f) + ((float)(rand() % 100) / 100.0f - 0.5f) * 0.4f;
                    SpawnAsteroid(s, Vector_Add(sm.pos, (Vec2){cosf(ssa)*chr, sinf(ssa)*chr}), (Vec2){cosf(ssa), sinf(ssa)}, chr);
                }
            }
        } else {
            s->asteroids[i].active = false; s->asteroids[j].active = false; s->asteroid_count -= 2;
            
            // A reflects off B (Normal points A -> B, so surface normal for A is N)
            // Wait, normal is A->B. Surface of B facing A has normal -N.
            // Surface of A facing B has normal N.
            // If A hits B, A bounces off B's surface. Surface normal is -Normal (B->A).
            
            Vec2 v_a = a.velocity;
            Vec2 n_a = Vector_Scale(normal, -1.0f); // Surface of B pointing at A
            float d_a = v_a.x * n_a.x + v_a.y * n_a.y;
            Vec2 ref_a = Vector_Sub(v_a, Vector_Scale(n_a, 2.0f * d_a));
            float ang_a = atan2f(ref_a.y, ref_a.x);

            // B reflects off A (Surface normal is Normal A->B)
            Vec2 v_b = b.velocity;
            float d_b = v_b.x * normal.x + v_b.y * normal.y;
            Vec2 ref_b = Vector_Sub(v_b, Vector_Scale(normal, 2.0f * d_b));
            float ang_b = atan2f(ref_b.y, ref_b.x);

            if (a.radius > ASTEROID_COLLISION_SPLIT_THRESHOLD) {
              float chr = ASTEROID_SPLIT_MIN_RADIUS * powf(a.radius / ASTEROID_SPLIT_MIN_RADIUS, ASTEROID_SPLIT_EXPONENT);
              if (chr >= ASTEROID_MIN_RADIUS) for (int k = 0; k < 2; k++) {
                float sa = ang_a + (k == 0 ? 0.4f : -0.4f);
                SpawnAsteroid(s, Vector_Add(a.pos, (Vec2){cosf(sa)*chr, sinf(sa)*chr}), (Vec2){cosf(sa), sinf(sa)}, chr);
              }
            }
            if (b.radius > ASTEROID_COLLISION_SPLIT_THRESHOLD) {
              float chr = ASTEROID_SPLIT_MIN_RADIUS * powf(b.radius / ASTEROID_SPLIT_MIN_RADIUS, ASTEROID_SPLIT_EXPONENT);
              if (chr >= ASTEROID_MIN_RADIUS) for (int k = 0; k < 2; k++) {
                float sa = ang_b + (k == 0 ? 0.4f : -0.4f);
                SpawnAsteroid(s, Vector_Add(b.pos, (Vec2){cosf(sa)*chr, sinf(sa)*chr}), (Vec2){cosf(sa), sinf(sa)}, chr);
              }
            }
        }
        break;
      }
    }
    for (int u = 0; u < MAX_UNITS; u++) {
        if (!s->units[u].active) continue;
        float dsq = Vector_DistanceSq(s->units[u].pos, s->asteroids[i].pos), md = s->asteroids[i].radius * ASTEROID_HITBOX_MULT + s->units[u].stats->radius;
        if (dsq < md * md) {
            s->units[u].health -= s->asteroids[i].radius * ASTEROID_HITBOX_MULT;
            SpawnExplosion(s, s->asteroids[i].pos, 20, s->asteroids[i].radius / 100.0f, EXPLOSION_COLLISION, s->asteroids[i].tex_idx);
            s->asteroids[i].active = false; s->asteroid_count--;
            if (s->units[u].health <= 0) {
                SpawnExplosion(s, s->units[u].pos, 100, 2.0f, EXPLOSION_COLLISION, s->asteroids[i].tex_idx);
                if (s->units[u].type == UNIT_MOTHERSHIP) { s->units[u].active = false; s->respawn_timer = RESPAWN_TIME; s->respawn_pos = s->units[u].pos; } else s->units[u].active = false;
            }
            break;
        }
    }
  }
}