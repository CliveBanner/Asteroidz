#include "physics.h"
#include "utils.h"
#include "game.h" 
#include "particles.h"
#include <math.h>
#include <stdlib.h>

void Physics_UpdateAsteroids(AppState *s, float dt) {
    for (int i = 0; i < MAX_ASTEROIDS; i++) {
        if (!s->world.asteroids[i].active) continue;
        int gx = (int)floorf(s->world.asteroids[i].pos.x / CELESTIAL_GRID_SIZE_F);
        int gy = (int)floorf(s->world.asteroids[i].pos.y / CELESTIAL_GRID_SIZE_F);
        
        for (int oy = -1; oy <= 1; oy++) {
            for (int ox = -1; ox <= 1; ox++) {
                Vec2 b_pos; float b_type, b_rad;
                if (GetCelestialBodyInfo(gx + ox, gy + oy, &b_pos, &b_type, &b_rad)) {
                    float dsq = Vector_DistanceSq(b_pos, s->world.asteroids[i].pos);
                    if (dsq > GRAVITY_MIN_DIST) {
                        float force = (b_type > 0.95f ? GRAVITY_MAX_FORCE : GRAVITY_MIN_FORCE) / dsq;
                        Vec2 dir = Vector_Normalize(Vector_Sub(b_pos, s->world.asteroids[i].pos));
                        s->world.asteroids[i].velocity = Vector_Add(s->world.asteroids[i].velocity, Vector_Scale(dir, force * dt));
                    }
                }
            }
        }
        s->world.asteroids[i].pos = Vector_Add(s->world.asteroids[i].pos, Vector_Scale(s->world.asteroids[i].velocity, dt));
        s->world.asteroids[i].rotation += s->world.asteroids[i].rot_speed * dt;
    }
}

void Physics_HandleCollisions(AppState *s, float dt) {
    // 1. Asteroid vs Asteroid
    for (int i = 0; i < MAX_ASTEROIDS; i++) {
        if (!s->world.asteroids[i].active) continue;
        for (int j = i + 1; j < MAX_ASTEROIDS; j++) {
            if (!s->world.asteroids[j].active) continue;
            float dsq = Vector_DistanceSq(s->world.asteroids[j].pos, s->world.asteroids[i].pos);
            float r1 = s->world.asteroids[i].radius * ASTEROID_HITBOX_MULT;
            float r2 = s->world.asteroids[j].radius * ASTEROID_HITBOX_MULT;
            
            if (dsq < (r1 + r2) * (r1 + r2)) {
                Asteroid *a = &s->world.asteroids[i];
                Asteroid *b = &s->world.asteroids[j];
                
                // Spawn collision effect
                Particles_SpawnExplosion(s, Vector_Add(a->pos, Vector_Scale(Vector_Sub(b->pos, a->pos), 0.5f)), 
                               40, (a->radius + b->radius) / 100.0f, EXPLOSION_COLLISION, a->tex_idx);

                // Collision Resolution
                int sm_i = -1, bg_i = -1;
                if (a->radius < b->radius * 0.7f) { sm_i = i; bg_i = j; }
                else if (b->radius < a->radius * 0.7f) { sm_i = j; bg_i = i; }

                Vec2 normal = Vector_Normalize(Vector_Sub(b->pos, a->pos));

                if (sm_i != -1) {
                    // Small hits Large: Small is destroyed/reflected, Large takes damage
                    Asteroid *sm = &s->world.asteroids[sm_i];
                    Asteroid *bg = &s->world.asteroids[bg_i];
                    
                    Vec2 n_surf = (sm_i == i) ? Vector_Scale(normal, -1.0f) : normal;
                    Vec2 v_inc = sm->velocity;
                    if (Vector_Length(v_inc) < 10.0f) v_inc = Vector_Normalize(Vector_Sub(sm->pos, bg->pos));
                    
                    float dot = v_inc.x * n_surf.x + v_inc.y * n_surf.y;
                    Vec2 reflect = Vector_Sub(v_inc, Vector_Scale(n_surf, 2.0f * dot));
                    float reflect_angle = atan2f(reflect.y, reflect.x);

                    sm->active = false; s->world.asteroid_count--;
                    bg->radius *= 0.9f; bg->health *= 0.9f;
                    
                    // Spawn fragments from small asteroid
                    float cr = sm->radius * 0.5f;
                    if (cr >= ASTEROID_MIN_RADIUS) {
                        float sa = reflect_angle + ((float)(rand() % 100) / 100.0f - 0.5f) * 0.5f;
                        SpawnAsteroid(s, Vector_Add(sm->pos, (Vec2){cosf(sa)*cr, sinf(sa)*cr}), (Vec2){cosf(sa), sinf(sa)}, cr);
                    }
                    if (sm->radius > ASTEROID_COLLISION_SPLIT_THRESHOLD) {
                        float chr = ASTEROID_SPLIT_MIN_RADIUS * powf(sm->radius / ASTEROID_SPLIT_MIN_RADIUS, ASTEROID_SPLIT_EXPONENT);
                        if (chr >= ASTEROID_MIN_RADIUS) for (int k = 0; k < 2; k++) {
                            float ssa = reflect_angle + (k == 0 ? 0.3f : -0.3f) + ((float)(rand() % 100) / 100.0f - 0.5f) * 0.4f;
                            SpawnAsteroid(s, Vector_Add(sm->pos, (Vec2){cosf(ssa)*chr, sinf(ssa)*chr}), (Vec2){cosf(ssa), sinf(ssa)}, chr);
                        }
                    }
                } else {
                    // Similar sizes: Both destroyed
                    s->world.asteroids[i].active = false; s->world.asteroids[j].active = false; s->world.asteroid_count -= 2;
                    
                    // A reflects off B
                    Vec2 v_a = a->velocity;
                    Vec2 n_a = Vector_Scale(normal, -1.0f); 
                    float d_a = v_a.x * n_a.x + v_a.y * n_a.y;
                    Vec2 ref_a = Vector_Sub(v_a, Vector_Scale(n_a, 2.0f * d_a));
                    float ang_a = atan2f(ref_a.y, ref_a.x);

                    // B reflects off A
                    Vec2 v_b = b->velocity;
                    float d_b = v_b.x * normal.x + v_b.y * normal.y;
                    Vec2 ref_b = Vector_Sub(v_b, Vector_Scale(normal, 2.0f * d_b));
                    float ang_b = atan2f(ref_b.y, ref_b.x);

                    if (a->radius > ASTEROID_COLLISION_SPLIT_THRESHOLD) {
                        float chr = ASTEROID_SPLIT_MIN_RADIUS * powf(a->radius / ASTEROID_SPLIT_MIN_RADIUS, ASTEROID_SPLIT_EXPONENT);
                        if (chr >= ASTEROID_MIN_RADIUS) for (int k = 0; k < 2; k++) {
                            float sa = ang_a + (k == 0 ? 0.4f : -0.4f);
                            SpawnAsteroid(s, Vector_Add(a->pos, (Vec2){cosf(sa)*chr, sinf(sa)*chr}), (Vec2){cosf(sa), sinf(sa)}, chr);
                        }
                    }
                    if (b->radius > ASTEROID_COLLISION_SPLIT_THRESHOLD) {
                        float chr = ASTEROID_SPLIT_MIN_RADIUS * powf(b->radius / ASTEROID_SPLIT_MIN_RADIUS, ASTEROID_SPLIT_EXPONENT);
                        if (chr >= ASTEROID_MIN_RADIUS) for (int k = 0; k < 2; k++) {
                            float sa = ang_b + (k == 0 ? 0.4f : -0.4f);
                            SpawnAsteroid(s, Vector_Add(b->pos, (Vec2){cosf(sa)*chr, sinf(sa)*chr}), (Vec2){cosf(sa), sinf(sa)}, chr);
                        }
                    }
                }
            }
        }
    }

    // 2. Unit vs Asteroid
    for (int u = 0; u < MAX_UNITS; u++) {
        if (!s->world.units[u].active) continue;
        for (int i = 0; i < MAX_ASTEROIDS; i++) {
            if (!s->world.asteroids[i].active) continue;
            float dsq = Vector_DistanceSq(s->world.units[u].pos, s->world.asteroids[i].pos);
            float md = s->world.asteroids[i].radius * ASTEROID_HITBOX_MULT + s->world.units[u].stats->radius;
            
            if (dsq < md * md) {
                s->world.units[u].health -= s->world.asteroids[i].radius * ASTEROID_HITBOX_MULT;
                Particles_SpawnExplosion(s, s->world.asteroids[i].pos, 20, s->world.asteroids[i].radius / 100.0f, EXPLOSION_COLLISION, s->world.asteroids[i].tex_idx);
                s->world.asteroids[i].active = false; s->world.asteroid_count--;
                
                if (s->world.units[u].health <= 0) {
                    Particles_SpawnExplosion(s, s->world.units[u].pos, 100, 2.0f, EXPLOSION_COLLISION, s->world.asteroids[i].tex_idx);
                    if (s->world.units[u].type == UNIT_MOTHERSHIP) {
                        s->world.units[u].active = false; 
                        s->ui.respawn_timer = RESPAWN_TIME; 
                        s->ui.respawn_pos = s->world.units[u].pos;
                    } else {
                        s->world.units[u].active = false;
                    }
                }
                break; // One collision per unit per frame max
            }
        }
    }
}
