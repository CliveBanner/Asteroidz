#include "game.h"
#include "constants.h"
#include <math.h>
#include <stdlib.h>

static void SpawnAsteroid(AppState *s, Vec2 pos, Vec2 vel, float radius) {
    if (s->asteroid_count >= MAX_ASTEROIDS) return;
    
    // Find an inactive slot
    for (int i = 0; i < MAX_ASTEROIDS; i++) {
        if (!s->asteroids[i].active) {
            s->asteroids[i].pos = pos;
            s->asteroids[i].velocity = vel;
            s->asteroids[i].radius = radius;
            s->asteroids[i].rotation = (float)(rand() % 360);
            s->asteroids[i].rot_speed = ((float)(rand() % 100) / 50.0f - 1.0f) * 20.0f;
            s->asteroids[i].tex_idx = rand() % ASTEROID_TYPE_COUNT;
            s->asteroids[i].active = true;
            s->asteroid_count++;
            break;
        }
    }
}

void Game_Init(AppState *s) {
    // Initial spawn of many large asteroids over a wide area
    for (int i = 0; i < 150; i++) {
        Vec2 pos = { (float)(rand() % 20000 - 10000), (float)(rand() % 20000 - 10000) };
        Vec2 vel = { (float)(rand() % 100 - 50), (float)(rand() % 100 - 50) };
        SpawnAsteroid(s, pos, vel, 80.0f + (rand() % 120));
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

    // 3. Collision and Splitting
    for (int i = 0; i < MAX_ASTEROIDS; i++) {
        if (!s->asteroids[i].active) continue;
        for (int j = i + 1; j < MAX_ASTEROIDS; j++) {
            if (!s->asteroids[j].active) continue;

            float dx = s->asteroids[j].pos.x - s->asteroids[i].pos.x;
            float dy = s->asteroids[j].pos.y - s->asteroids[i].pos.y;
            float dist_sq = dx*dx + dy*dy;
            
            // Tight hitbox: only collide at 50% of visual radius
            float r1 = s->asteroids[i].radius * 0.5f;
            float r2 = s->asteroids[j].radius * 0.5f;
            float min_dist = r1 + r2;

            if (dist_sq < min_dist * min_dist) {
                // COLLISION! 
                Asteroid a = s->asteroids[i];
                Asteroid b = s->asteroids[j];
                
                s->asteroids[i].active = false;
                s->asteroids[j].active = false;
                s->asteroid_count -= 2;

                // Spawn smaller pieces if they were big enough
                if (a.radius > 30.0f) {
                    for (int k = 0; k < 2; k++) {
                        Vec2 off = { (float)(rand()%40-20), (float)(rand()%40-20) };
                        Vec2 v = { a.velocity.x + (rand()%60-30), a.velocity.y + (rand()%60-30) };
                        SpawnAsteroid(s, (Vec2){a.pos.x + off.x, a.pos.y + off.y}, v, a.radius * 0.6f);
                    }
                }
                if (b.radius > 30.0f) {
                    for (int k = 0; k < 2; k++) {
                        Vec2 off = { (float)(rand()%40-20), (float)(rand()%40-20) };
                        Vec2 v = { b.velocity.x + (rand()%60-30), b.velocity.y + (rand()%60-30) };
                        SpawnAsteroid(s, (Vec2){b.pos.x + off.x, b.pos.y + off.y}, v, b.radius * 0.6f);
                    }
                }
                break; // Break j loop as i is now gone
            }
        }
    }
}
