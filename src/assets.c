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
  int center = size / 2;
  float radius = (size / 2.0f) * 0.70f;
  float atmo_outer = (size / 2.0f) * 0.95f;
  float theme = DeterministicHash((int)(seed * 1000), 42);
  float rm, gm, bm;
  if (theme > 0.75f) { rm = 1.0f; gm = 0.4f; bm = 0.4f; }
  else if (theme > 0.50f) { rm = 0.4f; gm = 1.0f; bm = 0.5f; }
  else if (theme > 0.25f) { rm = 0.8f; gm = 0.5f; bm = 1.0f; }
  else { rm = 0.5f; gm = 0.7f; bm = 1.0f; }
  for (int y = 0; y < size; y++) {
    for (int x = 0; x < size; x++) {
      float dx = (float)(x - center), dy = (float)(y - center);
      float dist = sqrtf(dx * dx + dy * dy);
      if (dist <= atmo_outer) {
        float nx = dx / radius, ny = dy / radius;
        float nz = (dist <= radius) ? sqrtf(fmaxf(0.0f, 1.0f - nx * nx - ny * ny)) : 0.0f;
        float noise = 0.0f, amp = 0.7f, freq = 0.02f;
        for (int o = 0; o < 4; o++) { noise += PerlinNoise2D((float)x * freq + seed, (float)y * freq + seed * 2.0f) * amp; amp *= 0.35f; freq *= 2.0f; }
        float dot = fmaxf(0.15f, nx * -0.6f + ny * -0.6f + nz * 0.5f); 
        float shading = powf(dot, 0.7f);
        Uint8 r = QUANTIZE(ApplyContrast((Uint8)fminf(255.0f, (100 + noise * 155) * rm * shading)));
        Uint8 g = QUANTIZE(ApplyContrast((Uint8)fminf(255.0f, (100 + noise * 155) * gm * shading)));
        Uint8 b = QUANTIZE(ApplyContrast((Uint8)fminf(255.0f, (100 + noise * 155) * bm * shading)));
        Uint8 alpha = 255;
        if (dist > radius) {
          float atmo_t = (dist - radius) / (atmo_outer - radius);
          float an = PerlinNoise2D((float)x * 0.01f + seed, (float)y * 0.01f);
          float af = powf(1.0f - atmo_t, 1.5f) * (0.3f + an * 0.7f);
          alpha = (Uint8)(af * 120);
          r = QUANTIZE(ApplyContrast((Uint8)(r * 0.2f + 255 * rm * af))); 
          g = QUANTIZE(ApplyContrast((Uint8)(g * 0.2f + 255 * gm * af))); 
          b = QUANTIZE(ApplyContrast((Uint8)(b * 0.2f + 255 * bm * af)));
        }
        pixels[y * size + x] = (alpha << 24) | (b << 16) | (g << 8) | r;
      }
    }
  }
}

void DrawGalaxyToBuffer(Uint32 *pixels, int size, float seed) {
  int center = size / 2;
  int s_int = (int)(seed * 1000000.0f);
  float twist = 5.0f + DeterministicHash(s_int, 7) * 15.0f;
  int arms = 2 + (int)(DeterministicHash(s_int, 13) * 10);
  float arm_thickness = 1.0f + DeterministicHash(s_int, 88) * 3.0f;
  float core_rad = size * (0.002f + DeterministicHash(s_int, 19) * 0.01f);
  float theme = DeterministicHash(s_int, 42);
  float tr, tg, tb, tr2, tg2, tb2;
  if (theme > 0.90f) { tr = 1.0f; tg = 0.8f; tb = 0.2f; tr2 = 1.0f; tg2 = 0.1f; tb2 = 0.1f; }
  else if (theme > 0.80f) { tr = 0.2f; tg = 1.0f; tb = 0.3f; tr2 = 1.0f; tg2 = 1.0f; tb2 = 0.2f; }
  else if (theme > 0.50f) { tr = 0.1f; tg = 0.2f; tb = 1.0f; tr2 = 0.2f; tg2 = 0.9f; tb2 = 1.0f; }
  else if (theme > 0.25f) { tr = 0.5f; tg = 0.1f; tb = 1.0f; tr2 = 1.0f; tg2 = 0.2f; tb2 = 0.8f; }
  else { tr = 0.1f; tg = 0.8f; tb = 0.8f; tr2 = 0.1f; tg2 = 0.3f; tb2 = 1.0f; }
  float cloud_scale = 0.5f + DeterministicHash(s_int, 55) * 2.0f;
  for (int y = 0; y < size; y++) {
    for (int x = 0; x < size; x++) {
      float dx = (float)(x - center), dy = (float)(y - center);
      float dist = sqrtf(dx * dx + dy * dy);
      float max_dist = size * 0.45f;
      if (dist > max_dist) continue;
      float angle = atan2f(dy, dx);
      float norm_dist = dist / max_dist;
      float twisted_angle = angle - norm_dist * twist;
      float arm_sin = powf(fabsf(cosf(twisted_angle * (float)arms / 2.0f)), arm_thickness);
      float n = PerlinNoise2D(x * 0.05f + seed, y * 0.05f);
      float cloud_n = PerlinNoise2D(x * 0.01f + seed + 200, y * 0.01f + 300);
      float cloud = powf(cloud_n, 1.1f) * cloud_scale;
      float intensity = (arm_sin * 0.5f + 0.1f) * n + cloud;
      float falloff = 1.0f - powf(norm_dist, 0.3f);
      float core = expf(-dist / core_rad) * 4.0f;
      float final_val = fminf(1.0f, (intensity * falloff + core) * 1.5f);
      if (final_val > 0.02f) {
        float cn = PerlinNoise2D(x * 0.025f + seed + 500, y * 0.025f);
        float mix_val = fmaxf(0.0f, fminf(1.0f, (cn - 0.5f) * (1.0f + norm_dist * 4.0f) + 0.5f));
        float cr = tr * (1.0f - mix_val) + tr2 * mix_val, cg = tg * (1.0f - mix_val) + tg2 * mix_val, cb = tb * (1.0f - mix_val) + tb2 * mix_val;
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
        float base_val = 140.0f + ValueNoise2D(x * 0.15f + seed, y * 0.15f + seed) * 100.0f + shape_n * 20.0f;
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
  float base_radius = size * 0.18f; // Reduced to fit glow (2.5x radius)
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
          Uint8 rv = QUANTIZE(ApplyContrast((Uint8)fminf(255, (r * 150 + shine * 105) * (1.2f - norm_dist * 0.5f))));
          Uint8 gv = QUANTIZE(ApplyContrast((Uint8)fminf(255, (g * 150 + shine * 105) * (1.2f - norm_dist * 0.5f))));
          Uint8 bv = QUANTIZE(ApplyContrast((Uint8)fminf(255, (b * 150 + shine * 105) * (1.2f - norm_dist * 0.5f))));
          Uint8 av = (Uint8)fminf(255, 120 + shine * 80); // Increased transparency for body
          pixels[y * size + x] = (av << 24) | (bv << 16) | (gv << 8) | rv;
      } else {
          // Glow / Dust
          float glow_dist = dist / (base_radius * 2.5f); // Increased glow radius
          if (glow_dist < 1.0f) {
              float glow_n = PerlinNoise2D(x * 0.05f + seed, y * 0.05f + seed);
              float glow_alpha = powf(1.0f - glow_dist, 2.0f) * (0.4f + glow_n * 0.6f); // Stronger alpha curve
              if (glow_alpha > 0.01f) {
                  float a_theme = DeterministicHash((int)(seed * 123), 456);
                  float r, g, b;
                  if (a_theme > 0.6f) { r = 0.2f; g = 0.8f; b = 1.0f; } 
                  else if (a_theme > 0.3f) { r = 0.8f; g = 0.3f; b = 1.0f; }
                  else { r = 0.4f; g = 1.0f; b = 0.4f; }
                  
                  Uint8 rv = (Uint8)(r * 255);
                  Uint8 gv = (Uint8)(g * 255);
                  Uint8 bv = (Uint8)(b * 255);
                  Uint8 av = (Uint8)fminf(255, glow_alpha * 255.0f); // Maximize opacity
                  pixels[y * size + x] = (av << 24) | (bv << 16) | (gv << 8) | rv;
              }
          }
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

void DrawMinerToBuffer(Uint32 *pixels, int size, float seed) {
    int center = size / 2;
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float dx = (float)(x - center) / (size/2.0f);
            float dy = (float)(y - center) / (size/2.0f);
            float adx = fabsf(dx), ady = fabsf(dy);
            
            // Industrial/Boxy shape
            bool body = (adx < 0.6f && ady < 0.8f) || (adx < 0.8f && ady < 0.4f);
            // Cockpit
            bool cockpit = (dy < -0.4f && dy > -0.7f && adx < 0.3f);
            // Engines
            bool engine = (dy > 0.6f && dy < 0.9f && adx > 0.3f && adx < 0.6f);
            // Drill bits (front)
            bool drill = (dy < -0.7f && dy > -0.95f && adx < 0.2f && fmodf(dy * 20.0f, 1.0f) > 0.5f);

            if (body || cockpit || engine || drill) {
                Uint8 r = 0, g = 0, b = 0;
                float shade = 1.0f - (adx * 0.5f + ady * 0.2f);
                
                if (drill) { r = 200; g = 200; b = 200; shade *= 1.2f; }
                else if (cockpit) { r = 50; g = 200; b = 255; shade = 1.0f; }
                else if (engine) { r = 100; g = 100; b = 100; }
                else { // Main Body - Yellow/Orange industrial
                    r = 220; g = 180; b = 50; 
                    if (adx < 0.1f || ady < 0.1f) { r -= 30; g -= 30; b -= 30; } // Panel lines
                }

                r = QUANTIZE((Uint8)(r * shade));
                g = QUANTIZE((Uint8)(g * shade));
                b = QUANTIZE((Uint8)(b * shade));
                pixels[y * size + x] = (255 << 24) | (b << 16) | (g << 8) | r;
            }
        }
    }
}

void DrawFighterToBuffer(Uint32 *pixels, int size, float seed) {
    int center = size / 2;
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float dx = (float)(x - center) / (size/2.0f);
            float dy = (float)(y - center) / (size/2.0f);
            float adx = fabsf(dx);
            
            // Triangular/Sleek shape
            // Main fuselage
            bool body = (adx < (0.8f - (dy + 1.0f) * 0.4f) && dy > -0.8f && dy < 0.8f);
            // Wings
            bool wings = (dy > 0.2f && dy < 0.7f && adx < (0.9f - (dy - 0.2f)));
            // Cockpit
            bool cockpit = (dy > -0.2f && dy < 0.1f && adx < 0.15f);
            // Engine glow
            bool engine = (dy > 0.8f && dy < 0.95f && adx < 0.2f);

            if (body || wings || cockpit || engine) {
                Uint8 r = 0, g = 0, b = 0;
                float shade = 1.0f - (adx + (dy + 1.0f) * 0.2f) * 0.5f;

                if (engine) { r = 255; g = 100; b = 50; shade = 1.2f; }
                else if (cockpit) { r = 100; g = 255; b = 255; shade = 1.0f; }
                else { // Hull - Red/Grey
                    r = 180; g = 60; b = 60;
                    if (wings) { r = 140; g = 50; b = 50; }
                    if (adx < 0.05f) { r += 40; g += 40; b += 40; } // Spine
                }

                r = QUANTIZE((Uint8)fminf(255, r * shade));
                g = QUANTIZE((Uint8)fminf(255, g * shade));
                b = QUANTIZE((Uint8)fminf(255, b * shade));
                pixels[y * size + x] = (255 << 24) | (b << 16) | (g << 8) | r;
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

void DrawIconToBuffer(Uint32 *pixels, int size, int type) {
    int center = size / 2;
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float dx = (float)(x - center) / (size/2.0f);
            float dy = (float)(y - center) / (size/2.0f);
            float adx = fabsf(dx), ady = fabsf(dy);
            bool fill = false;
            
            if (type == ICON_MOVE) {
                // Arrow
                fill = (dy > -0.6f && dy < 0.6f && adx < 0.2f) || (dy < -0.4f && ady < 0.8f && adx < (0.8f - (1.0f + dy)));
            } else if (type == ICON_ATTACK) {
                // Crosshair
                float dist = sqrtf(dx*dx + dy*dy);
                fill = (dist > 0.6f && dist < 0.8f) || (adx < 0.1f && ady > 0.3f) || (ady < 0.1f && adx > 0.3f) || (dist < 0.1f);
            } else if (type == ICON_PATROL) {
                // Loop
                float dist = sqrtf(dx*dx + dy*dy);
                fill = (dist > 0.5f && dist < 0.7f && !(dx > 0.2f && dy < 0.2f && dy > -0.2f));
                if (dx > 0.4f && dy < 0.1f && dy > -0.3f && adx < 0.2f) fill = true; // Arrowhead attempt
            } else if (type == ICON_STOP) {
                // Square
                fill = (adx < 0.5f && ady < 0.5f);
            } else if (type == ICON_OFFENSIVE) {
                // Sword-like
                fill = (adx < 0.1f && dy > -0.7f && dy < 0.7f) || (dy > 0.4f && dy < 0.5f && adx < 0.4f);
            } else if (type == ICON_DEFENSIVE) {
                // Shield
                fill = (dy > -0.5f && dy < 0.2f && adx < 0.6f) || (dy >= 0.2f && dy < 0.8f && adx < (0.6f - (dy-0.2f)));
            } else if (type == ICON_HOLD) {
                // Hand / Palm
                fill = (ady < 0.5f && adx < 0.4f);
            } else if (type == ICON_MAIN_CANNON) {
                // Big Circle
                float dist = sqrtf(dx*dx + dy*dy);
                fill = (dist < 0.8f);
            } else if (type == ICON_BACK) {
                // Back Arrow
                fill = (adx < 0.6f && ady < 0.2f) || (dx < -0.4f && ady < 0.5f && adx > (0.4f + ady));
            } else if (type == ICON_GATHER) {
                // Crystal/Gem shape
                fill = (ady + adx < 0.7f);
            } else if (type == ICON_RETURN) {
                // Upward arrow to "return"
                fill = (adx < 0.2f && dy > -0.4f && dy < 0.6f) || (dy < -0.2f && adx < (0.6f - fabsf(dy + 0.5f)));
            }

            if (fill) {
                pixels[y * size + x] = 0xFFFFFFFF; // White
            }
        }
    }
}

void Asset_GenerateStep(AppState *s) {
  int total_assets = PLANET_COUNT + GALAXY_COUNT + ASTEROID_TYPE_COUNT + CRYSTAL_COUNT + DEBRIS_COUNT + 4 + ICON_COUNT;
  if (s->assets_generated >= total_assets) return;
  
  if (s->assets_generated < PLANET_COUNT) {
    int i = s->assets_generated;
    int sz = 512; Uint32 *p = SDL_malloc(sz * sz * 4); SDL_memset(p, 0, sz * sz * 4);
    DrawPlanetToBuffer(p, sz, (float)i * 567.89f);
    s->textures.planet_textures[i] = SDL_CreateTexture(s->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, sz, sz);
    SDL_SetTextureBlendMode(s->textures.planet_textures[i], SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(s->textures.planet_textures[i], SDL_SCALEMODE_NEAREST);
    SDL_UpdateTexture(s->textures.planet_textures[i], NULL, p, sz * 4); SDL_free(p);
  } else if (s->assets_generated < PLANET_COUNT + GALAXY_COUNT) {
    int i = s->assets_generated - PLANET_COUNT;
    int sz = 1024; Uint32 *p = SDL_malloc(sz * sz * 4); SDL_memset(p, 0, sz * sz * 4);
    DrawGalaxyToBuffer(p, sz, (float)i * 123.45f + 99.0f);
    s->textures.galaxy_textures[i] = SDL_CreateTexture(s->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, sz, sz);
    SDL_SetTextureBlendMode(s->textures.galaxy_textures[i], SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(s->textures.galaxy_textures[i], SDL_SCALEMODE_NEAREST);
    SDL_UpdateTexture(s->textures.galaxy_textures[i], NULL, p, sz * 4); SDL_free(p);
  } else if (s->assets_generated < PLANET_COUNT + GALAXY_COUNT + ASTEROID_TYPE_COUNT) {
    int i = s->assets_generated - (PLANET_COUNT + GALAXY_COUNT);
    int sz = 256; Uint32 *p = SDL_malloc(sz * sz * 4); SDL_memset(p, 0, sz * sz * 4);
    DrawAsteroidToBuffer(p, sz, (float)i * 432.1f + 11.0f);
    s->textures.asteroid_textures[i] = SDL_CreateTexture(s->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, sz, sz);
    SDL_SetTextureBlendMode(s->textures.asteroid_textures[i], SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(s->textures.asteroid_textures[i], SDL_SCALEMODE_NEAREST);
    SDL_UpdateTexture(s->textures.asteroid_textures[i], NULL, p, sz * 4); SDL_free(p);
  } else if (s->assets_generated < PLANET_COUNT + GALAXY_COUNT + ASTEROID_TYPE_COUNT + CRYSTAL_COUNT) {
    int i = s->assets_generated - (PLANET_COUNT + GALAXY_COUNT + ASTEROID_TYPE_COUNT);
    int sz = 128; Uint32 *p = SDL_malloc(sz * sz * 4); SDL_memset(p, 0, sz * sz * 4);
    DrawCrystalToBuffer(p, sz, (float)i * 12.3f);
    s->textures.crystal_textures[i] = SDL_CreateTexture(s->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, sz, sz);
    SDL_SetTextureBlendMode(s->textures.crystal_textures[i], SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(s->textures.crystal_textures[i], SDL_SCALEMODE_NEAREST);
    SDL_UpdateTexture(s->textures.crystal_textures[i], NULL, p, sz * 4); SDL_free(p);
  } else if (s->assets_generated < PLANET_COUNT + GALAXY_COUNT + ASTEROID_TYPE_COUNT + CRYSTAL_COUNT + DEBRIS_COUNT) {
    int i = s->assets_generated - (PLANET_COUNT + GALAXY_COUNT + ASTEROID_TYPE_COUNT + CRYSTAL_COUNT);
    int sz = 128; Uint32 *p = SDL_malloc(sz * sz * 4); SDL_memset(p, 0, sz * sz * 4);
    DrawDebrisToBuffer(p, sz, (float)i * 987.6f + 55.0f);
    s->textures.debris_textures[i] = SDL_CreateTexture(s->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, sz, sz);
    SDL_SetTextureBlendMode(s->textures.debris_textures[i], SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(s->textures.debris_textures[i], SDL_SCALEMODE_NEAREST);
    SDL_UpdateTexture(s->textures.debris_textures[i], NULL, p, sz * 4); SDL_free(p);
  } else if (s->assets_generated == total_assets - (4 + ICON_COUNT)) {
    int sz = 128; Uint32 *p = SDL_malloc(sz * sz * 4); SDL_memset(p, 0, sz * sz * 4);
    DrawExplosionPuffToBuffer(p, sz, 777.7f);
    s->textures.explosion_puff_texture = SDL_CreateTexture(s->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, sz, sz);
    SDL_SetTextureBlendMode(s->textures.explosion_puff_texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(s->textures.explosion_puff_texture, SDL_SCALEMODE_NEAREST);
    SDL_UpdateTexture(s->textures.explosion_puff_texture, NULL, p, sz * 4); SDL_free(p);
  } else if (s->assets_generated == total_assets - (3 + ICON_COUNT)) {
    int sz = 256; Uint32 *p = SDL_malloc(sz * sz * 4); SDL_memset(p, 0, sz * sz * 4);
    DrawMothershipToBuffer(p, sz, 123.4f);
    s->textures.mothership_hull_texture = SDL_CreateTexture(s->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, sz, sz);
    SDL_SetTextureBlendMode(s->textures.mothership_hull_texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(s->textures.mothership_hull_texture, SDL_SCALEMODE_NEAREST);
    SDL_UpdateTexture(s->textures.mothership_hull_texture, NULL, p, sz * 4); SDL_free(p);
  } else if (s->assets_generated == total_assets - (2 + ICON_COUNT)) {
    int sz = 128; Uint32 *p = SDL_malloc(sz * sz * 4); SDL_memset(p, 0, sz * sz * 4);
    DrawMinerToBuffer(p, sz, 555.5f);
    s->textures.miner_texture = SDL_CreateTexture(s->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, sz, sz);
    SDL_SetTextureBlendMode(s->textures.miner_texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(s->textures.miner_texture, SDL_SCALEMODE_NEAREST);
    SDL_UpdateTexture(s->textures.miner_texture, NULL, p, sz * 4); SDL_free(p);
  } else if (s->assets_generated == total_assets - (1 + ICON_COUNT)) {
    int sz = 128; Uint32 *p = SDL_malloc(sz * sz * 4); SDL_memset(p, 0, sz * sz * 4);
    DrawFighterToBuffer(p, sz, 888.8f);
    s->textures.fighter_texture = SDL_CreateTexture(s->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, sz, sz);
    SDL_SetTextureBlendMode(s->textures.fighter_texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(s->textures.fighter_texture, SDL_SCALEMODE_NEAREST);
    SDL_UpdateTexture(s->textures.fighter_texture, NULL, p, sz * 4); SDL_free(p);
  } else if (s->assets_generated >= total_assets - ICON_COUNT) {
      int icon_idx = s->assets_generated - (total_assets - ICON_COUNT);
      int sz = 128; Uint32 *p = SDL_malloc(sz * sz * 4); SDL_memset(p, 0, sz * sz * 4);
      DrawIconToBuffer(p, sz, icon_idx);
      s->textures.icon_textures[icon_idx] = SDL_CreateTexture(s->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, sz, sz);
      SDL_SetTextureBlendMode(s->textures.icon_textures[icon_idx], SDL_BLENDMODE_BLEND);
      SDL_SetTextureScaleMode(s->textures.icon_textures[icon_idx], SDL_SCALEMODE_NEAREST);
      SDL_UpdateTexture(s->textures.icon_textures[icon_idx], NULL, p, sz * 4); SDL_free(p);
  }
  
  s->assets_generated++;
  if (s->assets_generated >= total_assets) s->game_state = STATE_GAME;
}

void Asset_DrawLoading(AppState *s) {
  int win_w, win_h; SDL_GetRenderLogicalPresentation(s->renderer, &win_w, &win_h, NULL);
  if (win_w == 0 || win_h == 0) SDL_GetRenderOutputSize(s->renderer, &win_w, &win_h);
  SDL_SetRenderDrawColor(s->renderer, 0, 0, 0, 255); SDL_RenderClear(s->renderer);
  int total_assets = PLANET_COUNT + GALAXY_COUNT + ASTEROID_TYPE_COUNT + CRYSTAL_COUNT + DEBRIS_COUNT + 4 + ICON_COUNT;
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
