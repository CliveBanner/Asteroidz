#include "physics.h"
#include "constants.h"
#include "utils.h"
#include <math.h>

void Physics_UpdateAsteroids(AppState *s, float dt) {
  for (int i = 0; i < MAX_ASTEROIDS; i++) {
    if (!s->world.asteroids.active[i])
      continue;
    s->world.asteroids.pos[i].x += s->world.asteroids.velocity[i].x * dt;
    s->world.asteroids.pos[i].y += s->world.asteroids.velocity[i].y * dt;
    s->world.asteroids.rotation[i] += s->world.asteroids.rot_speed[i] * dt;
  }
}

void Physics_UpdateResources(AppState *s, float dt) {
  for (int i = 0; i < MAX_RESOURCES; i++) {
    if (!s->world.resources.active[i])
      continue;
    s->world.resources.pos[i].x += s->world.resources.velocity[i].x * dt;
    s->world.resources.pos[i].y += s->world.resources.velocity[i].y * dt;
    s->world.resources.rotation[i] += s->world.resources.rot_speed[i] * dt;
  }
}

static void SolveCollision(Vec2 *p1, Vec2 *v1, float r1, Vec2 *p2, Vec2 *v2, float r2) {
    float dx = p2->x - p1->x;
    float dy = p2->y - p1->y;
    float dist_sq = dx * dx + dy * dy;
    float r_sum = (r1 + r2) * ASTEROID_HITBOX_MULT;
    if (dist_sq < r_sum * r_sum) {
        float dist = sqrtf(dist_sq);
        if (dist < 0.001f) return;
        float nx = dx / dist, ny = dy / dist;
        float overlap = r_sum - dist;
        p1->x -= nx * overlap * 0.5f;
        p1->y -= ny * overlap * 0.5f;
        p2->x += nx * overlap * 0.5f;
        p2->y += ny * overlap * 0.5f;
        float v1n = v1->x * nx + v1->y * ny;
        float v2n = v2->x * nx + v2->y * ny;
        float impulse = 2.0f * (v1n - v2n) / (r1 + r2);
        v1->x -= impulse * r2 * nx;
        v1->y -= impulse * r2 * ny;
        v2->x += impulse * r1 * nx;
        v2->y += impulse * r1 * ny;
    }
}

void Physics_HandleCollisions(AppState *s, float dt) {
  (void)dt;
  // 1. Asteroid vs Asteroid
  for (int i = 0; i < MAX_ASTEROIDS; i++) {
    if (!s->world.asteroids.active[i]) continue;
    for (int j = i + 1; j < MAX_ASTEROIDS; j++) {
      if (!s->world.asteroids.active[j]) continue;
      SolveCollision(&s->world.asteroids.pos[i], &s->world.asteroids.velocity[i], s->world.asteroids.radius[i],
                     &s->world.asteroids.pos[j], &s->world.asteroids.velocity[j], s->world.asteroids.radius[j]);
    }
  }

  // 2. (Removed) Resource vs Resource

  // 3. (Removed) Asteroid vs Resource - Crystals don't collide with asteroids

  // 4. Unit vs Asteroid/Resource
  for (int i = 0; i < MAX_UNITS; i++) {
      if (!s->world.units.active[i]) continue;
      // vs Asteroids
      for (int j = 0; j < MAX_ASTEROIDS; j++) {
          if (!s->world.asteroids.active[j]) continue;
          SolveCollision(&s->world.units.pos[i], &s->world.units.velocity[i], s->world.units.stats[i]->radius / ASTEROID_HITBOX_MULT,
                         &s->world.asteroids.pos[j], &s->world.asteroids.velocity[j], s->world.asteroids.radius[j]);
      }
      // vs Resources
      for (int j = 0; j < MAX_RESOURCES; j++) {
          if (!s->world.resources.active[j]) continue;
          SolveCollision(&s->world.units.pos[i], &s->world.units.velocity[i], s->world.units.stats[i]->radius / ASTEROID_HITBOX_MULT,
                         &s->world.resources.pos[j], &s->world.resources.velocity[j], s->world.resources.radius[j]);
      }
  }
}