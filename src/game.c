#include "game.h"
#include "ui.h"
#include "constants.h"
#include "particles.h"
#include "physics.h"
#include "utils.h"
#include "weapons.h"
#include "abilities.h"
#include "ai.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static void UpdateRadar(AppState *s);
static void UpdateSpawning(AppState *s, Vec2 cam_center);

void SpawnAsteroid(AppState *s, Vec2 pos, Vec2 vel_dir, float radius) {
  if (s->world.asteroid_count >= MAX_ASTEROIDS)
    return;
  for (int i = 0; i < MAX_ASTEROIDS; i++) {
    if (!s->world.asteroids.active[i]) {
      s->world.asteroids.pos[i] = pos;
      float speed = ASTEROID_SPEED_FACTOR / radius;
      s->world.asteroids.velocity[i].x = vel_dir.x * speed;
      s->world.asteroids.velocity[i].y = vel_dir.y * speed;
      s->world.asteroids.radius[i] = radius;
      s->world.asteroids.rotation[i] = (float)(rand() % 360);
      s->world.asteroids.rot_speed[i] =
          ((float)(rand() % 100) / 50.0f - 1.0f) *
          (ASTEROID_ROTATION_SPEED_FACTOR / radius);
      s->world.asteroids.tex_idx[i] = rand() % ASTEROID_TYPE_COUNT;
      s->world.asteroids.active[i] = true;
      // Make smaller asteroids exponentially weaker
      float health_scale = powf(radius / 1000.0f, 1.5f) * 1000.0f;
      s->world.asteroids.max_health[i] = health_scale * ASTEROID_HEALTH_MULT * 0.1f;
      s->world.asteroids.health[i] = s->world.asteroids.max_health[i];
      s->world.asteroids.targeted[i] = false;
      s->world.asteroid_count++;
      break;
    }
  }
}

void SpawnCrystal(AppState *s, Vec2 pos, Vec2 vel_dir, float radius) {
    if (s->world.resource_count >= MAX_RESOURCES) return;
    for (int i = 0; i < MAX_RESOURCES; i++) {
        if (!s->world.resources.active[i]) {
            s->world.resources.pos[i] = pos;
            float speed = (ASTEROID_SPEED_FACTOR * 0.1f) / radius; // Crystals drift very slowly
            s->world.resources.velocity[i].x = vel_dir.x * speed;
            s->world.resources.velocity[i].y = vel_dir.y * speed;
            s->world.resources.radius[i] = radius;
            s->world.resources.rotation[i] = (float)(rand() % 360);
            s->world.resources.rot_speed[i] =
                ((float)(rand() % 100) / 50.0f - 1.0f) *
                (ASTEROID_ROTATION_SPEED_FACTOR * 0.1f / radius); // Slow rotation
            s->world.resources.amount[i] = radius * CRYSTAL_VALUE_MULT;
            s->world.resources.max_health[i] = radius * 5.0f; // 5 health per radius unit
            s->world.resources.health[i] = s->world.resources.max_health[i];
            s->world.resources.tex_idx[i] = rand() % CRYSTAL_COUNT;
            s->world.resources.active[i] = true;
            s->world.resource_count++;
            break;
        }
    }
}

static void UpdateSimAnchors(AppState *s, Vec2 cam_center) {
  s->world.sim_anchor_count = 0;
  s->world.sim_anchors[s->world.sim_anchor_count++].pos = cam_center;
  for (int i = 0; i < MAX_UNITS; i++) {
    if (!s->world.units.active[i])
      continue;
    if (s->world.sim_anchor_count >= MAX_SIM_ANCHORS)
      break;
    bool covered = false;
    for (int a = 0; a < s->world.sim_anchor_count; a++) {
      float dx = s->world.units.pos[i].x - s->world.sim_anchors[a].pos.x;
      float dy = s->world.units.pos[i].y - s->world.sim_anchors[a].pos.y;
      if (dx * dx + dy * dy < (DESPAWN_RANGE * 0.5f) * (DESPAWN_RANGE * 0.5f)) {
        covered = true;
        break;
      }
    }
    if (!covered)
      s->world.sim_anchors[s->world.sim_anchor_count++].pos =
          s->world.units.pos[i];
  }
}

void Game_Init(AppState *s) {
  // Mothership Stats
  s->world.unit_stats[UNIT_MOTHERSHIP] =
      (UnitStats){.max_health = 10000.0f,
                  .max_energy = 1000.0f,
                  .max_cargo = 10000.0f,
                  .speed = MOTHERSHIP_SPEED,
                  .friction = MOTHERSHIP_FRICTION,
                  .radius = MOTHERSHIP_RADIUS,
                  .radar_range = MOTHERSHIP_RADAR_RANGE,
                  .regen_rate = 0.05f,
                  .visual_scale = MOTHERSHIP_VISUAL_SCALE,
                  .main_cannon_damage = LARGE_CANNON_DAMAGE,
                  .main_cannon_range = LARGE_CANNON_RANGE,
                  .main_cannon_cooldown = LARGE_CANNON_COOLDOWN,
                  .small_cannon_damage = SMALL_CANNON_DAMAGE,
                  .small_cannon_range = SMALL_CANNON_RANGE,
                  .small_cannon_cooldown = SMALL_CANNON_COOLDOWN,
                  .small_cannon_energy_cost = SMALL_CANNON_ENERGY_COST,
                  .laser_thickness = LASER_THICKNESS_MULT,
                  .laser_glow_mult = LASER_GLOW_MULT,
                  .laser_core_thickness_mult = LASER_CORE_THICKNESS_MULT,
                  .laser_start_offset_mult = LASER_START_OFFSET_MULT};

  // Scout Stats
  s->world.unit_stats[UNIT_SCOUT] =
      (UnitStats){.max_health = 200.0f,
                  .max_energy = 100.0f,
                  .max_cargo = 500.0f,
                  .speed = 1800.0f,
                  .friction = 3.0f,
                  .radius = 60.0f,
                  .radar_range = 4000.0f,
                  .regen_rate = 0.08f,
                  .visual_scale = 1.2f,
                  .main_cannon_damage = 0,
                  .main_cannon_range = 0,
                  .main_cannon_cooldown = 0,
                  .small_cannon_damage = 200.0f,
                  .small_cannon_range = 1500.0f,
                  .small_cannon_cooldown = 0.5f,
                  .small_cannon_energy_cost = 5.0f,
                  .laser_thickness = 0.1f,
                  .laser_glow_mult = 1.5f,
                  .laser_core_thickness_mult = 0.4f,
                  .laser_start_offset_mult = 5.0f};

  // Miner Stats
  s->world.unit_stats[UNIT_MINER] =
      (UnitStats){.max_health = 150.0f,
                  .max_energy = 80.0f,
                  .max_cargo = 1000.0f,
                  .speed = 1200.0f,
                  .friction = 3.0f,
                  .radius = 50.0f,
                  .radar_range = 3000.0f,
                  .regen_rate = 0.05f,
                  .visual_scale = 1.0f,
                  .production_cost = MINER_COST,
                  .production_time = MINER_BUILD_TIME,
                  .main_cannon_damage = 0,
                  .main_cannon_range = 0,
                  .main_cannon_cooldown = 0,
                  .small_cannon_damage = 0.0f, // No attack
                  .small_cannon_range = 0.0f,
                  .small_cannon_cooldown = 1.0f,
                  .small_cannon_energy_cost = 0.0f,
                  .laser_thickness = 0.2f,
                  .laser_glow_mult = 1.8f,
                  .laser_core_thickness_mult = 0.5f,
                  .laser_start_offset_mult = 4.0f};

  // Fighter Stats
  s->world.unit_stats[UNIT_FIGHTER] =
      (UnitStats){.max_health = 300.0f,
                  .max_energy = 80.0f, // Less energy
                  .max_cargo = 100.0f,
                  .speed = 2000.0f,
                  .friction = 3.0f,
                  .radius = 55.0f,
                  .radar_range = 3500.0f,
                  .regen_rate = 0.25f, // Much faster regeneration
                  .visual_scale = 1.1f,
                  .production_cost = FIGHTER_COST,
                  .production_time = FIGHTER_BUILD_TIME,
                  .main_cannon_damage = 0,
                  .main_cannon_cooldown = 0,
                  .small_cannon_damage = 2500.0f,
                  .small_cannon_range = 3500.0f,
                  .small_cannon_cooldown = 0.25f,
                  .small_cannon_energy_cost = 10.0f, // Reduced cost
                  .laser_thickness = 0.15f,
                  .laser_glow_mult = 1.6f,
                  .laser_core_thickness_mult = 0.45f,
                  .laser_start_offset_mult = 4.5f};

  SDL_memset(&s->world.units, 0, sizeof(UnitPool));
  s->world.unit_count = 0;
  s->world.energy = INITIAL_ENERGY;
  s->world.stored_resources = 500.0f; // Starting resources

  // Clear asteroids
  SDL_memset(&s->world.asteroids, 0, sizeof(AsteroidPool));
  s->world.asteroid_count = 0;

  // Clear resources
  SDL_memset(&s->world.resources, 0, sizeof(ResourcePool));
  s->world.resource_count = 0;

  for (int i = 0; i < MAX_UNITS; i++) {
    s->world.units.active[i] = false;
    s->world.units.production_mode[i] = UNIT_TYPE_COUNT;
  }

  // Create starting Mothership
  int idx = 0;
  s->world.units.active[idx] = true;
  s->world.units.type[idx] = UNIT_MOTHERSHIP;
  s->world.units.stats[idx] = &s->world.unit_stats[UNIT_MOTHERSHIP];
  s->world.units.pos[idx] = (Vec2){0, 0};
  s->world.units.velocity[idx] = (Vec2){0, 0};
  s->world.units.rotation[idx] = 0;
  s->world.units.health[idx] = s->world.units.stats[idx]->max_health;
  s->world.units.energy[idx] = s->world.units.stats[idx]->max_energy;
  s->world.units.current_cargo[idx] = 0.0f;
  s->world.units.behavior[idx] = BEHAVIOR_DEFENSIVE;
  s->world.units.command_count[idx] = 0;
  s->world.units.command_current_idx[idx] = 0;
  s->world.units.has_target[idx] = false;
  s->world.units.large_target_idx[idx] = -1;
  s->world.units.mining_cooldown[idx] = 0.0f;
  s->world.units.production_mode[idx] = UNIT_TYPE_COUNT;
  for (int c = 0; c < 4; c++)
    s->world.units.small_target_idx[idx][c] = -1;
  s->world.unit_count = 1;

  s->selection.primary_unit_idx = 0;
  SDL_memset(s->selection.unit_selected, 0, sizeof(s->selection.unit_selected));
  s->selection.unit_selected[0] = true;

  // Initialize Camera
  int win_w, win_h;
  SDL_GetRenderOutputSize(s->renderer, &win_w, &win_h);
  s->camera.zoom = MIN_ZOOM;
  s->camera.shake_intensity = 0.0f;
  s->camera.pos.x = s->world.units.pos[idx].x - (win_w / 2.0f) / s->camera.zoom;
  s->camera.pos.y = s->world.units.pos[idx].y - (win_h / 2.0f) / s->camera.zoom;
}

static void HandleRespawn(AppState *s, float dt, int win_w, int win_h) {
  if (s->ui.respawn_timer <= 0)
    return;

  s->ui.respawn_timer -= dt;
  // During countdown, keep camera near respawn point or death point
  s->camera.pos.x = s->ui.respawn_pos.x - (win_w / 2.0f) / s->camera.zoom;
  s->camera.pos.y = s->ui.respawn_pos.y - (win_h / 2.0f) / s->camera.zoom;

  if (s->ui.respawn_timer <= 0) {
    int m_idx = -1;
    for (int i = 0; i < MAX_UNITS; i++) {
        if (s->world.units.active[i] && s->world.units.type[i] == UNIT_MOTHERSHIP) {
            m_idx = i; break;
        }
    }
    
    // If no mothership, find a free slot
    if (m_idx == -1) {
        for (int i = 0; i < MAX_UNITS; i++) {
            if (!s->world.units.active[i]) {
                m_idx = i;
                s->world.units.active[m_idx] = true;
                s->world.units.type[m_idx] = UNIT_MOTHERSHIP;
                s->world.units.stats[m_idx] = &s->world.unit_stats[UNIT_MOTHERSHIP];
                s->world.unit_count++;
                break;
            }
        }
    }

    if (m_idx != -1) {
        int i = m_idx;
        s->world.units.health[i] = s->world.units.stats[i]->max_health;
        s->world.units.energy[i] = s->world.units.stats[i]->max_energy;
        s->world.units.velocity[i] = (Vec2){0, 0};
        s->world.units.command_count[i] = 0;
        s->world.units.command_current_idx[i] = 0;
        s->world.units.has_target[i] = false;
        s->world.units.production_timer[i] = 0.0f;
        s->world.units.production_count[i] = 0;

        int attempts = 0;
        Vec2 spawn_p = s->ui.respawn_pos;
        while (attempts < RESPAWN_ATTEMPTS) {
          float rx = s->ui.respawn_pos.x + (float)(rand() % (int)(RESPAWN_RANGE * 2) - RESPAWN_RANGE),
                ry = s->ui.respawn_pos.y + (float)(rand() % (int)(RESPAWN_RANGE * 2) - RESPAWN_RANGE);
          bool safe = true;
          for (int j = 0; j < MAX_ASTEROIDS; j++) {
            if (!s->world.asteroids.active[j]) continue;
            if (Vector_DistanceSq((Vec2){rx, ry}, s->world.asteroids.pos[j]) < 
                powf(s->world.asteroids.radius[j] + s->world.units.stats[i]->radius + RESPAWN_BUFFER, 2)) {
              safe = false; break;
            }
          }
          if (safe) { spawn_p = (Vec2){rx, ry}; break; }
          attempts++;
        }
        s->world.units.pos[i] = spawn_p;
        Particles_SpawnTeleport(s, spawn_p, s->world.units.stats[i]->radius * 2.5f);
        UI_SetError(s, "MOTHERSHIP ONLINE");
        
        // Select it
        SDL_memset(s->selection.unit_selected, 0, sizeof(s->selection.unit_selected));
        s->selection.unit_selected[i] = true;
        s->selection.primary_unit_idx = i;
    }
  }
}

static void UpdateSpawning(AppState *s, Vec2 cam_center) {
  float local_density = GetAsteroidDensity(cam_center);
  int total_target_count = (int)(local_density * MAX_DYNAMIC_ASTEROIDS);
  if (total_target_count > 200)
    total_target_count = 200;

  for (int i = 0; i < MAX_ASTEROIDS; i++) {
    if (!s->world.asteroids.active[i])
      continue;
    s->world.asteroids.targeted[i] = false;
    bool in_range = false;
    for (int a = 0; a < s->world.sim_anchor_count; a++) {
      if (Vector_DistanceSq(s->world.asteroids.pos[i],
                            s->world.sim_anchors[a].pos) <
          DESPAWN_RANGE * DESPAWN_RANGE) {
        in_range = true;
        break;
      }
    }
    if (!in_range) {
      s->world.asteroids.active[i] = false;
      s->world.asteroid_count--;
    }
  }

  for (int i = 0; i < MAX_RESOURCES; i++) {
      if (!s->world.resources.active[i]) continue;
      bool in_range = false;
      for (int a = 0; a < s->world.sim_anchor_count; a++) {
          if (Vector_DistanceSq(s->world.resources.pos[i], s->world.sim_anchors[a].pos) < DESPAWN_RANGE * DESPAWN_RANGE) { in_range = true; break; }
      }
      if (!in_range) { s->world.resources.active[i] = false; s->world.resource_count--; }
  }

  int attempts = 0;
  while (s->world.asteroid_count < total_target_count &&
         attempts < ASTEROID_SPAWN_ATTEMPTS) {
    attempts++;
    Vec2 target_center =
        s->world.sim_anchors[rand() % s->world.sim_anchor_count].pos;
    float angle = (float)(rand() % 360) * 0.0174533f;
    float dist = SPAWN_MIN_DIST +
                 (float)(rand() % (int)(DESPAWN_RANGE * 0.8f - SPAWN_MIN_DIST));
    Vec2 spawn_pos = {target_center.x + cosf(angle) * dist,
                      target_center.y + sinf(angle) * dist};

    // Check safe zone around ALL anchors (units + camera)
    bool unsafe = false;
    for (int a = 0; a < s->world.sim_anchor_count; a++) {
        if (Vector_DistanceSq(spawn_pos, s->world.sim_anchors[a].pos) < SPAWN_SAFE_ZONE * SPAWN_SAFE_ZONE) {
            unsafe = true;
            break;
        }
    }
    if (unsafe) continue;

    if (((float)rand() / (float)RAND_MAX) < GetAsteroidDensity(spawn_pos)) {
      float new_rad = ASTEROID_BASE_RADIUS_MIN +
                      (rand() % (int)ASTEROID_BASE_RADIUS_VARIANCE);
      bool overlap = false;
      for (int j = 0; j < MAX_ASTEROIDS; j++) {
        if (s->world.asteroids.active[j] &&
            Vector_DistanceSq(s->world.asteroids.pos[j], spawn_pos) <
                powf((s->world.asteroids.radius[j] + new_rad) *
                             ASTEROID_HITBOX_MULT +
                         SPAWN_BUFFER,
                     2)) {
          overlap = true;
          break;
        }
      }
      if (!overlap) {
        float move_angle = (float)(rand() % 360) * 0.0174533f;
        SpawnAsteroid(s, spawn_pos, (Vec2){cosf(move_angle), sinf(move_angle)}, new_rad);
      }
    }
  }

  // --- INDEPENDENT CRYSTAL SPAWNING PASS ---
  if (s->world.resource_count < MAX_RESOURCES) {
      for (int c = 0; c < CRYSTAL_SPAWN_ATTEMPTS; c++) {
        Vec2 target_center = s->world.sim_anchors[rand() % s->world.sim_anchor_count].pos;
        float angle = (float)(rand() % 360) * 0.0174533f;
        float dist = SPAWN_MIN_DIST + (float)(rand() % (int)(DESPAWN_RANGE * 0.8f - SPAWN_MIN_DIST));
        Vec2 spawn_pos = {target_center.x + cosf(angle) * dist, target_center.y + sinf(angle) * dist};

        // Safe zone check
        bool unsafe = false;
        for (int a = 0; a < s->world.sim_anchor_count; a++) {
            if (Vector_DistanceSq(spawn_pos, s->world.sim_anchors[a].pos) < SPAWN_SAFE_ZONE * SPAWN_SAFE_ZONE) {
                unsafe = true; break;
            }
        }
        if (unsafe) continue;

        int gx_center = (int)floorf(spawn_pos.x / CELESTIAL_GRID_SIZE_F);
        int gy_center = (int)floorf(spawn_pos.y / CELESTIAL_GRID_SIZE_F);
        
        float density = GetAsteroidDensity(spawn_pos);
        if (density > 0.1f) {
            float crystal_prob = (density > 0.8f) ? CRYSTAL_PROB_GALAXY : CRYSTAL_PROB_PLANET;
            // Modulate probability by density strength
            crystal_prob *= (density / DENSITY_MAX);

            if (((float)rand() / (float)RAND_MAX) < crystal_prob) {
                float c_rad = CRYSTAL_RADIUS_LARGE_MIN + ((float)rand() / (float)RAND_MAX) * CRYSTAL_RADIUS_LARGE_VARIANCE;
                float move_angle = (float)(rand() % 360) * 0.0174533f;
                
                // Find a celestial body to anchor the cluster visually
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        Vec2 b_pos; float type_seed, b_rad;
                        if (GetCelestialBodyInfo(gx_center + dx, gy_center + dy, &b_pos, &type_seed, &b_rad)) {
                            float spread_dist = b_rad + 800.0f + ((float)rand() / (float)RAND_MAX) * 3000.0f;
                            float spread_angle = (float)(rand() % 360) * 0.0174533f;
                            Vec2 crystal_pos = {
                                b_pos.x + cosf(spread_angle) * spread_dist,
                                b_pos.y + sinf(spread_angle) * spread_dist
                            };
                            
                            if (Vector_DistanceSq(crystal_pos, cam_center) > (1280.0f/s->camera.zoom) * (1280.0f/s->camera.zoom)) {
                                SpawnCrystal(s, crystal_pos, (Vec2){cosf(move_angle), sinf(move_angle)}, c_rad);
                            }
                            goto next_crystal_attempt;
                        }
                    }
                }
            }
        }
        next_crystal_attempt:;
      }
  }
}

static void UpdateRadar(AppState *s) {
  Vec2 m_pos = {0, 0};
  bool found = false;
  for (int i = 0; i < MAX_UNITS; i++)
    if (s->world.units.active[i] && s->world.units.type[i] == UNIT_MOTHERSHIP) {
      m_pos = s->world.units.pos[i];
      found = true;
      break;
    }
  if (found && SDL_GetAtomicInt(&s->threads.radar_request_update) == 0) {
    SDL_LockMutex(s->threads.radar_mutex);
    s->threads.radar_mothership_pos = m_pos;
    SDL_UnlockMutex(s->threads.radar_mutex);
    SDL_SetAtomicInt(&s->threads.radar_request_update, 1);
  }
}

void Game_Update(AppState *s, float dt) {
  if (s->game_state == STATE_PAUSED)
    return;
  int win_w, win_h;
  SDL_GetRenderOutputSize(s->renderer, &win_w, &win_h);

  HandleRespawn(s, dt, win_w, win_h);
  if (s->ui.hold_flash_timer > 0)
    s->ui.hold_flash_timer -= dt;
  if (s->ui.tactical_flash_timer > 0)
    s->ui.tactical_flash_timer -= dt;
  if (s->ui.ui_error_timer > 0)
    s->ui.ui_error_timer -= dt;

  // Transaction Log Update
  s->ui.resource_log_timer -= dt;
  if (s->ui.resource_log_timer <= 0) {
      if (s->ui.resource_accumulator > 1.0f) {
          LogTransaction(s, s->ui.resource_accumulator, "Resource Gathered");
          s->ui.resource_accumulator = 0;
      }
      s->ui.resource_log_timer = 1.0f; // Log every 1s if gain > 0
  }
  for (int i = 0; i < MAX_LOGS; i++) {
      if (s->ui.transaction_log[i].life > 0) s->ui.transaction_log[i].life -= dt;
  }

  Vec2 cam_center = {s->camera.pos.x + (win_w / 2.0f) / s->camera.zoom,
                     s->camera.pos.y + (win_h / 2.0f) / s->camera.zoom};
  float move_speed = CAMERA_SPEED / s->camera.zoom;
  if (s->input.mouse_pos.x < EDGE_SCROLL_THRESHOLD)
    s->camera.pos.x -= move_speed * dt;
  if (s->input.mouse_pos.x > (win_w - EDGE_SCROLL_THRESHOLD))
    s->camera.pos.x += move_speed * dt;
  if (s->input.mouse_pos.y < EDGE_SCROLL_THRESHOLD)
    s->camera.pos.y -= move_speed * dt;
  if (s->input.mouse_pos.y > (win_h - EDGE_SCROLL_THRESHOLD))
    s->camera.pos.y += move_speed * dt;

  // Apply Camera Shake
  if (s->camera.shake_intensity > 0.1f) {
      s->camera.pos.x += ((float)rand()/(float)RAND_MAX - 0.5f) * s->camera.shake_intensity;
      s->camera.pos.y += ((float)rand()/(float)RAND_MAX - 0.5f) * s->camera.shake_intensity;
      s->camera.shake_intensity *= expf(-5.0f * dt); // Quick decay
  } else {
      s->camera.shake_intensity = 0;
  }

  UpdateSimAnchors(s, cam_center);
  // Update Energy
  s->world.energy = fminf(INITIAL_ENERGY, s->world.energy + ENERGY_REGEN_RATE * dt);
  for (int i = 0; i < MAX_UNITS; i++) {
      if (s->world.units.active[i] && s->world.units.type[i] != UNIT_MOTHERSHIP) {
          float regen = s->world.units.stats[i]->max_energy * s->world.units.stats[i]->regen_rate;
          s->world.units.energy[i] = fminf(s->world.units.stats[i]->max_energy, s->world.units.energy[i] + regen * dt);
      }
  }

  // Camera Management (Simulated Anchors)

  for (int i = 0; i < MAX_ASTEROIDS; i++) s->world.asteroids.targeted[i] = false;

  UpdateRadar(s);

  // Update Production Logic (Continuous Toggle)
  for (int i = 0; i < MAX_UNITS; i++) {
      if (s->world.units.active[i] && s->world.units.type[i] == UNIT_MOTHERSHIP && s->world.units.production_mode[i] != UNIT_TYPE_COUNT) {
          UnitType target_type = s->world.units.production_mode[i];
          float cost = s->world.unit_stats[target_type].production_cost;
          float build_time = s->world.unit_stats[target_type].production_time;

          // Check for resources and unit cap at start of cycle or while waiting
          if (s->world.units.production_timer[i] == 0.0f) {
              if (s->world.stored_resources < cost) {
                  // Wait for resources, don't advance timer
                  continue;
              }
              if (s->world.unit_count >= MAX_UNITS) {
                  // Unit cap reached, wait
                  continue;
              }
              // Consume resources at start of build
              s->world.stored_resources -= cost;
              LogTransaction(s, -cost, target_type == UNIT_MINER ? "Miner Production" : "Fighter Production");
          }

          s->world.units.production_timer[i] += dt;
          
          if (s->world.units.production_timer[i] >= build_time) {
              // Spawn Unit
              int new_idx = -1;
              for (int u = 0; u < MAX_UNITS; u++) { if (!s->world.units.active[u]) { new_idx = u; break; } }
              
              if (new_idx != -1) {
                  float spawn_angle = (float)(rand() % 360) * 0.0174533f;
                  float spawn_dist = s->world.units.stats[i]->radius + 150.0f + ((float)rand()/(float)RAND_MAX) * 200.0f;
                  Vec2 spawn_pos = {
                      s->world.units.pos[i].x + cosf(spawn_angle) * spawn_dist,
                      s->world.units.pos[i].y + sinf(spawn_angle) * spawn_dist
                  };

                  s->world.units.active[new_idx] = true;
                  s->world.units.type[new_idx] = target_type;
                  s->world.units.stats[new_idx] = &s->world.unit_stats[target_type];
                  s->world.units.pos[new_idx] = spawn_pos;
                  s->world.units.velocity[new_idx] = (Vec2){0,0};
                  s->world.units.rotation[new_idx] = s->world.units.rotation[i];
                  s->world.units.health[new_idx] = s->world.units.stats[new_idx]->max_health;
                  s->world.units.energy[new_idx] = s->world.units.stats[new_idx]->max_energy;
                  s->world.units.current_cargo[new_idx] = 0.0f;
                  s->world.units.behavior[new_idx] = BEHAVIOR_DEFENSIVE;
                  s->world.units.command_count[new_idx] = 0;
                  s->world.units.command_current_idx[new_idx] = 0;
                  s->world.units.has_target[new_idx] = false;
                  s->world.units.large_target_idx[new_idx] = -1;
                  s->world.units.mining_cooldown[new_idx] = 0.0f;
                  s->world.units.production_mode[new_idx] = UNIT_TYPE_COUNT;
                  for(int c=0; c<4; c++) s->world.units.small_target_idx[new_idx][c] = -1;
                  s->world.unit_count++;
                  
                  Particles_SpawnTeleport(s, spawn_pos, s->world.units.stats[new_idx]->radius * 2.0f);
                  
                  s->world.units.production_timer[i] = 0.0f; // Reset for next loop and continue producing the same type
                  UI_SetError(s, "UNIT READY");
              }
          }
      } else {
          s->world.units.production_timer[i] = 0.0f;
      }
  }

  // Update Mouse Over Asteroid
  float wx = s->camera.pos.x + s->input.mouse_pos.x / s->camera.zoom;
  float wy = s->camera.pos.y + s->input.mouse_pos.y / s->camera.zoom;
  s->input.hover_asteroid_idx = -1;
  for (int a = 0; a < MAX_ASTEROIDS; a++) {
    if (!s->world.asteroids.active[a])
      continue;
    float dx = s->world.asteroids.pos[a].x - wx,
          dy = s->world.asteroids.pos[a].y - wy;
    float r = s->world.asteroids.radius[a] * ASTEROID_HITBOX_MULT;
    if (dx * dx + dy * dy < r * r) {
      s->input.hover_asteroid_idx = a;
      break;
    }
  }

  // Update Mouse Over Resource
  s->input.hover_resource_idx = -1;
  for (int i = 0; i < MAX_RESOURCES; i++) {
      if (!s->world.resources.active[i]) continue;
      float dx = s->world.resources.pos[i].x - wx, dy = s->world.resources.pos[i].y - wy;
      // Crystals are visually larger than their physics radius might imply, especially with glow.
      // Using visual scale for hit detection feels better for UI interaction.
      float r = s->world.resources.radius[i] * CRYSTAL_VISUAL_SCALE * 0.5f; 
      if (dx*dx + dy*dy < r*r) {
          s->input.hover_resource_idx = i;
          break;
      }
  }

  UpdateSpawning(s, cam_center);

  Physics_UpdateAsteroids(s, dt);
  Physics_UpdateResources(s, dt);
  Physics_HandleCollisions(s, dt);

  for (int i = 0; i < MAX_UNITS; i++) {
    if (!s->world.units.active[i])
      continue;
    
    AI_UpdateUnitMovement(s, i, dt);

    Abilities_Update(s, i, dt);

    // Unit Destruction Logic
    if (s->world.units.health[i] <= 0) {
        s->world.units.active[i] = false;
        s->world.unit_count--;
        
        // Remove from selection and groups
        s->selection.unit_selected[i] = false;
        for (int g = 0; g < 10; g++) s->selection.group_members[g][i] = false;
        if (s->selection.primary_unit_idx == i) s->selection.primary_unit_idx = -1;

        Particles_SpawnExplosion(s, s->world.units.pos[i], 120, s->world.units.stats[i]->visual_scale * 3.0f, EXPLOSION_COLLISION, 0);
        Physics_AreaDamage(s, s->world.units.pos[i], s->world.units.stats[i]->radius * 5.0f, s->world.units.stats[i]->max_health * 0.5f, i);
        
        if (s->world.units.type[i] == UNIT_MOTHERSHIP) {
            UI_SetError(s, "MOTHERSHIP DESTROYED! RESPAWNING...");
            s->ui.respawn_timer = RESPAWN_TIME;
            s->ui.respawn_pos = s->world.units.pos[i];
        } else {
            char kill_msg[64];
            snprintf(kill_msg, 64, "UNIT LOST");
            UI_SetError(s, kill_msg);
        }
    }
  }

  Particles_Update(s, dt);
}