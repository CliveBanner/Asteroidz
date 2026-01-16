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

void Physics_HandleCollisions(AppState *s, float dt) {
  (void)dt;
  for (int i = 0; i < MAX_ASTEROIDS; i++) {
    if (!s->world.asteroids.active[i])
      continue;
    for (int j = i + 1; j < MAX_ASTEROIDS; j++) {
      if (!s->world.asteroids.active[j])
        continue;
      float dx = s->world.asteroids.pos[j].x - s->world.asteroids.pos[i].x;
      float dy = s->world.asteroids.pos[j].y - s->world.asteroids.pos[i].y;
      float dist_sq = dx * dx + dy * dy;
      float r_sum = (s->world.asteroids.radius[i] + s->world.asteroids.radius[j]) *
                    ASTEROID_HITBOX_MULT;
      if (dist_sq < r_sum * r_sum) {
        float dist = sqrtf(dist_sq);
        if (dist < 0.001f)
          continue;
        float nx = dx / dist, ny = dy / dist;
        float overlap = r_sum - dist;
        s->world.asteroids.pos[i].x -= nx * overlap * 0.5f;
        s->world.asteroids.pos[i].y -= ny * overlap * 0.5f;
        s->world.asteroids.pos[j].x += nx * overlap * 0.5f;
        s->world.asteroids.pos[j].y += ny * overlap * 0.5f;
        float v1n = s->world.asteroids.velocity[i].x * nx +
                    s->world.asteroids.velocity[i].y * ny;
        float v2n = s->world.asteroids.velocity[j].x * nx +
                    s->world.asteroids.velocity[j].y * ny;
        float impulse = 2.0f * (v1n - v2n) /
                        (s->world.asteroids.radius[i] + s->world.asteroids.radius[j]);
        s->world.asteroids.velocity[i].x -=
            impulse * s->world.asteroids.radius[j] * nx;
        s->world.asteroids.velocity[i].y -=
            impulse * s->world.asteroids.radius[j] * ny;
        s->world.asteroids.velocity[j].x +=
            impulse * s->world.asteroids.radius[i] * nx;
        s->world.asteroids.velocity[j].y +=
            impulse * s->world.asteroids.radius[i] * ny;
      }
    }
  }
}