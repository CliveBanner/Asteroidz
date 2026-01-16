#include "particles.h"
#include "utils.h"
#include "constants.h"
#include "game.h"
#include <math.h>
#include <stdlib.h>

void Particles_SpawnExplosion(AppState *s, Vec2 pos, int count, float size_mult, ExplosionType type, int asteroid_tex_idx) {
  float capped_mult = powf(size_mult, 0.6f); 
  float count_mult = powf(size_mult, 0.35f);
  float chunky_mult = powf(size_mult, 0.75f);
  
  float a_theme = DeterministicHash(asteroid_tex_idx * 789, 123);
  SDL_Color base_col;
  if (a_theme > 0.7f) base_col = (SDL_Color){30, 18, 12, 255};
  else if (a_theme > 0.4f) base_col = (SDL_Color){15, 20, 35, 255};
  else base_col = (SDL_Color){18, 18, 22, 255};

  if (type == EXPLOSION_IMPACT) {
      int spark_count = (int)(count * 2.5f * count_mult); 
      for (int i = 0; i < spark_count; i++) {
        int idx = s->world.particle_next_idx;
        s->world.particles.active[idx] = true;
        s->world.particles.type[idx] = PARTICLE_SPARK;
        s->world.particles.pos[idx] = pos;
        float angle = (float)(rand() % 360) * 0.0174533f;
        float speed = (float)(rand() % 800 + 400) * capped_mult; 
        s->world.particles.velocity[idx].x = cosf(angle) * speed;
        s->world.particles.velocity[idx].y = sinf(angle) * speed;
        s->world.particles.life[idx] = PARTICLE_LIFE_BASE * 0.5f;
        s->world.particles.size[idx] = (float)(rand() % 12 + 6) * capped_mult;
        s->world.particles.color[idx] = (SDL_Color){(Uint8)((base_col.r + 255)/2), (Uint8)((base_col.g + 220)/2), (Uint8)((base_col.b + 150)/2), 255};
        s->world.particle_next_idx = (s->world.particle_next_idx + 1) % MAX_PARTICLES;
      }
      int puff_count = (int)(count * 1.2f * count_mult); 
      for (int i = 0; i < puff_count; i++) {
        int idx = s->world.particle_next_idx;
        s->world.particles.active[idx] = true;
        s->world.particles.type[idx] = PARTICLE_PUFF;
        s->world.particles.pos[idx] = pos;
        float angle = (float)(rand() % 360) * 0.0174533f;
        float speed = (float)(rand() % 200 + 80) * capped_mult; 
        s->world.particles.velocity[idx].x = cosf(angle) * speed;
        s->world.particles.velocity[idx].y = sinf(angle) * speed;
        s->world.particles.life[idx] = PARTICLE_LIFE_BASE * 0.5f; 
        s->world.particles.size[idx] = (float)(rand() % 120 + 60) * capped_mult;
        Uint8 v = (Uint8)(rand() % 40 + 80); 
        s->world.particles.color[idx] = (SDL_Color){v, (Uint8)(v * 0.8f), (Uint8)(v * 0.6f), 255};
        s->world.particle_next_idx = (s->world.particle_next_idx + 1) % MAX_PARTICLES;
      }
      int fine_debris_count = (int)(10 * count_mult); 
      for (int i = 0; i < fine_debris_count; i++) {
          int idx = s->world.particle_next_idx;
          s->world.particles.active[idx] = true;
          s->world.particles.type[idx] = PARTICLE_DEBRIS;
          s->world.particles.asteroid_tex_idx[idx] = asteroid_tex_idx;
          s->world.particles.pos[idx] = pos;
          s->world.particles.tex_idx[idx] = rand() % DEBRIS_COUNT;
          float angle = (float)(rand() % 360) * 0.0174533f;
          float speed = (float)(rand() % 300 + 100) * capped_mult; 
          s->world.particles.velocity[idx].x = cosf(angle) * speed;
          s->world.particles.velocity[idx].y = sinf(angle) * speed;
          s->world.particles.life[idx] = PARTICLE_LIFE_BASE * 0.35f; 
          s->world.particles.size[idx] = (float)(rand() % 18 + 12) * chunky_mult;
          s->world.particles.rotation[idx] = (float)(rand() % 360);
          s->world.particles.color[idx] = base_col;
          s->world.particle_next_idx = (s->world.particle_next_idx + 1) % MAX_PARTICLES;
      }
      int debris_count = (int)(3 * count_mult); 
      for (int i = 0; i < debris_count; i++) {
          int idx = s->world.particle_next_idx;
          s->world.particles.active[idx] = true;
          s->world.particles.type[idx] = PARTICLE_DEBRIS;
          s->world.particles.asteroid_tex_idx[idx] = asteroid_tex_idx;
          s->world.particles.pos[idx] = pos;
          s->world.particles.tex_idx[idx] = rand() % DEBRIS_COUNT;
          float angle = (float)(rand() % 360) * 0.0174533f;
          float speed = (float)(rand() % 150 + 60) * capped_mult; 
          s->world.particles.velocity[idx].x = cosf(angle) * speed;
          s->world.particles.velocity[idx].y = sinf(angle) * speed;
          s->world.particles.life[idx] = PARTICLE_LIFE_BASE * 0.45f; 
          s->world.particles.size[idx] = (float)(rand() % 45 + 35) * chunky_mult;
          s->world.particles.rotation[idx] = (float)(rand() % 360);
          s->world.particles.color[idx] = base_col;
          s->world.particle_next_idx = (s->world.particle_next_idx + 1) % MAX_PARTICLES;
      }
      int sw_idx = s->world.particle_next_idx;
      s->world.particles.active[sw_idx] = true;
      s->world.particles.type[sw_idx] = PARTICLE_SHOCKWAVE;
      s->world.particles.pos[sw_idx] = pos;
      s->world.particles.velocity[sw_idx] = (Vec2){0,0};
      s->world.particles.life[sw_idx] = 0.3f; 
      s->world.particles.size[sw_idx] = 55.0f * capped_mult;
      s->world.particles.color[sw_idx] = (SDL_Color){255, 255, 255, 200}; 
      s->world.particle_next_idx = (s->world.particle_next_idx + 1) % MAX_PARTICLES;
  } else {
      int puff_count = (int)(count * 0.5f * count_mult); 
      for (int i = 0; i < puff_count; i++) {
        int idx = s->world.particle_next_idx;
        s->world.particles.active[idx] = true;
        s->world.particles.type[idx] = PARTICLE_PUFF;
        s->world.particles.pos[idx] = pos;
        float angle = (float)(rand() % 360) * 0.0174533f;
        float speed = (float)(rand() % 150 + 20) * capped_mult; 
        s->world.particles.velocity[idx].x = cosf(angle) * speed;
        s->world.particles.velocity[idx].y = sinf(angle) * speed;
        s->world.particles.life[idx] = PARTICLE_LIFE_BASE * (0.8f + 0.4f * (float)rand()/(float)RAND_MAX);
        s->world.particles.size[idx] = (float)(rand() % 120 + 60) * capped_mult;
        Uint8 v = (Uint8)(rand() % 30 + 40); 
        s->world.particles.color[idx] = (SDL_Color){v, (Uint8)(v * 0.95f), (Uint8)(v * 0.9f), 255};
        s->world.particle_next_idx = (s->world.particle_next_idx + 1) % MAX_PARTICLES;
      }
      int debris_count = (int)(5 * count_mult);
      for (int i = 0; i < debris_count; i++) {
          int idx = s->world.particle_next_idx;
          s->world.particles.active[idx] = true;
          s->world.particles.type[idx] = PARTICLE_DEBRIS;
          s->world.particles.asteroid_tex_idx[idx] = asteroid_tex_idx;
          s->world.particles.pos[idx] = pos;
          s->world.particles.tex_idx[idx] = rand() % DEBRIS_COUNT;
          float angle = (float)(rand() % 360) * 0.0174533f;
          float speed = (float)(rand() % 100 + 40) * capped_mult; 
          s->world.particles.velocity[idx].x = cosf(angle) * speed;
          s->world.particles.velocity[idx].y = sinf(angle) * speed;
          s->world.particles.life[idx] = PARTICLE_LIFE_BASE * 0.7f; 
          s->world.particles.size[idx] = (float)(rand() % 40 + 20) * chunky_mult;
          s->world.particles.rotation[idx] = (float)(rand() % 360);
          s->world.particles.color[idx] = base_col;
          s->world.particle_next_idx = (s->world.particle_next_idx + 1) % MAX_PARTICLES;
      }
      int sw_idx = s->world.particle_next_idx;
      s->world.particles.active[sw_idx] = true;
      s->world.particles.type[sw_idx] = PARTICLE_SHOCKWAVE;
      s->world.particles.pos[sw_idx] = pos;
      s->world.particles.velocity[sw_idx] = (Vec2){0,0};
      s->world.particles.life[sw_idx] = 0.5f;
      s->world.particles.size[sw_idx] = (float)(40.0f * capped_mult); 
      s->world.particles.color[sw_idx] = (SDL_Color){255, 255, 255, 100};
      s->world.particle_next_idx = (s->world.particle_next_idx + 1) % MAX_PARTICLES;
  }
}

void Particles_SpawnLaserFlash(AppState *s, Vec2 pos, float size, bool is_impact) {
    int m_idx = s->world.particle_next_idx;
    s->world.particles.active[m_idx] = true;
    s->world.particles.type[m_idx] = PARTICLE_GLOW; 
    s->world.particles.pos[m_idx] = pos;
    s->world.particles.velocity[m_idx] = (Vec2){0, 0};
    s->world.particles.life[m_idx] = MUZZLE_FLASH_LIFE;
    s->world.particles.size[m_idx] = size * MUZZLE_FLASH_SIZE_MULT;
    s->world.particles.color[m_idx] = (SDL_Color)COLOR_MUZZLE_FLASH;
    s->world.particle_next_idx = (s->world.particle_next_idx + 1) % MAX_PARTICLES;

    if (is_impact) {
        int i_g_idx = s->world.particle_next_idx;
        s->world.particles.active[i_g_idx] = true;
        s->world.particles.type[i_g_idx] = PARTICLE_GLOW;
        s->world.particles.pos[i_g_idx] = pos;
        s->world.particles.velocity[i_g_idx] = (Vec2){0, 0};
        s->world.particles.life[i_g_idx] = 0.25f;
        s->world.particles.size[i_g_idx] = size * 12.0f;
        s->world.particles.color[i_g_idx] = (SDL_Color){255, 255, 255, 255};
        s->world.particle_next_idx = (s->world.particle_next_idx + 1) % MAX_PARTICLES;
    }
}

void Particles_Update(AppState *s, float dt) {
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (!s->world.particles.active[i]) continue;
    if (s->world.particles.type[i] == PARTICLE_TRACER) s->world.particles.life[i] -= dt * 2.0f;
    else if (s->world.particles.type[i] == PARTICLE_SHOCKWAVE) {
        s->world.particles.life[i] -= dt * 2.0f;
        s->world.particles.size[i] += dt * 1200.0f;
    }
    else { 
        s->world.particles.pos[i].x += s->world.particles.velocity[i].x * dt;
        s->world.particles.pos[i].y += s->world.particles.velocity[i].y * dt;
        s->world.particles.life[i] -= dt * PARTICLE_LIFE_DECAY; 
    }
    if (s->world.particles.life[i] <= 0) s->world.particles.active[i] = false;
  }
}
