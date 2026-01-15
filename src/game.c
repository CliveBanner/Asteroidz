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

void SpawnAsteroid(AppState *s, Vec2 pos, Vec2 vel_dir, float radius) {
  if (s->world.asteroid_count >= MAX_ASTEROIDS)
    return;
  for (int i = 0; i < MAX_ASTEROIDS; i++) {
    if (!s->world.asteroids[i].active) {
      s->world.asteroids[i].pos = pos;
      float speed = ASTEROID_SPEED_FACTOR / radius;
      s->world.asteroids[i].velocity.x = vel_dir.x * speed;
      s->world.asteroids[i].velocity.y = vel_dir.y * speed;
      s->world.asteroids[i].radius = radius;
      s->world.asteroids[i].rotation = (float)(rand() % 360);
      s->world.asteroids[i].rot_speed =
          ((float)(rand() % 100) / 50.0f - 1.0f) *
          (ASTEROID_ROTATION_SPEED_FACTOR / radius);
      s->world.asteroids[i].tex_idx = rand() % ASTEROID_TYPE_COUNT;
      s->world.asteroids[i].active = true;
      s->world.asteroids[i].max_health = radius * ASTEROID_HEALTH_MULT;
      s->world.asteroids[i].health = s->world.asteroids[i].max_health;
      s->world.asteroids[i].targeted = false;
      s->world.asteroid_count++;
      break;
    }
  }
}

static void UpdateSimAnchors(AppState *s, Vec2 cam_center) {
  s->world.sim_anchor_count = 0;
  s->world.sim_anchors[s->world.sim_anchor_count++].pos = cam_center;
  for (int i = 0; i < MAX_UNITS; i++) {
    if (!s->world.units[i].active)
      continue;
    if (s->world.sim_anchor_count >= MAX_SIM_ANCHORS)
      break;
    bool covered = false;
    for (int a = 0; a < s->world.sim_anchor_count; a++) {
      float dx = s->world.units[i].pos.x - s->world.sim_anchors[a].pos.x;
      float dy = s->world.units[i].pos.y - s->world.sim_anchors[a].pos.y;
      if (dx * dx + dy * dy < (DESPAWN_RANGE * 0.5f) * (DESPAWN_RANGE * 0.5f)) {
        covered = true;
        break;
      }
    }
    if (!covered)
      s->world.sim_anchors[s->world.sim_anchor_count++].pos =
          s->world.units[i].pos;
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

  for (int i = 0; i < MAX_UNITS; i++)
    s->world.units[i].active = false;
  s->world.unit_count = 0;
  s->world.energy = INITIAL_ENERGY;

  // Spawn Initial Mothership
  int idx = s->world.unit_count++;
  Unit *u = &s->world.units[idx];
  u->active = true;
  u->type = UNIT_MOTHERSHIP;
  u->stats = &s->world.unit_stats[UNIT_MOTHERSHIP];
  u->pos = (Vec2){0, 0};
  u->velocity = (Vec2){0, 0};
  u->rotation = 0;
  u->health = u->stats->max_health;
  u->energy = u->stats->max_energy;
  u->behavior = BEHAVIOR_OFFENSIVE;
  u->command_count = 0;
  u->command_current_idx = 0;
  u->has_target = false;
  u->large_target_idx = -1;
  for (int c = 0; c < 4; c++)
    u->small_target_idx[c] = -1;

  s->selection.primary_unit_idx = 0;
  SDL_memset(s->selection.unit_selected, 0, sizeof(s->selection.unit_selected));
  s->selection.unit_selected[0] = true;
}

static void HandleRespawn(AppState *s, float dt, int win_w, int win_h) {
  if (s->ui.respawn_timer <= 0)
    return;

  s->ui.respawn_timer -= dt;
  s->camera.pos.x = s->ui.respawn_pos.x - (win_w / 2.0f) / s->camera.zoom;
  s->camera.pos.y = s->ui.respawn_pos.y - (win_h / 2.0f) / s->camera.zoom;

  if (s->ui.respawn_timer <= 0) {
    for (int i = 0; i < MAX_UNITS; i++) {
      if (s->world.units[i].type == UNIT_MOTHERSHIP) {
        Unit *u = &s->world.units[i];
        u->active = true;
        u->health = u->stats->max_health;
        u->energy = u->stats->max_energy;
        u->velocity = (Vec2){0, 0};
        u->command_count = 0;
        u->command_current_idx = 0;
        u->has_target = false;

        int attempts = 0;
        while (attempts < RESPAWN_ATTEMPTS) {
          float rx = (float)(rand() % (int)(RESPAWN_RANGE * 2) - RESPAWN_RANGE),
                ry = (float)(rand() % (int)(RESPAWN_RANGE * 2) - RESPAWN_RANGE);
          bool safe = true;
          for (int j = 0; j < MAX_ASTEROIDS; j++) {
            if (!s->world.asteroids[j].active)
              continue;
            if (Vector_DistanceSq((Vec2){rx, ry}, s->world.asteroids[j].pos) <
                powf(s->world.asteroids[j].radius + u->stats->radius +
                         RESPAWN_BUFFER,
                     2)) {
              safe = false;
              break;
            }
          }
          if (safe) {
            u->pos = (Vec2){rx, ry};
            break;
          }
          attempts++;
        }
        s->camera.pos.x = u->pos.x - (win_w / 2.0f) / s->camera.zoom;
        s->camera.pos.y = u->pos.y - (win_h / 2.0f) / s->camera.zoom;
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
    if (!s->world.asteroids[i].active)
      continue;
    s->world.asteroids[i].targeted = false;
    bool in_range = false;
    for (int a = 0; a < s->world.sim_anchor_count; a++) {
      if (Vector_DistanceSq(s->world.asteroids[i].pos,
                            s->world.sim_anchors[a].pos) <
          DESPAWN_RANGE * DESPAWN_RANGE) {
        in_range = true;
        break;
      }
    }
    if (!in_range) {
      s->world.asteroids[i].active = false;
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
        if (s->world.asteroids[j].active &&
            Vector_DistanceSq(s->world.asteroids[j].pos, spawn_pos) <
                powf((s->world.asteroids[j].radius + new_rad) *
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
    if (s->world.units[i].active && s->world.units[i].type == UNIT_MOTHERSHIP) {
      m_pos = s->world.units[i].pos;
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

  for (int i = 0; i < MAX_ASTEROIDS; i++) s->world.asteroids[i].targeted = false;

  UpdateRadar(s);

  // Update Mouse Over Asteroid
  float wx = s->camera.pos.x + s->input.mouse_pos.x / s->camera.zoom;
  float wy = s->camera.pos.y + s->input.mouse_pos.y / s->camera.zoom;
  s->input.hover_asteroid_idx = -1;
  for (int a = 0; a < MAX_ASTEROIDS; a++) {
    if (!s->world.asteroids[a].active)
      continue;
    float dx = s->world.asteroids[a].pos.x - wx,
          dy = s->world.asteroids[a].pos.y - wy;
    if (dx * dx + dy * dy <
        s->world.asteroids[a].radius * s->world.asteroids[a].radius) {
      s->input.hover_asteroid_idx = a;
      break;
    }
  }

  UpdateSpawning(s, cam_center);

  Physics_UpdateAsteroids(s, dt);
  Physics_HandleCollisions(s, dt);

  for (int i = 0; i < MAX_UNITS; i++) {
    if (!s->world.units[i].active)
      continue;
    Unit *u = &s->world.units[i];
    if (u->has_target) {
      Command *cur_cmd = &u->command_queue[u->command_current_idx];

      if (u->behavior == BEHAVIOR_HOLD_GROUND) {
          u->velocity = (Vec2){0,0};
          // Don't advance command, just stay put and rotate
      }

      // Auto-advance if target-based command target is dead
      if (cur_cmd->type == CMD_ATTACK_MOVE && cur_cmd->target_idx != -1) {
          Asteroid *a = &s->world.asteroids[cur_cmd->target_idx];
          if (!a->active) {
            u->command_current_idx++;
            if (u->command_current_idx >= u->command_count) {
              u->has_target = false;
              u->command_count = 0;
              u->command_current_idx = 0;
            }
            continue;
          }
          // Update command position to track moving target
          cur_cmd->pos = a->pos;
      }

      if (u->has_target) {
        float dsq = Vector_DistanceSq(cur_cmd->pos, u->pos);
        float stop_dist = UNIT_STOP_DIST;
        
        // If attack move with target, stop at firing range
        if (cur_cmd->type == CMD_ATTACK_MOVE && cur_cmd->target_idx != -1) {
            Asteroid *a = &s->world.asteroids[cur_cmd->target_idx];
            stop_dist = u->stats->small_cannon_range + a->radius * ASTEROID_HITBOX_MULT;
            stop_dist *= 0.9f; // Hysteresis buffer
        }

        if (dsq > stop_dist * stop_dist) {
          float dist = sqrtf(dsq), speed = u->stats->speed;
          // Only brake if it's the final command and NOT a patrol command
          // (which loops)
          if (cur_cmd->type != CMD_PATROL &&
              (u->command_current_idx == u->command_count - 1) &&
              dist < UNIT_BRAKING_DIST)
            speed *= (dist / UNIT_BRAKING_DIST);
          Vec2 target_v = Vector_Scale(
              Vector_Normalize(Vector_Sub(cur_cmd->pos, u->pos)), speed);
          u->velocity.x +=
              (target_v.x - u->velocity.x) * UNIT_STEERING_FORCE * dt;
          u->velocity.y +=
              (target_v.y - u->velocity.y) * UNIT_STEERING_FORCE * dt;
        } else {
          // Reached waypoint
          bool should_advance = true;
          if (cur_cmd->type == CMD_ATTACK_MOVE && cur_cmd->target_idx != -1) {
              // If we have a specific target, we only "reach" the waypoint if the target is dead.
              // Since we already handled dead targets at the top of the loop,
              // being here means the target is alive and we are simply in range.
              should_advance = false;
              u->velocity = (Vec2){0,0}; // Stay put
          }

          if (should_advance) {
              if (cur_cmd->type == CMD_PATROL) {
                // If we are at the last point, loop back to the FIRST consecutive
                // patrol point
                if (u->command_current_idx == u->command_count - 1) {
                  int first_patrol = u->command_current_idx;
                  while (first_patrol > 0 &&
                         u->command_queue[first_patrol - 1].type == CMD_PATROL) {
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
    }
    u->velocity = Vector_Sub(
        u->velocity, Vector_Scale(u->velocity, u->stats->friction * dt));
    u->pos = Vector_Add(u->pos, Vector_Scale(u->velocity, dt));

    // Rotate to face movement direction or target
    float speed_sq =
        u->velocity.x * u->velocity.x + u->velocity.y * u->velocity.y;
    bool should_rotate = false;
    float target_rot = 0.0f;

    if (speed_sq > 100.0f) {
      target_rot = atan2f(u->velocity.y, u->velocity.x) * (180.0f / SDL_PI_F) + 90.0f;
      should_rotate = true;
    } else if (u->has_target) {
      Command *cur_cmd = &u->command_queue[u->command_current_idx];
      Vec2 dir = Vector_Sub(cur_cmd->pos, u->pos);
      if (Vector_Length(dir) > 0.1f) {
          target_rot = atan2f(dir.y, dir.x) * (180.0f / SDL_PI_F) + 90.0f;
          should_rotate = true;
      }
    }

    if (should_rotate) {
      // Smooth rotation (LerpAngle equivalent)
      float diff = target_rot - u->rotation;
      while (diff < -180.0f)
        diff += 360.0f;
      while (diff > 180.0f)
        diff -= 360.0f;
      u->rotation += diff * UNIT_STEERING_FORCE * dt;
    }

    Abilities_Update(s, u, dt);
  }

  Particles_Update(s, dt);
}
