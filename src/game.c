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
  int puff_count = count / EXPLOSION_PUFF_DIVISOR;
  for (int i = 0; i < puff_count; i++) {
    int idx = s->particle_next_idx;
    s->particles[idx].active = true;
    s->particles[idx].type = PARTICLE_PUFF;
    s->particles[idx].pos = pos;
    float angle = (float)(rand() % 360) * 0.0174533f;
    float speed = (float)(rand() % 100 + 20) * size_mult;
    s->particles[idx].velocity.x = cosf(angle) * speed;
    s->particles[idx].velocity.y = sinf(angle) * speed;
    s->particles[idx].life = PARTICLE_LIFE_BASE;
    s->particles[idx].size = (float)(rand() % 100 + 50) * size_mult;
    Uint8 v = (Uint8)(rand() % 50 + 100);
    s->particles[idx].color =
        (SDL_Color){v, (Uint8)(v * 0.9f), (Uint8)(v * 0.8f), 255};
    s->particle_next_idx = (s->particle_next_idx + 1) % MAX_PARTICLES;
  }
  for (int i = 0; i < count; i++) {
    int idx = s->particle_next_idx;
    s->particles[idx].active = true;
    s->particles[idx].type = PARTICLE_SPARK;
    s->particles[idx].pos = pos;
    float angle = (float)(rand() % 360) * 0.0174533f;
    float speed = (float)(rand() % 300 + 100) * size_mult;
    s->particles[idx].velocity.x = cosf(angle) * speed;
    s->particles[idx].velocity.y = sinf(angle) * speed;
    s->particles[idx].life = PARTICLE_LIFE_BASE;
    s->particles[idx].size = (float)(rand() % 6 + 2) * size_mult;
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
    u->radius = MOTHERSHIP_RADIUS;
    u->command_count = 0;
    u->has_target = false;
    u->large_target_idx = -1;
    for(int c=0; c<4; c++) u->small_target_idx[c] = -1;
    s->selected_unit_idx = 0;
    s->unit_selected[0] = true;
}

static void FireWeapon(AppState *s, Unit *u, int asteroid_idx, float damage, float energy_cost) {
    if (s->energy < energy_cost) return;
    s->energy -= energy_cost;
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
    if (dist > 0.1f) {
        start_pos.x += (dx / dist) * u->radius;
        start_pos.y += (dy / dist) * u->radius;
    }

    int p_idx = s->particle_next_idx;
    s->particles[p_idx].active = true;
    s->particles[p_idx].type = PARTICLE_TRACER;
    s->particles[p_idx].pos = start_pos;
    s->particles[p_idx].target_pos = a->pos;
    s->particles[p_idx].life = 0.1f + (damage / 500.0f);
    s->particles[p_idx].size = 1.0f + (damage / 50.0f);
    s->particles[p_idx].color = (SDL_Color){255, 255, 50, 255}; // Yellow
    s->particle_next_idx = (s->particle_next_idx + 1) % MAX_PARTICLES;
}

void Game_Update(AppState *s, float dt) {
  int win_w, win_h;
  SDL_GetRenderOutputSize(s->renderer, &win_w, &win_h);
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
    float dist = SPAWN_SAFE_ZONE + (float)(rand() % (int)(DESPAWN_RANGE - SPAWN_SAFE_ZONE));
    Vec2 spawn_pos = {target_center.x + cosf(angle) * dist, target_center.y + sinf(angle) * dist};
    float p_density = GetAsteroidDensity(spawn_pos);
    if (((float)rand() / (float)RAND_MAX) < p_density) {
      float move_angle = (float)(rand() % 360) * 0.0174533f;
      SpawnAsteroid(s, spawn_pos, (Vec2){cosf(move_angle), sinf(move_angle)}, ASTEROID_BASE_RADIUS_MIN + (rand() % (int)ASTEROID_BASE_RADIUS_VARIANCE));
    }
  }
  for (int i = 0; i < MAX_ASTEROIDS; i++) {
    if (!s->asteroids[i].active) continue;
    s->asteroids[i].pos.x += s->asteroids[i].velocity.x * dt;
    s->asteroids[i].pos.y += s->asteroids[i].velocity.y * dt;
    s->asteroids[i].rotation += s->asteroids[i].rot_speed * dt;
  }
  for (int i = 0; i < MAX_UNITS; i++) {
      if (!s->units[i].active) continue;
      Unit *u = &s->units[i];
      bool can_attack = false;
      if (u->has_target) {
          Command *cur_cmd = &u->command_queue[u->command_current_idx];
          if (cur_cmd->type == CMD_HOLD) {
              u->has_target = false; u->command_count = 0; u->command_current_idx = 0; u->velocity = (Vec2){0, 0};
          } else {
              if (cur_cmd->type == CMD_ATTACK || cur_cmd->type == CMD_PATROL) can_attack = true;
              float dx = cur_cmd->pos.x - u->pos.x, dy = cur_cmd->pos.y - u->pos.y;
              float dist = sqrtf(dx * dx + dy * dy);
              if (dist > 10.0f) { u->velocity.x = (dx / dist) * MOTHERSHIP_SPEED; u->velocity.y = (dy / dist) * MOTHERSHIP_SPEED; }
              else {
                  if (cur_cmd->type == CMD_PATROL) u->command_current_idx = (u->command_current_idx + 1) % u->command_count;
                  else {
                      u->command_current_idx++;
                      if (u->command_current_idx >= u->command_count) { u->velocity = (Vec2){0, 0}; u->has_target = false; u->command_count = 0; u->command_current_idx = 0; }
                  }
              }
          }
      }
      u->pos.x += u->velocity.x * dt; u->pos.y += u->velocity.y * dt; u->rotation += 0.1f * dt;
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
                  FireWeapon(s, u, l_target, LARGE_CANNON_DAMAGE, LARGE_CANNON_ENERGY_COST);
                  u->large_cannon_cooldown = LARGE_CANNON_COOLDOWN;
              }
          }
          for (int c = 0; c < 4; c++) {
              if (s_targets[c] != -1 && s->asteroids[s_targets[c]].active) {
                  s->asteroids[s_targets[c]].targeted = true;
                  if (u->small_cannon_cooldown[c] <= 0) {
                      FireWeapon(s, u, s_targets[c], SMALL_CANNON_DAMAGE, SMALL_CANNON_ENERGY_COST);
                      u->small_cannon_cooldown[c] = SMALL_CANNON_COOLDOWN;
                  }
              }
          }
      }
  }
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (!s->particles[i].active) continue;
    if (s->particles[i].type == PARTICLE_TRACER) s->particles[i].life -= dt * 5.0f;
    else { s->particles[i].pos.x += s->particles[i].velocity.x * dt; s->particles[i].pos.y += s->particles[i].velocity.y * dt; s->particles[i].life -= dt * PARTICLE_LIFE_DECAY; }
    if (s->particles[i].life <= 0) s->particles[i].active = false;
  }
  for (int i = 0; i < MAX_ASTEROIDS; i++) {
    if (!s->asteroids[i].active) continue;
    for (int j = i + 1; j < MAX_ASTEROIDS; j++) {
      if (!s->asteroids[j].active) continue;
      float dx = s->asteroids[j].pos.x - s->asteroids[i].pos.x, dy = s->asteroids[j].pos.y - s->asteroids[i].pos.y;
      float dist_sq = dx * dx + dy * dy;
      float r1 = s->asteroids[i].radius * 0.5f, r2 = s->asteroids[j].radius * 0.5f;
      float min_dist = r1 + r2;
      if (dist_sq < min_dist * min_dist) {
        Asteroid a = s->asteroids[i], b = s->asteroids[j];
        s->asteroids[i].active = false; s->asteroids[j].active = false; s->asteroid_count -= 2;
        SpawnExplosion(s, (Vec2){a.pos.x + dx * 0.5f, a.pos.y + dy * 0.5f}, 40, (a.radius + b.radius) / 100.0f);
        float collision_angle = atan2f(dy, dx);
        if (a.radius > ASTEROID_COLLISION_SPLIT_THRESHOLD) {
          float child_rad = ASTEROID_SPLIT_MIN_RADIUS * powf(a.radius / ASTEROID_SPLIT_MIN_RADIUS, ASTEROID_SPLIT_EXPONENT);
          for (int k = 0; k < 2; k++) {
            float sa = (k == 0) ? (collision_angle + 1.5708f) : (collision_angle - 1.5708f);
            SpawnAsteroid(s, (Vec2){a.pos.x + cosf(sa) * child_rad, a.pos.y + sinf(sa) * child_rad}, (Vec2){cosf(sa), sinf(sa)}, child_rad);
          }
        }
        break;
      }
    }
    for (int u = 0; u < MAX_UNITS; u++) {
        if (!s->units[u].active) continue;
        float dx = s->units[u].pos.x - s->asteroids[i].pos.x, dy = s->units[u].pos.y - s->asteroids[i].pos.y;
        float dsq = dx * dx + dy * dy, md = s->asteroids[i].radius + s->units[u].radius * 0.8f;
        if (dsq < md * md) {
            s->units[u].health -= s->asteroids[i].radius * 0.5f;
            SpawnExplosion(s, s->asteroids[i].pos, 20, s->asteroids[i].radius / 100.0f);
            s->asteroids[i].active = false; s->asteroid_count--;
            if (s->units[u].health <= 0) {
                SpawnExplosion(s, s->units[u].pos, 100, 2.0f);
                if (s->units[u].type == UNIT_MOTHERSHIP) {
                    // Respawn logic
                    s->units[u].health = s->units[u].max_health;
                    s->units[u].energy = s->units[u].max_energy;
                    s->units[u].pos.x = (float)(rand() % 5000 - 2500);
                    s->units[u].pos.y = (float)(rand() % 5000 - 2500);
                    s->units[u].velocity = (Vec2){0, 0};
                    s->units[u].command_count = 0;
                    s->units[u].command_current_idx = 0;
                    s->units[u].has_target = false;
                    
                    // Move camera to new position
                    int ww, wh;
                    SDL_GetRenderOutputSize(s->renderer, &ww, &wh);
                    s->camera_pos.x = s->units[u].pos.x - (ww / 2.0f) / s->zoom;
                    s->camera_pos.y = s->units[u].pos.y - (wh / 2.0f) / s->zoom;
                } else {
                    s->units[u].active = false;
                }
            }
            break;
        }
    }
  }
}