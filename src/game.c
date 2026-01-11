#include "game.h"
#include "constants.h"
#include <math.h>
#include <stdlib.h>

static void SpawnAsteroid(AppState *s, Vec2 pos, Vec2 vel_dir, float radius) {
    if (s->asteroid_count >= MAX_ASTEROIDS) return;
    
    // Find an inactive slot
    for (int i = 0; i < MAX_ASTEROIDS; i++) {
        if (!s->asteroids[i].active) {
            s->asteroids[i].pos = pos;
            
            // Speed is inversely proportional to radius
            // Base speed factor of 15000 (15000/200 = 75 units/s, 15000/30 = 500 units/s)
            float speed = 15000.0f / radius;
            s->asteroids[i].velocity.x = vel_dir.x * speed;
            s->asteroids[i].velocity.y = vel_dir.y * speed;

            s->asteroids[i].radius = radius;
            s->asteroids[i].rotation = (float)(rand() % 360);
            s->asteroids[i].rot_speed = ((float)(rand() % 100) / 50.0f - 1.0f) * (400.0f / radius);
            s->asteroids[i].tex_idx = rand() % ASTEROID_TYPE_COUNT;
            s->asteroids[i].active = true;
            s->asteroid_count++;
            break;
        }
    }
}

static void SpawnExplosion(AppState *s, Vec2 pos, int count, float size_mult) {
    // 1. Spawn Puffs (Smoke/Dust)
    int puff_count = count / 4;
    for (int i = 0; i < puff_count; i++) {
        int idx = s->particle_next_idx;
        s->particles[idx].active = true;
        s->particles[idx].type = PARTICLE_PUFF;
        s->particles[idx].pos = pos;
        
        float angle = (float)(rand() % 360) * 0.0174533f;
        float speed = (float)(rand() % 100 + 20) * size_mult;
        s->particles[idx].velocity.x = cosf(angle) * speed;
        s->particles[idx].velocity.y = sinf(angle) * speed;
        
        s->particles[idx].life = 1.0f;
        s->particles[idx].size = (float)(rand() % 100 + 50) * size_mult;
        
        // Grey/Brownish dust colors
        Uint8 v = (Uint8)(rand() % 50 + 100);
        s->particles[idx].color = (SDL_Color){v, (Uint8)(v * 0.9f), (Uint8)(v * 0.8f), 255};
        
        s->particle_next_idx = (s->particle_next_idx + 1) % MAX_PARTICLES;
    }

    // 2. Spawn Sparks (Fire/Rock)
    for (int i = 0; i < count; i++) {
        int idx = s->particle_next_idx;
        s->particles[idx].active = true;
        s->particles[idx].type = PARTICLE_SPARK;
        s->particles[idx].pos = pos;
        
        float angle = (float)(rand() % 360) * 0.0174533f;
        float speed = (float)(rand() % 300 + 100) * size_mult;
        s->particles[idx].velocity.x = cosf(angle) * speed;
        s->particles[idx].velocity.y = sinf(angle) * speed;
        
        s->particles[idx].life = 1.0f;
        s->particles[idx].size = (float)(rand() % 6 + 2) * size_mult;
        
        if (rand() % 2 == 0) {
            s->particles[idx].color = (SDL_Color){255, (Uint8)(rand()%100+150), 50, 255}; // Fire
        } else {
            s->particles[idx].color = (SDL_Color){(Uint8)(rand()%50+150), (Uint8)(rand()%50+150), (Uint8)(rand()%50+150), 255}; // Rock
        }
        
        s->particle_next_idx = (s->particle_next_idx + 1) % MAX_PARTICLES;
    }
}

void Game_Init(AppState *s) {
    // Initial spawn of many large asteroids over a wide area
    for (int i = 0; i < 150; i++) {
        Vec2 pos = { (float)(rand() % 20000 - 10000), (float)(rand() % 20000 - 10000) };
        // Generate a random direction vector
        float angle = (float)(rand() % 360) * 0.0174533f;
        Vec2 dir = { cosf(angle), sinf(angle) };
        SpawnAsteroid(s, pos, dir, 80.0f + (rand() % 120));
    }
}

void Game_Update(AppState *s, float dt) {
    // 1. Camera Movement (Edge Scroll)
    float move_speed = CAMERA_SPEED / s->zoom;
    int win_w, win_h; SDL_GetRenderOutputSize(s->renderer, &win_w, &win_h);
    if (s->mouse_pos.x < EDGE_SCROLL_THRESHOLD) s->camera_pos.x -= move_speed * dt;
    if (s->mouse_pos.x > (win_w - EDGE_SCROLL_THRESHOLD)) s->camera_pos.x += move_speed * dt;
    if (s->mouse_pos.y < EDGE_SCROLL_THRESHOLD) s->camera_pos.y -= move_speed * dt;
    if (s->mouse_pos.y > (win_h - EDGE_SCROLL_THRESHOLD)) s->camera_pos.y += move_speed * dt;

    // 2. Asteroid Movement
    for (int i = 0; i < MAX_ASTEROIDS; i++) {
        if (!s->asteroids[i].active) continue;
        s->asteroids[i].pos.x += s->asteroids[i].velocity.x * dt;
        s->asteroids[i].pos.y += s->asteroids[i].velocity.y * dt;
        s->asteroids[i].rotation += s->asteroids[i].rot_speed * dt;
    }

    // 3. Particle Update
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!s->particles[i].active) continue;
        s->particles[i].pos.x += s->particles[i].velocity.x * dt;
        s->particles[i].pos.y += s->particles[i].velocity.y * dt;
        s->particles[i].life -= dt * 1.5f; // Faster fade
        if (s->particles[i].life <= 0) s->particles[i].active = false;
    }

    // 4. Collision and Splitting
    for (int i = 0; i < MAX_ASTEROIDS; i++) {
        if (!s->asteroids[i].active) continue;
        for (int j = i + 1; j < MAX_ASTEROIDS; j++) {
            if (!s->asteroids[j].active) continue;

            float dx = s->asteroids[j].pos.x - s->asteroids[i].pos.x;
            float dy = s->asteroids[j].pos.y - s->asteroids[i].pos.y;
            float dist_sq = dx*dx + dy*dy;
            
            float r1 = s->asteroids[i].radius * 0.5f;
            float r2 = s->asteroids[j].radius * 0.5f;
            float min_dist = r1 + r2;

            if (dist_sq < min_dist * min_dist) {
                Asteroid a = s->asteroids[i];
                Asteroid b = s->asteroids[j];
                
                s->asteroids[i].active = false;
                s->asteroids[j].active = false;
                s->asteroid_count -= 2;

                // Spawn Explosion
                Vec2 contact_point = { a.pos.x + dx*0.5f, a.pos.y + dy*0.5f };
                SpawnExplosion(s, contact_point, 40, (a.radius + b.radius) / 100.0f);

                // Calculate collision angle (from a to b)
                float collision_angle = atan2f(dy, dx);

                if (a.radius > 30.0f) {
                    float child_rad = a.radius * 0.6f;
                    // Spawn 2 pieces of A perpendicular to the collision axis
                    for (int k = 0; k < 2; k++) {
                        float split_angle = (k == 0) ? (collision_angle + 1.5708f) : (collision_angle - 1.5708f);
                        Vec2 dir = { cosf(split_angle), sinf(split_angle) };
                        // Spawn at an offset to prevent instant re-collision
                        Vec2 spawn_pos = { a.pos.x + dir.x * child_rad, a.pos.y + dir.y * child_rad };
                        SpawnAsteroid(s, spawn_pos, dir, child_rad);
                    }
                }
                if (b.radius > 30.0f) {
                    float child_rad = b.radius * 0.6f;
                    // Spawn 2 pieces of B perpendicular to the collision axis
                    for (int k = 0; k < 2; k++) {
                        float split_angle = (k == 0) ? (collision_angle + 1.5708f) : (collision_angle - 1.5708f);
                        Vec2 dir = { cosf(split_angle), sinf(split_angle) };
                        // Offset from B's center
                        Vec2 spawn_pos = { b.pos.x + dir.x * child_rad, b.pos.y * dir.y * child_rad };
                        // Pieces of B move in the opposite general direction of A's pieces? 
                        // No, let's keep it simple: split along the same axis but from B's center.
                        SpawnAsteroid(s, spawn_pos, dir, child_rad);
                    }
                }
                break;
            }
        }
    }
}