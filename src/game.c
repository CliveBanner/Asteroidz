#include "game.h"
#include "constants.h"
#include "particles.h"
#include "physics.h"
#include "utils.h"
#include "weapons.h"
#include "abilities.h"
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
      s->world.asteroids.max_health[i] = radius * ASTEROID_HEALTH_MULT;
      s->world.asteroids.health[i] = s->world.asteroids.max_health[i];
      s->world.asteroids.targeted[i] = false;
      s->world.asteroid_count++;
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
      (UnitStats){.max_health = 1000.0f,
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
                  .small_cannon_energy_cost = SMALL_CANNON_ENERGY_COST};

  // Scout Stats
  s->world.unit_stats[UNIT_SCOUT] =
      (UnitStats){.max_health = 200.0f,
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
                  .small_cannon_energy_cost = 5.0f};

  SDL_memset(&s->world.units, 0, sizeof(UnitPool));
  s->world.unit_count = 0;
  s->world.energy = INITIAL_ENERGY;

  // Clear asteroids
  SDL_memset(&s->world.asteroids, 0, sizeof(AsteroidPool));
  s->world.asteroid_count = 0;

  // Spawn Initial Mothership
  int idx = 0;
  s->world.units.active[idx] = true;
  s->world.units.type[idx] = UNIT_MOTHERSHIP;
  s->world.units.stats[idx] = &s->world.unit_stats[UNIT_MOTHERSHIP];
  s->world.units.pos[idx] = (Vec2){0, 0};
  s->world.units.velocity[idx] = (Vec2){0, 0};
  s->world.units.rotation[idx] = 0;
  s->world.units.health[idx] = s->world.units.stats[idx]->max_health;
  s->world.units.energy[idx] = s->world.units.stats[idx]->max_energy;
  s->world.units.behavior[idx] = BEHAVIOR_DEFENSIVE;
  s->world.units.command_count[idx] = 0;
  s->world.units.command_current_idx[idx] = 0;
  s->world.units.has_target[idx] = false;
  s->world.units.large_target_idx[idx] = -1;
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
  s->camera.pos.x = s->world.units.pos[idx].x - (win_w / 2.0f) / s->camera.zoom;
  s->camera.pos.y = s->world.units.pos[idx].y - (win_h / 2.0f) / s->camera.zoom;
}

static void HandleRespawn(AppState *s, float dt, int win_w, int win_h) {
  if (s->ui.respawn_timer <= 0)
    return;

  s->ui.respawn_timer -= dt;
  s->camera.pos.x = s->ui.respawn_pos.x - (win_w / 2.0f) / s->camera.zoom;
  s->camera.pos.y = s->ui.respawn_pos.y - (win_h / 2.0f) / s->camera.zoom;

  if (s->ui.respawn_timer <= 0) {
    for (int i = 0; i < MAX_UNITS; i++) {
      if (s->world.units.active[i] && s->world.units.type[i] == UNIT_MOTHERSHIP) {
        s->world.units.health[i] = s->world.units.stats[i]->max_health;
        s->world.units.energy[i] = s->world.units.stats[i]->max_energy;
        s->world.units.velocity[i] = (Vec2){0, 0};
        s->world.units.command_count[i] = 0;
        s->world.units.command_current_idx[i] = 0;
        s->world.units.has_target[i] = false;

        int attempts = 0;
        while (attempts < RESPAWN_ATTEMPTS) {
          float rx = (float)(rand() % (int)(RESPAWN_RANGE * 2) - RESPAWN_RANGE),
                ry = (float)(rand() % (int)(RESPAWN_RANGE * 2) - RESPAWN_RANGE);
          bool safe = true;
          for (int j = 0; j < MAX_ASTEROIDS; j++) {
            if (!s->world.asteroids.active[j])
              continue;
            if (Vector_DistanceSq((Vec2){rx, ry}, s->world.asteroids.pos[j]) <
                powf(s->world.asteroids.radius[j] + s->world.units.stats[i]->radius +
                         RESPAWN_BUFFER,
                     2)) {
              safe = false;
              break;
            }
          }
          if (safe) {
            s->world.units.pos[i] = (Vec2){rx, ry};
            break;
          }
          attempts++;
        }
        s->camera.pos.x = s->world.units.pos[i].x - (win_w / 2.0f) / s->camera.zoom;
        s->camera.pos.y = s->world.units.pos[i].y - (win_h / 2.0f) / s->camera.zoom;
        break;
      }
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

  int attempts = 0;
  while (s->world.asteroid_count < total_target_count &&
         attempts < SPAWN_ATTEMPTS) {
    attempts++;
    Vec2 target_center =
        s->world.sim_anchors[rand() % s->world.sim_anchor_count].pos;
    float angle = (float)(rand() % 360) * 0.0174533f;
    float dist = SPAWN_MIN_DIST +
                 (float)(rand() % (int)(DESPAWN_RANGE * 0.8f - SPAWN_MIN_DIST));
    Vec2 spawn_pos = {target_center.x + cosf(angle) * dist,
                      target_center.y + sinf(angle) * dist};

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
        SpawnAsteroid(s, spawn_pos, (Vec2){cosf(move_angle), sinf(move_angle)},
                      new_rad);
      }
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

  UpdateSimAnchors(s, cam_center);
  s->world.energy =
      fminf(INITIAL_ENERGY, s->world.energy + ENERGY_REGEN_RATE * dt);

  for (int i = 0; i < MAX_ASTEROIDS; i++) s->world.asteroids.targeted[i] = false;

  UpdateRadar(s);

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

  UpdateSpawning(s, cam_center);

  Physics_UpdateAsteroids(s, dt);
  Physics_HandleCollisions(s, dt);

  for (int i = 0; i < MAX_UNITS; i++) {
    if (!s->world.units.active[i])
      continue;
    
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
            continue;
          }
          cur_cmd->pos = s->world.asteroids.pos[ti];
      }

      if (s->world.units.has_target[i]) {
        float dsq = Vector_DistanceSq(cur_cmd->pos, s->world.units.pos[i]);
        float stop_dist = UNIT_STOP_DIST;
        
        if (cur_cmd->type == CMD_ATTACK_MOVE && cur_cmd->target_idx != -1) {
            int ti = cur_cmd->target_idx;
            stop_dist = s->world.units.stats[i]->small_cannon_range + s->world.asteroids.radius[ti] * ASTEROID_HITBOX_MULT;
            stop_dist *= 0.95f;
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

    Abilities_Update(s, i, dt);
  }

  Particles_Update(s, dt);
}