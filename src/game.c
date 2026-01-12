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

void Game_Init(AppState *s) {}

void Game_Update(AppState *s, float dt) {
  int win_w, win_h;
  SDL_GetRenderOutputSize(s->renderer, &win_w, &win_h);
  Vec2 cam_center = {s->camera_pos.x + (win_w / 2.0f) / s->zoom,
                     s->camera_pos.y + (win_h / 2.0f) / s->zoom};

  float move_speed = CAMERA_SPEED / s->zoom;
  if (s->mouse_pos.x < EDGE_SCROLL_THRESHOLD)
    s->camera_pos.x -= move_speed * dt;
  if (s->mouse_pos.x > (win_w - EDGE_SCROLL_THRESHOLD))
    s->camera_pos.x += move_speed * dt;
  if (s->mouse_pos.y < EDGE_SCROLL_THRESHOLD)
    s->camera_pos.y -= move_speed * dt;
  if (s->mouse_pos.y > (win_h - EDGE_SCROLL_THRESHOLD))
    s->camera_pos.y += move_speed * dt;

  float local_density = GetAsteroidDensity(cam_center);
  // Baseline of 20, scaling up to MAX_DYNAMIC_ASTEROIDS
  int dynamic_target_count =
      (int)(MIN_DYNAMIC_ASTEROIDS +
            local_density * (MAX_DYNAMIC_ASTEROIDS - MIN_DYNAMIC_ASTEROIDS));

  for (int i = 0; i < MAX_ASTEROIDS; i++) {
    if (!s->asteroids[i].active)
      continue;
    float dx = s->asteroids[i].pos.x - cam_center.x;
    float dy = s->asteroids[i].pos.y - cam_center.y;
    if (dx * dx + dy * dy > DESPAWN_RANGE * DESPAWN_RANGE) {
      s->asteroids[i].active = false;
      s->asteroid_count--;
    }
  }

  int attempts = 0;
  while (s->asteroid_count < dynamic_target_count && attempts < 100) {
    attempts++;
    float angle = (float)(rand() % 360) * 0.0174533f;
    float dist = SPAWN_SAFE_ZONE +
                 (float)(rand() % (int)(DESPAWN_RANGE - SPAWN_SAFE_ZONE));
    Vec2 spawn_pos = {cam_center.x + cosf(angle) * dist,
                      cam_center.y + sinf(angle) * dist};
    float p_density = GetAsteroidDensity(spawn_pos);
    if (((float)rand() / (float)RAND_MAX) < p_density) {
      float move_angle = (float)(rand() % 360) * 0.0174533f;
      SpawnAsteroid(s, spawn_pos, (Vec2){cosf(move_angle), sinf(move_angle)},
                    ASTEROID_BASE_RADIUS_MIN +
                        (rand() % ASTEROID_BASE_RADIUS_VARIANCE));
    }
  }

  for (int i = 0; i < MAX_ASTEROIDS; i++) {
    if (!s->asteroids[i].active)
      continue;
    s->asteroids[i].pos.x += s->asteroids[i].velocity.x * dt;
    s->asteroids[i].pos.y += s->asteroids[i].velocity.y * dt;
    s->asteroids[i].rotation += s->asteroids[i].rot_speed * dt;
  }

  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (!s->particles[i].active)
      continue;
    s->particles[i].pos.x += s->particles[i].velocity.x * dt;
    s->particles[i].pos.y += s->particles[i].velocity.y * dt;
    s->particles[i].life -= dt * PARTICLE_LIFE_DECAY;
    if (s->particles[i].life <= 0)
      s->particles[i].active = false;
  }

  for (int i = 0; i < MAX_ASTEROIDS; i++) {
    if (!s->asteroids[i].active)
      continue;
    for (int j = i + 1; j < MAX_ASTEROIDS; j++) {
      if (!s->asteroids[j].active)
        continue;
      float dx = s->asteroids[j].pos.x - s->asteroids[i].pos.x;
      float dy = s->asteroids[j].pos.y - s->asteroids[i].pos.y;
      float dist_sq = dx * dx + dy * dy;
      float r1 = s->asteroids[i].radius * 0.5f,
            r2 = s->asteroids[j].radius * 0.5f;
      float min_dist = r1 + r2;
      if (dist_sq < min_dist * min_dist) {
        Asteroid a = s->asteroids[i], b = s->asteroids[j];
        s->asteroids[i].active = false;
        s->asteroids[j].active = false;
        s->asteroid_count -= 2;
        SpawnExplosion(s, (Vec2){a.pos.x + dx * 0.5f, a.pos.y + dy * 0.5f},
                       EXPLOSION_PARTICLE_COUNT,
                       (a.radius + b.radius) / 100.0f);
        float collision_angle = atan2f(dy, dx);
        if (a.radius > ASTEROID_COLLISION_SPLIT_THRESHOLD) {
          float child_rad = a.radius * ASTEROID_SPLIT_FACTOR;
          for (int k = 0; k < 2; k++) {
            float split_angle = (k == 0) ? (collision_angle + 1.5708f)
                                         : (collision_angle - 1.5708f);
            Vec2 dir = {cosf(split_angle), sinf(split_angle)};
            SpawnAsteroid(s,
                          (Vec2){a.pos.x + dir.x * child_rad,
                                 a.pos.y + dir.y * child_rad},
                          dir, child_rad);
          }
        }
        if (b.radius > ASTEROID_COLLISION_SPLIT_THRESHOLD) {
          float child_rad = b.radius * ASTEROID_SPLIT_FACTOR;
          for (int k = 0; k < 2; k++) {
            float split_angle = (k == 0) ? (collision_angle + 1.5708f)
                                         : (collision_angle - 1.5708f);
            Vec2 dir = {cosf(split_angle), sinf(split_angle)};
            SpawnAsteroid(s,
                          (Vec2){b.pos.x + dir.x * child_rad,
                                 b.pos.y + dir.y * child_rad},
                          dir, child_rad);
          }
        }
        break;
      }
    }
  }
}
