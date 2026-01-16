#include "assets.h"
#include "constants.h"
#include "utils.h"
#include <math.h>
#include <stdlib.h>

#define QUANTIZE(c) ((c) < 5 ? 0 : ((c) & 0xE0)) 

static uint8_t ApplyContrast(uint8_t c) {
    float f = (float)c / 255.0f;
    f = (f < 0.5f) ? powf(f, 0.7f) : 1.0f - powf(1.0f - f, 1.3f);
    return (uint8_t)fminf(255.0f, f * 255.0f);
}

void DrawPlanetToBuffer(Uint32 *pixels, int size, float seed) {
  int center = size / 2; float radius = size * 0.45f;
  float theme = DeterministicHash((int)(seed * 1000), 42);
  float r1, g1, b1, r2, g2, b2;
  if (theme > 0.7f) { r1 = 0.8f; g1 = 0.3f; b1 = 0.2f; r2 = 0.4f; g2 = 0.1f; b2 = 0.1f; }
  else if (theme > 0.4f) { r1 = 0.2f; g1 = 0.5f; b1 = 0.8f; r2 = 0.1f; g2 = 0.2f; b2 = 0.4f; }
  else { r1 = 0.4f; g1 = 0.6f; b1 = 0.3f; r2 = 0.2f; g2 = 0.3f; b2 = 0.1f; }
  for (int y = 0; y < size; y++) {
    for (int x = 0; x < size; x++) {
      float dx = (float)(x - center), dy = (float)(y - center);
      float dist = sqrtf(dx * dx + dy * dy);
      if (dist <= radius) {
        float noise = 0.0f, freq = 0.02f, amp = 1.0f;
        for (int o = 0; o < 4; o++) { noise += PerlinNoise2D((float)x * freq + seed, (float)y * freq + seed * 2.0f) * amp; amp *= 0.35f; freq *= 2.0f; }
        float dot = (dx / radius * 0.5f + 0.5f) * (dy / radius * 0.5f + 0.5f);
        float final_val = fminf(1.0f, noise * 0.8f + dot * 0.4f);
        float mix_val = SDL_clamp(noise * 1.5f + 0.2f, 0.0f, 1.0f);
        Uint8 r = (Uint8)(255 * (r1 * mix_val + r2 * (1.0f - mix_val)) * final_val);
        Uint8 g = (Uint8)(255 * (g1 * mix_val + g2 * (1.0f - mix_val)) * final_val);
        Uint8 b = (Uint8)(255 * (b1 * mix_val + b2 * (1.0f - mix_val)) * final_val);
        pixels[y * size + x] = (255 << 24) | (b << 16) | (g << 8) | r;
      }
    }
  }
}

void DrawGalaxyToBuffer(Uint32 *pixels, int size, float seed) {
  int center = size / 2; float max_rad = size * 0.48f;
  float theme = DeterministicHash((int)(seed * 1000), 88);
  float cr, cg, cb;
  if (theme > 0.6f) { cr = 1.0f; cg = 0.4f; cb = 0.8f; } else if (theme > 0.3f) { cr = 0.4f; cg = 0.6f; cb = 1.0f; } else { cr = 1.0f; cg = 0.8f; cb = 0.4f; }
  for (int y = 0; y < size; y++) {
    for (int x = 0; x < size; x++) {
      float dx = (float)(x - center), dy = (float)(y - center);
      float dist = sqrtf(dx * dx + dy * dy); if (dist > max_rad) continue;
      float angle = atan2f(dy, dx);
      float arms = sinf(angle * 3.0f + powf(dist / max_rad, 0.5f) * 8.0f);
      float arm_noise = PerlinNoise2D(x * 0.05f + seed, y * 0.05f + seed);
      float final_val = powf(1.0f - (dist / max_rad), 2.0f) * (0.4f + 0.6f * arms) * arm_noise;
      if (final_val > 0.05f) {
        float core = powf(1.0f - fminf(1.0f, dist / (max_rad * 0.2f)), 2.0f);
        float falloff = 1.0f - (dist / max_rad);
        float norm_dist = dist / max_rad;
        float saturation = fminf(1.0f, norm_dist * 8.0f);
        cr = cr * saturation + (1.0f - saturation); cg = cg * saturation + (1.0f - saturation); cb = cb * saturation + (1.0f - saturation);
        Uint8 rv = QUANTIZE(ApplyContrast((Uint8)fminf(255, final_val * (cr * 150 + core * 255))));
        Uint8 gv = QUANTIZE(ApplyContrast((Uint8)fminf(255, final_val * (cg * 150 + core * 255))));
        Uint8 bv = QUANTIZE(ApplyContrast((Uint8)fminf(255, final_val * (cb * 150 + core * 255))));
        Uint8 av = (Uint8)fminf(255, final_val * 255 * fminf(1.0f, falloff * 5.0f));
        pixels[y * size + x] = (av << 24) | (bv << 16) | (gv << 8) | rv;
      }
    }
  }
}

void DrawAsteroidToBuffer(Uint32 *pixels, int size, float seed) {
  int center = size / 2;
  float base_radius = size * 0.15f;
  for (int y = 0; y < size; y++) {
    for (int x = 0; x < size; x++) {
      float dx = (float)(x - center), dy = (float)(y - center);
      float dist = sqrtf(dx * dx + dy * dy);
      float angle = atan2f(dy, dx);
      float shape_n = 0.0f, s_freq = 1.2f, s_amp = 0.6f;
      for (int o = 0; o < 5; o++) { 
          shape_n += (PerlinNoise2D(cosf(angle) * s_freq + seed, sinf(angle) * s_freq + seed) - 0.5f) * s_amp; 
          s_freq *= 2.5f; s_amp *= 0.45f; 
      }
      float distorted_radius = base_radius * (1.0f + shape_n);
      if (dist <= distorted_radius) {
        float warp = ValueNoise2D(x * 0.04f + seed, y * 0.04f + seed) * 0.5f;
        float cracks1 = VoronoiCracks2D(x * 0.06f + seed + warp, y * 0.06f + seed + warp);
        float cracks2 = VoronoiCracks2D(x * 0.2f + seed * 2.0f, y * 0.2f + seed * 3.0f);
        float mix_val = fmaxf(0.0f, fminf(1.0f, (ValueNoise2D(x * 0.08f + seed * 4.0f, y * 0.08f + seed * 4.0f) - 0.3f) * 1.5f));
        float a_theme = DeterministicHash((int)(seed * 789.0f), 123), r1, g1, b1, r2, g2, b2;
        if (a_theme > 0.7f) { r1 = 0.5f; g1 = 0.45f; b1 = 0.42f; r2 = 0.75f; g2 = 0.4f; b2 = 0.35f; }
        else if (a_theme > 0.4f) { r1 = 0.55f; g1 = 0.55f; b1 = 0.6f; r2 = 0.45f; g2 = 0.5f; b2 = 0.75f; }
        else { r1 = 0.35f; g1 = 0.35f; b1 = 0.38f; r2 = 0.55f; g2 = 0.55f; b2 = 0.58f; }
        float tr = r1 * (1.0f - mix_val) + r2 * mix_val, tg = g1 * (1.0f - mix_val) + g2 * mix_val, tb = b1 * (1.0f - mix_val) + b2 * mix_val;
        float lx = -0.707f, ly = -0.707f;
        float nx = dx / (dist + 0.1f), ny = dy / (dist + 0.1f);
        float dot = fmaxf(0.2f, nx * lx + ny * ly);
        float shade_3d = powf(dot, 0.5f);
        float base_val = 180.0f + ValueNoise2D(x * 0.15f + seed, y * 0.15f + seed) * 120.0f + shape_n * 25.0f;
        float edge_shade = 1.0f - powf(dist / distorted_radius, 4.0f) * 0.6f;
        float darken = edge_shade * shade_3d * (0.4f + 0.6f * powf(fmaxf(0.0f, fminf(1.0f, cracks1 / 0.4f)), 0.8f)) * (0.6f + 0.4f * powf(fmaxf(0.0f, fminf(1.0f, cracks2 / 0.2f)), 0.8f));
        Uint8 r = QUANTIZE(ApplyContrast((Uint8)fminf(255, base_val * tr * darken)));
        Uint8 g = QUANTIZE(ApplyContrast((Uint8)fminf(255, base_val * tg * darken)));
        Uint8 b = QUANTIZE(ApplyContrast((Uint8)fminf(255, base_val * tb * darken)));
        pixels[y * size + x] = (255 << 24) | (b << 16) | (g << 8) | r;
      } else {
        float dust_noise = PerlinNoise2D(cosf(angle) * 0.8f + seed + 100, sinf(angle) * 0.8f + seed + 100);
        float dust_outer = base_radius * (1.5f + dust_noise * 3.0f);
        if (dist <= dust_outer) {
          float dust_t = (dist - distorted_radius) / (dust_outer - distorted_radius);
          float detail_n = PerlinNoise2D(x * 0.08f + seed + 500, y * 0.08f + seed);
          float edge_falloff = fminf(1.0f, fminf(fminf(x, (size - 1) - x), fminf(y, (size - 1) - y)) / 30.0f);
          float alpha_f = powf(1.0f - dust_t, 2.5f) * detail_n * edge_falloff;
          if (alpha_f > 0.01f) { 
              Uint8 val = QUANTIZE(ApplyContrast((Uint8)(35 + detail_n * 25))); 
              pixels[y * size + x] = ((Uint8)(alpha_f * 100) << 24) | (val << 16) | (val << 8) | val; 
          }
        }
      }
    }
  }
}

void DrawCrystalToBuffer(Uint32 *pixels, int size, float seed) {
  int center = size / 2;
  float base_radius = size * 0.35f;
  for (int y = 0; y < size; y++) {
    for (int x = 0; x < size; x++) {
      float dx = (float)(x - center), dy = (float)(y - center);
      float dist = sqrtf(dx * dx + dy * dy);
      float v = VoronoiNoise2D(x * 0.1f + seed, y * 0.1f + seed);
      float edge = VoronoiCracks2D(x * 0.08f + seed, y * 0.08f + seed);
      float radius = base_radius * (0.8f + v * 0.4f);
      if (dist <= radius) {
          float norm_dist = dist / radius;
          float shine = powf(1.0f - edge, 4.0f);
          float a_theme = DeterministicHash((int)(seed * 123), 456);
          float r, g, b;
          if (a_theme > 0.6f) { r = 0.2f; g = 0.8f; b = 1.0f; } 
          else if (a_theme > 0.3f) { r = 0.8f; g = 0.3f; b = 1.0f; }
          else { r = 0.4f; g = 1.0f; b = 0.4f; }
          Uint8 rv = (Uint8)fminf(255, (r * 150 + shine * 105) * (1.2f - norm_dist * 0.5f));
          Uint8 gv = (Uint8)fminf(255, (g * 150 + shine * 105) * (1.2f - norm_dist * 0.5f));
          Uint8 bv = (Uint8)fminf(255, (b * 150 + shine * 105) * (1.2f - norm_dist * 0.5f));
          Uint8 av = (Uint8)fminf(255, 200 + shine * 55);
          pixels[y * size + x] = (av << 24) | (bv << 16) | (gv << 8) | rv;
      }
    }
  }
}

void DrawMothershipToBuffer(Uint32 *pixels, int size, float seed) {
    float world_scale = (float)size / MOTHERSHIP_WORLD_SCALE_DIVISOR;
    for (int y = 0; y < size; y++) {
        float wy = (float)y / world_scale - 4.0f;
        for (int x = 0; x < size; x++) {
            float wx = (float)x / world_scale - 4.0f;
            float ax = fabsf(wx); bool in_hull = false; float hull_depth = 0.0f; 
            float hull_w = 0.7f - (wy * 0.15f);
            if (wy > -3.0f && wy < 2.0f && ax < hull_w) { in_hull = true; hull_depth = 1.0f - (ax / hull_w); }
            if (wy > -0.5f && wy < 0.8f && ax < 2.2f) { float wing_depth = 1.0f - (ax / 2.2f); if (wing_depth > 0) { in_hull = true; hull_depth = fmaxf(hull_depth, wing_depth); } }
            if (wy > 1.2f && wy < 2.5f && ax > 0.5f && ax < 1.8f) { float fin_w = 1.8f - (wy - 1.2f) * 0.5f; if (ax < fin_w) { in_hull = true; hull_depth = fmaxf(hull_depth, fin_w - ax); } }
            if (in_hull) {
                Uint8 intensity = (Uint8)(SDL_clamp(hull_depth * 150.0f, 40.0f, 200.0f));
                intensity = (Uint8)SDL_clamp(intensity - (wy * 10.0f), 30.0f, 240.0f);
                intensity = (intensity / 16) * 16; 
                Uint8 r = QUANTIZE(60), g = QUANTIZE(intensity), b = QUANTIZE(100 + intensity/4), a = 255;
                float bx = floorf(ax * 6.0f), by = floorf(wy * 6.0f);
                if (DeterministicHash((int)bx, (int)by) > 0.8f) { r -= 15; g -= 15; b -= 15; }
                if (fmodf(wy + 4.0f, 0.8f) < 0.1f) { r = (Uint8)(r * 1.2f); g = (Uint8)(g * 1.2f); b = (Uint8)(b * 1.2f); }
                if (wy < -1.5f && wy > -2.0f && ax < 0.3f) { r = 100; g = 200; b = 255; }
                if (wy > 0.0f && wy < 0.2f && ax > 2.0f && ax < 2.15f) { if (wx > 0) { r = 255; g = 50; b = 50; } else { r = 50; g = 255; b = 50; } }
                pixels[y * size + x] = (a << 24) | (b << 16) | (g << 8) | r;
            }
        }
    }
}

void DrawExplosionPuffToBuffer(Uint32 *pixels, int size, float seed) {
  int center = size / 2; float max_rad = size * 0.45f;
  for (int y = 0; y < size; y++) {
    for (int x = 0; x < size; x++) {
      float dx = (float)(x - center), dy = (float)(y - center);
      float dist = sqrtf(dx * dx + dy * dy); if (dist > max_rad) continue;
      float noise = (PerlinNoise2D(x * 0.2f + seed, y * 0.2f + seed) * 0.7f + PerlinNoise2D(x * 0.4f + seed * 2, y * 0.4f + seed * 2) * 0.3f);
      float alpha_f = powf(1.0f - (dist / max_rad), 1.5f) * noise;
      if (alpha_f > 0.05f) { Uint8 val = QUANTIZE(ApplyContrast((Uint8)(150 + noise * 100))); pixels[y * size + x] = ((Uint8)(alpha_f * 255) << 24) | (val << 16) | (val << 8) | val; }
    }
  }
}

void DrawDebrisToBuffer(Uint32 *pixels, int size, float seed) {
  int center = size / 2;
  float base_radius = size * 0.25f;
  for (int y = 0; y < size; y++) {
    for (int x = 0; x < size; x++) {
      float dx = (float)(x - center), dy = (float)(y - center);
      float dist = sqrtf(dx * dx + dy * dy);
      if (dist < 1.0f) continue;
      float angle = atan2f(dy, dx);
      float shape_n = 0.0f, s_freq = 1.5f, s_amp = 0.8f;
      for (int o = 0; o < 4; o++) { shape_n += (PerlinNoise2D(cosf(angle) * s_freq + seed, sinf(angle) * s_freq + seed) - 0.5f) * s_amp; s_freq *= 2.5f; s_amp *= 0.45f; };
      float distorted_radius = base_radius * (1.0f + shape_n);
      if (dist <= distorted_radius) {
        float lx = -0.707f, ly = -0.707f;
        float nx = dx / (dist + 0.1f), ny = dy / (dist + 0.1f);
        float dot = fmaxf(0.15f, nx * lx + ny * ly); 
        float shade_3d = powf(dot, 0.45f);
        float n1 = ValueNoise2D(x * 0.3f + seed, y * 0.3f + seed);
        float n2 = ValueNoise2D(x * 1.0f + seed * 2.0f, y * 1.0f + seed * 2.0f);
        float detail = n1 * 0.7f + n2 * 0.3f;
        float edge_factor = 1.0f - powf(dist / distorted_radius, 4.0f);
        float crack_val = VoronoiCracks2D(x * 0.3f + seed, y * 0.3f + seed);
        float cracks = powf(fminf(1.0f, crack_val * 5.0f), 0.5f);
        float base_val = 20.0f + detail * 60.0f + shape_n * 20.0f;
        Uint8 v = QUANTIZE(ApplyContrast((Uint8)fminf(255, base_val * shade_3d * edge_factor * (0.15f + 0.85f * cracks))));
        pixels[y * size + x] = (255 << 24) | (v << 16) | (v << 8) | v;
      }
    }
  }
}

void Asset_GenerateStep(AppState *s) {
  int total_assets = PLANET_COUNT + GALAXY_COUNT + ASTEROID_TYPE_COUNT + CRYSTAL_COUNT + DEBRIS_COUNT + 2;
  if (s->assets_generated >= total_assets) return;
  
  if (s->assets_generated < PLANET_COUNT) {
    int i = s->assets_generated;
    int sz = 512; Uint32 *p = SDL_malloc(sz * sz * 4); SDL_memset(p, 0, sz * sz * 4);
    DrawPlanetToBuffer(p, sz, (float)i * 567.89f);
    s->textures.planet_textures[i] = SDL_CreateTexture(s->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, sz, sz);
    SDL_SetTextureBlendMode(s->textures.planet_textures[i], SDL_BLENDMODE_BLEND);
    SDL_UpdateTexture(s->textures.planet_textures[i], NULL, p, sz * 4); SDL_free(p);
  } else if (s->assets_generated < PLANET_COUNT + GALAXY_COUNT) {
    int i = s->assets_generated - PLANET_COUNT;
    int sz = 1024; Uint32 *p = SDL_malloc(sz * sz * 4); SDL_memset(p, 0, sz * sz * 4);
    DrawGalaxyToBuffer(p, sz, (float)i * 123.45f + 99.0f);
    s->textures.galaxy_textures[i] = SDL_CreateTexture(s->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, sz, sz);
    SDL_SetTextureBlendMode(s->textures.galaxy_textures[i], SDL_BLENDMODE_BLEND);
    SDL_UpdateTexture(s->textures.galaxy_textures[i], NULL, p, sz * 4); SDL_free(p);
  } else if (s->assets_generated < PLANET_COUNT + GALAXY_COUNT + ASTEROID_TYPE_COUNT) {
    int i = s->assets_generated - (PLANET_COUNT + GALAXY_COUNT);
    int sz = 256; Uint32 *p = SDL_malloc(sz * sz * 4); SDL_memset(p, 0, sz * sz * 4);
    DrawAsteroidToBuffer(p, sz, (float)i * 432.1f + 11.0f);
    s->textures.asteroid_textures[i] = SDL_CreateTexture(s->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, sz, sz);
    SDL_SetTextureBlendMode(s->textures.asteroid_textures[i], SDL_BLENDMODE_BLEND);
    SDL_UpdateTexture(s->textures.asteroid_textures[i], NULL, p, sz * 4); SDL_free(p);
  } else if (s->assets_generated < PLANET_COUNT + GALAXY_COUNT + ASTEROID_TYPE_COUNT + CRYSTAL_COUNT) {
    int i = s->assets_generated - (PLANET_COUNT + GALAXY_COUNT + ASTEROID_TYPE_COUNT);
    int sz = 128; Uint32 *p = SDL_malloc(sz * sz * 4); SDL_memset(p, 0, sz * sz * 4);
    DrawCrystalToBuffer(p, sz, (float)i * 12.3f);
    s->textures.crystal_textures[i] = SDL_CreateTexture(s->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, sz, sz);
    SDL_SetTextureBlendMode(s->textures.crystal_textures[i], SDL_BLENDMODE_BLEND);
    SDL_UpdateTexture(s->textures.crystal_textures[i], NULL, p, sz * 4); SDL_free(p);
  } else if (s->assets_generated < PLANET_COUNT + GALAXY_COUNT + ASTEROID_TYPE_COUNT + CRYSTAL_COUNT + DEBRIS_COUNT) {
    int i = s->assets_generated - (PLANET_COUNT + GALAXY_COUNT + ASTEROID_TYPE_COUNT + CRYSTAL_COUNT);
    int sz = 128; Uint32 *p = SDL_malloc(sz * sz * 4); SDL_memset(p, 0, sz * sz * 4);
    DrawDebrisToBuffer(p, sz, (float)i * 987.6f + 55.0f);
    s->textures.debris_textures[i] = SDL_CreateTexture(s->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, sz, sz);
    SDL_SetTextureBlendMode(s->textures.debris_textures[i], SDL_BLENDMODE_BLEND);
    SDL_UpdateTexture(s->textures.debris_textures[i], NULL, p, sz * 4); SDL_free(p);
  } else if (s->assets_generated == total_assets - 2) {
    int sz = 128; Uint32 *p = SDL_malloc(sz * sz * 4); SDL_memset(p, 0, sz * sz * 4);
    DrawExplosionPuffToBuffer(p, sz, 777.7f);
    s->textures.explosion_puff_texture = SDL_CreateTexture(s->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, sz, sz);
    SDL_SetTextureBlendMode(s->textures.explosion_puff_texture, SDL_BLENDMODE_BLEND);
    SDL_UpdateTexture(s->textures.explosion_puff_texture, NULL, p, sz * 4); SDL_free(p);
  } else if (s->assets_generated == total_assets - 1) {
    int sz = 256; Uint32 *p = SDL_malloc(sz * sz * 4); SDL_memset(p, 0, sz * sz * 4);
    DrawMothershipToBuffer(p, sz, 123.4f);
    s->textures.mothership_hull_texture = SDL_CreateTexture(s->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, sz, sz);
    SDL_SetTextureBlendMode(s->textures.mothership_hull_texture, SDL_BLENDMODE_BLEND);
    SDL_UpdateTexture(s->textures.mothership_hull_texture, NULL, p, sz * 4); SDL_free(p);
  }
  
  s->assets_generated++;
  if (s->assets_generated >= total_assets) s->game_state = STATE_GAME;
}

void Asset_DrawLoading(AppState *s) {
  int win_w, win_h; SDL_GetRenderLogicalPresentation(s->renderer, &win_w, &win_h, NULL);
  if (win_w == 0 || win_h == 0) SDL_GetRenderOutputSize(s->renderer, &win_w, &win_h);
  SDL_SetRenderDrawColor(s->renderer, 0, 0, 0, 255); SDL_RenderClear(s->renderer);
  int total_assets = PLANET_COUNT + GALAXY_COUNT + ASTEROID_TYPE_COUNT + CRYSTAL_COUNT + DEBRIS_COUNT + 2;
  float progress = (float)s->assets_generated / (float)total_assets;
  float bar_w = 400.0f, bar_h = 20.0f, x = (win_w - bar_w) / 2.0f, y = (win_h - bar_h) / 2.0f;
  SDL_SetRenderScale(s->renderer, 4.0f, 4.0f);
  SDL_SetRenderDrawColor(s->renderer, 255, 255, 255, 255); SDL_RenderDebugText(s->renderer, (win_w / 8.0f) - 36, (y / 4.0f) - 30, "Asteroidz");
  SDL_SetRenderScale(s->renderer, 1.0f, 1.0f);
  SDL_SetRenderDrawColor(s->renderer, 200, 200, 200, 255); SDL_RenderDebugText(s->renderer, x, y - 25, "Loading ...");
  SDL_SetRenderDrawColor(s->renderer, 50, 50, 50, 255); SDL_RenderFillRect(s->renderer, &(SDL_FRect){x, y, bar_w, bar_h});
  SDL_SetRenderDrawColor(s->renderer, 100, 255, 100, 255); SDL_RenderFillRect(s->renderer, &(SDL_FRect){x, y, bar_w * progress, bar_h});
  SDL_RenderPresent(s->renderer);
}