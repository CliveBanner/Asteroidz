#include "particles.h"
#include "utils.h"
#include "constants.h"
#include "game.h"
#include <math.h>
#include <stdlib.h>

void Particles_SpawnExplosion(AppState *s, Vec2 pos, int count, float size_mult, ExplosionType type, int asteroid_tex_idx) {
  // Balanced scaling
  float capped_mult = powf(size_mult, 0.6f); 
  float count_mult = powf(size_mult, 0.35f);   // Fewer pieces for large asteroids
  float chunky_mult = powf(size_mult, 0.75f);  // Reduced from 0.95f
  
  // Theme color lookup - Even darker for a solid rocky look
  float a_theme = DeterministicHash(asteroid_tex_idx * 789, 123);
  SDL_Color base_col;
  if (a_theme > 0.7f) base_col = (SDL_Color){30, 18, 12, 255};      // Deep Red
  else if (a_theme > 0.4f) base_col = (SDL_Color){15, 20, 35, 255}; // Deep Blue
  else base_col = (SDL_Color){18, 18, 22, 255};                      // Deep Grey

  if (type == EXPLOSION_IMPACT) {
      // Fast, sharp sparks
      int spark_count = (int)(count * 2.5f * count_mult); 
      for (int i = 0; i < spark_count; i++) {
        int idx = s->world.particle_next_idx;
        s->world.particles[idx].active = true;
        s->world.particles[idx].type = PARTICLE_SPARK;
        s->world.particles[idx].pos = pos;
        float angle = (float)(rand() % 360) * 0.0174533f;
        float speed = (float)(rand() % 800 + 400) * capped_mult; 
        s->world.particles[idx].velocity.x = cosf(angle) * speed;
        s->world.particles[idx].velocity.y = sinf(angle) * speed;
        s->world.particles[idx].life = PARTICLE_LIFE_BASE * 0.5f;
        s->world.particles[idx].size = (float)(rand() % 12 + 6) * capped_mult; // Increased from 8+4
        s->world.particles[idx].color = (SDL_Color){(Uint8)((base_col.r + 255)/2), (Uint8)((base_col.g + 220)/2), (Uint8)((base_col.b + 150)/2), 255};
        s->world.particle_next_idx = (s->world.particle_next_idx + 1) % MAX_PARTICLES;
      }
      // Small puffs
      int puff_count = (int)(count * 1.2f * count_mult); 
      for (int i = 0; i < puff_count; i++) {
        int idx = s->world.particle_next_idx;
        s->world.particles[idx].active = true;
        s->world.particles[idx].type = PARTICLE_PUFF;
        s->world.particles[idx].pos = pos;
        float angle = (float)(rand() % 360) * 0.0174533f;
        float speed = (float)(rand() % 200 + 80) * capped_mult; 
        s->world.particles[idx].velocity.x = cosf(angle) * speed;
        s->world.particles[idx].velocity.y = sinf(angle) * speed;
        s->world.particles[idx].life = PARTICLE_LIFE_BASE * 0.5f; 
        s->world.particles[idx].size = (float)(rand() % 120 + 60) * capped_mult; // Increased from 80+40
        Uint8 v = (Uint8)(rand() % 40 + 80); 
        s->world.particles[idx].color = (SDL_Color){v, (Uint8)(v * 0.8f), (Uint8)(v * 0.6f), 255};
        s->world.particle_next_idx = (s->world.particle_next_idx + 1) % MAX_PARTICLES;
      }
      
      // Fine Debris
      int fine_debris_count = (int)(10 * count_mult); 
      for (int i = 0; i < fine_debris_count; i++) {
          int idx = s->world.particle_next_idx;
          s->world.particles[idx].active = true;
          s->world.particles[idx].type = PARTICLE_DEBRIS;
          s->world.particles[idx].asteroid_tex_idx = asteroid_tex_idx;
          s->world.particles[idx].pos = pos;
          s->world.particles[idx].tex_idx = rand() % DEBRIS_COUNT;
          float angle = (float)(rand() % 360) * 0.0174533f;
          float speed = (float)(rand() % 300 + 100) * capped_mult; 
          s->world.particles[idx].velocity.x = cosf(angle) * speed;
          s->world.particles[idx].velocity.y = sinf(angle) * speed;
          s->world.particles[idx].life = PARTICLE_LIFE_BASE * 0.35f; 
          s->world.particles[idx].size = (float)(rand() % 18 + 12) * chunky_mult; // Increased from 12+8
          s->world.particles[idx].rotation = (float)(rand() % 360);
          s->world.particles[idx].color = base_col;
          s->world.particle_next_idx = (s->world.particle_next_idx + 1) % MAX_PARTICLES;
      }

      // Main Debris
      int debris_count = (int)(3 * count_mult); 
      for (int i = 0; i < debris_count; i++) {
          int idx = s->world.particle_next_idx;
          s->world.particles[idx].active = true;
          s->world.particles[idx].type = PARTICLE_DEBRIS;
          s->world.particles[idx].asteroid_tex_idx = asteroid_tex_idx;
          s->world.particles[idx].pos = pos;
          s->world.particles[idx].tex_idx = rand() % DEBRIS_COUNT;
          float angle = (float)(rand() % 360) * 0.0174533f;
          float speed = (float)(rand() % 150 + 60) * capped_mult; 
          s->world.particles[idx].velocity.x = cosf(angle) * speed;
          s->world.particles[idx].velocity.y = sinf(angle) * speed;
          s->world.particles[idx].life = PARTICLE_LIFE_BASE * 0.45f; 
          s->world.particles[idx].size = (float)(rand() % 45 + 35) * chunky_mult; // Increased from 30+20
          s->world.particles[idx].rotation = (float)(rand() % 360);
          s->world.particles[idx].color = base_col;
          s->world.particle_next_idx = (s->world.particle_next_idx + 1) % MAX_PARTICLES;
      }

      // Energetic shockwave for impact
      int sw_idx = s->world.particle_next_idx;
      s->world.particles[sw_idx].active = true;
      s->world.particles[sw_idx].type = PARTICLE_SHOCKWAVE;
      s->world.particles[sw_idx].pos = pos;
      s->world.particles[sw_idx].velocity = (Vec2){0,0};
      s->world.particles[sw_idx].life = 0.3f; 
      s->world.particles[sw_idx].size = 55.0f * capped_mult; // Increased from 40
      s->world.particles[sw_idx].color = (SDL_Color){255, 255, 255, 200}; 
      s->world.particle_next_idx = (s->world.particle_next_idx + 1) % MAX_PARTICLES;


  } else {
      // EXPLOSION_COLLISION
      int puff_count = (int)(count * 0.5f * count_mult); 
      for (int i = 0; i < puff_count; i++) {
        int idx = s->world.particle_next_idx;
        s->world.particles[idx].active = true;
        s->world.particles[idx].type = PARTICLE_PUFF;
        s->world.particles[idx].pos = pos;
        float angle = (float)(rand() % 360) * 0.0174533f;
        float speed = (float)(rand() % 150 + 20) * capped_mult; 
        s->world.particles[idx].velocity.x = cosf(angle) * speed;
        s->world.particles[idx].velocity.y = sinf(angle) * speed;
        s->world.particles[idx].life = PARTICLE_LIFE_BASE * (0.8f + 0.4f * (float)rand()/(float)RAND_MAX); // Middle ground
        s->world.particles[idx].size = (float)(rand() % 120 + 60) * capped_mult; // Slightly smaller
        Uint8 v = (Uint8)(rand() % 30 + 40); // Dark smoke
        s->world.particles[idx].color = (SDL_Color){v, (Uint8)(v * 0.95f), (Uint8)(v * 0.9f), 255};
        s->world.particle_next_idx = (s->world.particle_next_idx + 1) % MAX_PARTICLES;
      }

      int debris_count = (int)(5 * count_mult); // Reduced from 10
      for (int i = 0; i < debris_count; i++) {
          int idx = s->world.particle_next_idx;
          s->world.particles[idx].active = true;
          s->world.particles[idx].type = PARTICLE_DEBRIS;
          s->world.particles[idx].asteroid_tex_idx = asteroid_tex_idx;
          s->world.particles[idx].pos = pos;
          s->world.particles[idx].tex_idx = rand() % DEBRIS_COUNT;
          float angle = (float)(rand() % 360) * 0.0174533f;
          float speed = (float)(rand() % 100 + 40) * capped_mult; 
          s->world.particles[idx].velocity.x = cosf(angle) * speed;
          s->world.particles[idx].velocity.y = sinf(angle) * speed;
          s->world.particles[idx].life = PARTICLE_LIFE_BASE * 0.7f; 
          s->world.particles[idx].size = (float)(rand() % 40 + 20) * chunky_mult; // Toned down
          s->world.particles[idx].rotation = (float)(rand() % 360);
          s->world.particles[idx].color = base_col;
          s->world.particle_next_idx = (s->world.particle_next_idx + 1) % MAX_PARTICLES;
      }
      
      // Shockwave (Toned down size)
      int sw_idx = s->world.particle_next_idx;
      s->world.particles[sw_idx].active = true;
      s->world.particles[sw_idx].type = PARTICLE_SHOCKWAVE;
      s->world.particles[sw_idx].pos = pos;
      s->world.particles[sw_idx].velocity = (Vec2){0,0};
      s->world.particles[sw_idx].life = 0.5f;
      // Reduced initial size from 60.0f
      s->world.particles[sw_idx].size = (float)(40.0f * capped_mult); 
      s->world.particles[sw_idx].color = (SDL_Color){255, 255, 255, 100};
      s->world.particle_next_idx = (s->world.particle_next_idx + 1) % MAX_PARTICLES;
  }
}

void Particles_SpawnLaserFlash(AppState *s, Vec2 pos, float size, bool is_impact) {
    // 1. Primary Energy Flash
    int m_idx = s->world.particle_next_idx;
    s->world.particles[m_idx].active = true;
    s->world.particles[m_idx].type = PARTICLE_GLOW; 
    s->world.particles[m_idx].pos = pos;
    s->world.particles[m_idx].velocity = (Vec2){0, 0};
    s->world.particles[m_idx].life = MUZZLE_FLASH_LIFE;
    s->world.particles[m_idx].size = size * MUZZLE_FLASH_SIZE_MULT;
    s->world.particles[m_idx].color = (SDL_Color)COLOR_MUZZLE_FLASH;
    s->world.particle_next_idx = (s->world.particle_next_idx + 1) % MAX_PARTICLES;

    if (is_impact) {
        // 2. Extra Bright Intense Core for Impacts
        int i_g_idx = s->world.particle_next_idx;
        s->world.particles[i_g_idx].active = true;
        s->world.particles[i_g_idx].type = PARTICLE_GLOW;
        s->world.particles[i_g_idx].pos = pos;
        s->world.particles[i_g_idx].velocity = (Vec2){0, 0};
        s->world.particles[i_g_idx].life = 0.25f;
        s->world.particles[i_g_idx].size = size * 12.0f; // Boosted core
        s->world.particles[i_g_idx].color = (SDL_Color){255, 255, 255, 255}; // White hot
        s->world.particle_next_idx = (s->world.particle_next_idx + 1) % MAX_PARTICLES;
    }
}

void Particles_Update(AppState *s, float dt) {
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (!s->world.particles[i].active) continue;
    if (s->world.particles[i].type == PARTICLE_TRACER) s->world.particles[i].life -= dt * 2.0f;
    else if (s->world.particles[i].type == PARTICLE_SHOCKWAVE) {
        s->world.particles[i].life -= dt * 2.0f;
        s->world.particles[i].size += dt * 1200.0f; // Slower expansion
    }
    else { s->world.particles[i].pos = Vector_Add(s->world.particles[i].pos, Vector_Scale(s->world.particles[i].velocity, dt)); s->world.particles[i].life -= dt * PARTICLE_LIFE_DECAY; }
    if (s->world.particles[i].life <= 0) s->world.particles[i].active = false;
  }
}