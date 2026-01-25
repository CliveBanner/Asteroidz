#include "physics.h"
#include "constants.h"
#include "game.h"
#include "particles.h"
#include "utils.h"
#include <math.h>
#include <stdlib.h>

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

static float SolveCollision(Vec2 *p1, Vec2 *v1, float r1, Vec2 *p2, Vec2 *v2, float r2, bool is_unit1, bool is_unit2) {
    float dx = p2->x - p1->x;
    float dy = p2->y - p1->y;
    float dist_sq = dx * dx + dy * dy;
    
    // Units use full radius for hitbox, asteroids use ASTEROID_HITBOX_MULT
    float effective_r1 = is_unit1 ? r1 : (r1 * ASTEROID_HITBOX_MULT);
    float effective_r2 = is_unit2 ? r2 : (r2 * ASTEROID_HITBOX_MULT);
    float r_sum = effective_r1 + effective_r2;

    if (dist_sq < r_sum * r_sum) {
        float dist = sqrtf(dist_sq);
        if (dist < 0.001f) return 0;
        float nx = dx / dist, ny = dy / dist;
        float overlap = r_sum - dist;
        p1->x -= nx * overlap * 0.5f;
        p1->y -= ny * overlap * 0.5f;
        p2->x += nx * overlap * 0.5f;
        p2->y += ny * overlap * 0.5f;
        
        float v1n = v1->x * nx + v1->y * ny;
        float v2n = v2->x * nx + v2->y * ny;
        
        // Elastic collision impulse
        float m1 = r1; // Mass proportional to radius
        float m2 = r2;
        float impulse = 2.0f * (v1n - v2n) / (m1 + m2);
        
        v1->x -= impulse * m2 * nx;
        v1->y -= impulse * m2 * ny;
        v2->x += impulse * m1 * nx;
        v2->y += impulse * m1 * ny;
        
        return fabsf(impulse * (m1 + m2)); // Return force-like value for damage
    }
    return 0;
}

void Physics_AreaDamage(AppState *s, Vec2 pos, float range, float damage, int exclude_unit_idx) {
    float range_sq = range * range;
    // Damage units
    for (int i = 0; i < MAX_UNITS; i++) {
        if (!s->world.units.active[i] || i == exclude_unit_idx) continue;
        float dsq = Vector_DistanceSq(pos, s->world.units.pos[i]);
        if (dsq < range_sq) {
            float dist = sqrtf(dsq);
            float falloff = 1.0f - (dist / range);
            s->world.units.health[i] -= damage * falloff;
        }
    }
    // Damage asteroids
    for (int i = 0; i < MAX_ASTEROIDS; i++) {
        if (!s->world.asteroids.active[i]) continue;
        float dsq = Vector_DistanceSq(pos, s->world.asteroids.pos[i]);
        if (dsq < range_sq) {
            float dist = sqrtf(dsq);
            float falloff = 1.0f - (dist / range);
            s->world.asteroids.health[i] -= damage * falloff * 0.5f;
        }
    }
}

void Physics_HandleCollisions(AppState *s, float dt) {
  (void)dt;
  // 1. Asteroid vs Asteroid
  for (int i = 0; i < MAX_ASTEROIDS; i++) {
    if (!s->world.asteroids.active[i]) continue;
    for (int j = i + 1; j < MAX_ASTEROIDS; j++) {
      if (!s->world.asteroids.active[j]) continue;
      float imp = SolveCollision(&s->world.asteroids.pos[i], &s->world.asteroids.velocity[i], s->world.asteroids.radius[i],
                     &s->world.asteroids.pos[j], &s->world.asteroids.velocity[j], s->world.asteroids.radius[j], false, false);
      
      if (imp > ASTEROID_COLLISION_SPLIT_THRESHOLD) {
          // Both asteroids take damage
          s->world.asteroids.health[i] -= imp * 0.1f;
          s->world.asteroids.health[j] -= imp * 0.1f;
          
          // Check for splitting
          int targets[2] = {i, j};
          for(int k=0; k<2; k++) {
              int idx = targets[k];
              if (s->world.asteroids.radius[idx] > ASTEROID_SPLIT_MIN_RADIUS) {
                  float old_rad = s->world.asteroids.radius[idx];
                  float new_rad = old_rad * ASTEROID_SPLIT_EXPONENT;
                  Vec2 pos = s->world.asteroids.pos[idx];
                  Vec2 vel = s->world.asteroids.velocity[idx];
                  
                  // Deactivate old
                  s->world.asteroids.active[idx] = false;
                  s->world.asteroid_count--;
                  
                  // Explosion VFX for splitting
                  Particles_SpawnExplosion(s, pos, 20, old_rad / 500.0f, EXPLOSION_COLLISION, s->world.asteroids.tex_idx[idx]);

                  // Spawn two smaller fragments
                  for(int f=0; f<2; f++) {
                      float angle = ((float)rand()/(float)RAND_MAX) * 2.0f * 3.14159f;
                      Vec2 off = {cosf(angle) * new_rad, sinf(angle) * new_rad};
                      Vec2 f_pos = {pos.x + off.x, pos.y + off.y};
                      Vec2 f_vel = {vel.x + off.x * 0.5f, vel.y + off.y * 0.5f};
                      SpawnAsteroid(s, f_pos, Vector_Normalize(f_vel), new_rad);
                  }
              }
          }
      }
    }
  }

  // 4. Unit vs Asteroid/Resource
  for (int i = 0; i < MAX_UNITS; i++) {
      if (!s->world.units.active[i]) continue;
      // vs Asteroids
      for (int j = 0; j < MAX_ASTEROIDS; j++) {
          if (!s->world.asteroids.active[j]) continue;
          float imp = SolveCollision(&s->world.units.pos[i], &s->world.units.velocity[i], s->world.units.stats[i]->radius,
                         &s->world.asteroids.pos[j], &s->world.asteroids.velocity[j], s->world.asteroids.radius[j], true, false);
          if (imp > 10.0f) { // Lower threshold for explosion
              s->world.units.health[i] -= imp * 0.5f; // Units take more damage from collisions
              
              // Asteroid explodes on unit collision
              float rad = s->world.asteroids.radius[j];
              Vec2 pos = s->world.asteroids.pos[j];
              int tex = s->world.asteroids.tex_idx[j];
              
              s->world.asteroids.active[j] = false;
              s->world.asteroid_count--;
              
              Particles_SpawnExplosion(s, pos, 40, rad / 200.0f, EXPLOSION_COLLISION, tex);
              Physics_AreaDamage(s, pos, rad * 2.5f, rad * 50.0f, -1);
          }
      }
      // vs Resources
      for (int j = 0; j < MAX_RESOURCES; j++) {
          if (!s->world.resources.active[j]) continue;
          float imp = SolveCollision(&s->world.units.pos[i], &s->world.units.velocity[i], s->world.units.stats[i]->radius,
                         &s->world.resources.pos[j], &s->world.resources.velocity[j], s->world.resources.radius[j], true, false);
          if (imp > 50.0f) {
              s->world.units.health[i] -= imp * 0.02f;
          }
      }
  }
}