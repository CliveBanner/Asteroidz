#include "constants.h"
#include "game.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

Vec2 Vector_Add(Vec2 a, Vec2 b) { return (Vec2){a.x + b.x, a.y + b.y}; }
Vec2 Vector_Sub(Vec2 a, Vec2 b) { return (Vec2){a.x - b.x, a.y - b.y}; }
Vec2 Vector_Scale(Vec2 v, float s) { return (Vec2){v.x * s, v.y * s}; }
float Vector_Length(Vec2 v) { return sqrtf(v.x * v.x + v.y * v.y); }
float Vector_DistanceSq(Vec2 a, Vec2 b) {
  float dx = a.x - b.x, dy = a.y - b.y;
  return dx * dx + dy * dy;
}

float Vector_Distance(Vec2 a, Vec2 b) {
    return sqrtf(Vector_DistanceSq(a, b));
}

Vec2 Vector_Normalize(Vec2 v) {
  float len = Vector_Length(v);
  return (len == 0) ? (Vec2){0, 0} : (Vec2){v.x / len, v.y / len};
}

float DeterministicHash(int x, int y) {
  unsigned int h =
      (unsigned int)x * 0x37476139u + (unsigned int)y * 0x668265263u;
  h ^= h >> 16;
  h *= 0x85ebca6bu;
  h ^= h >> 13;
  h *= 0xc2b2ae35u;
  h ^= h >> 16;
  return (float)(h & 0x7FFFFFFF) / (float)0x7FFFFFFF;
}

// --- Value Noise ---
static float Smooth(float t) {
  return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

static float Hash(float x, float y) {
  float dot = x * 12.9898f + y * 78.233f;
  float sin_val = sinf(dot) * 43758.5453f;
  return sin_val - floorf(sin_val);
}

float ValueNoise2D(float x, float y) {
  float i_x = floorf(x), i_y = floorf(y);
  float f_x = x - i_x, f_y = y - i_y;
  float a = Hash(i_x, i_y), b = Hash(i_x + 1, i_y);
  float c = Hash(i_x, i_y + 1), d = Hash(i_x + 1, i_y + 1);
  float u = Smooth(f_x), v = Smooth(f_y);
  return (a * (1 - u) + b * u) * (1 - v) + (c * (1 - u) + d * u) * v;
}

// --- Optimized 2D Perlin Noise ---
static Vec2 GetGradient2D(int ix, int iy) {
  unsigned int h =
      (unsigned int)ix * 374761393u + (unsigned int)iy * 668265263u;
  h = (h ^ (h >> 13)) * 1274126177u;
  float angle = ((float)(h & 0xFFFF) / 65535.0f) * 6.283185f;
  return (Vec2){cosf(angle), sinf(angle)};
}

static float DotGridGradient2D(int ix, int iy, float x, float y) {
  Vec2 g = GetGradient2D(ix, iy);
  return ((x - (float)ix) * g.x + (y - (float)iy) * g.y);
}

float PerlinNoise2D(float x, float y) {
  int x0 = (int)floorf(x), x1 = x0 + 1;
  int y0 = (int)floorf(y), y1 = y0 + 1;
  float sx = Smooth(x - (float)x0), sy = Smooth(y - (float)y0);
  float n0 = DotGridGradient2D(x0, y0, x, y),
        n1 = DotGridGradient2D(x1, y0, x, y);
  float ix0 = n0 * (1.0f - sx) + n1 * sx;
  n0 = DotGridGradient2D(x0, y1, x, y);
  n1 = DotGridGradient2D(x1, y1, x, y);
  float ix1 = n0 * (1.0f - sx) + n1 * sx;
  return (ix0 * (1.0f - sy) + ix1 * sy) * 0.5f + 0.5f;
}

// --- Voronoi / Cellular Noise ---
float VoronoiNoise2D(float x, float y) {
  int ix = (int)floorf(x);
  int iy = (int)floorf(y);
  float minDist = 1.0f;

  for (int v = -1; v <= 1; v++) {
    for (int u = -1; u <= 1; u++) {
      float hx = DeterministicHash(ix + u, iy + v);
      float hy = DeterministicHash(ix + u + 123, iy + v + 456);
      float targetX = (float)(ix + u) + hx;
      float targetY = (float)(iy + v) + hy;
      float dx = targetX - x;
      float dy = targetY - y;
      float dist = sqrtf(dx * dx + dy * dy);
      if (dist < minDist)
        minDist = dist;
    }
  }
  return minDist;
}

float VoronoiCracks2D(float x, float y) {
  int ix = (int)floorf(x);
  int iy = (int)floorf(y);
  float f1 = 8.0f, f2 = 8.0f;

  for (int v = -1; v <= 1; v++) {
    for (int u = -1; u <= 1; u++) {
      float hx = DeterministicHash(ix + u, iy + v);
      float hy = DeterministicHash(ix + u + 123, iy + v + 456);
      float dx = (float)(ix + u) + hx - x;
      float dy = (float)(iy + v) + hy - y;
      float d = dx * dx + dy * dy;

      if (d < f1) {
        f2 = f1;
        f1 = d;
      } else if (d < f2) {
        f2 = d;
      }
    }
  }
  return sqrtf(f2) - sqrtf(f1);
}

bool GetCelestialBodyInfo(int gx, int gy, Vec2 *out_pos, float *out_type_seed,
                          float *out_radius) {

  float seed = DeterministicHash(gx, gy + 1000);
  if (seed > 0.98f || (gx == 0 && gy == 0)) {
    float jx = (DeterministicHash(gx + 5, gy + 9) - 0.5f) * 2000.0f;
    float jy = (DeterministicHash(gx + 12, gy + 3) - 0.5f) * 2000.0f;
    out_pos->x =
        (float)gx * CELESTIAL_GRID_SIZE_F + (CELESTIAL_GRID_SIZE_F / 2.0f) + jx;
    out_pos->y =
        (float)gy * CELESTIAL_GRID_SIZE_F + (CELESTIAL_GRID_SIZE_F / 2.0f) + jy;
    *out_type_seed = DeterministicHash(gx, gy + 2000);
        // Calculate Radius
        if (*out_type_seed > 0.95f) {
            float r_s = DeterministicHash(gx + 3, gy + 5);
            *out_radius = GALAXY_RADIUS_MIN + r_s * GALAXY_RADIUS_VARIANCE;
        } else {
            float r_s = DeterministicHash(gx + 7, gy + 11);
            *out_radius = PLANET_RADIUS_MIN + r_s * PLANET_RADIUS_VARIANCE;
        }
        return true;
      }  return false;
}

Vec2 WorldToParallax(Vec2 world_pos, float parallax) {
  return (Vec2){world_pos.x * parallax, world_pos.y * parallax};
}

float GetAsteroidDensity(Vec2 p) {
  float val = DENSITY_BASELINE;
  const float body_grid = CELESTIAL_GRID_SIZE_F;

  int gx_center = (int)floorf(p.x / body_grid);
  int gy_center = (int)floorf(p.y / body_grid);

  // Significantly expanded search range for the new massive galaxies
  for (int oy = -10; oy <= 10; oy++) {
    for (int ox = -10; ox <= 10; ox++) {
      Vec2 b_pos;
      float b_type;
      float b_radius;
      if (GetCelestialBodyInfo(gx_center + ox, gy_center + oy, &b_pos, &b_type,
                               &b_radius)) {
        float dx = p.x - b_pos.x, dy = p.y - b_pos.y;
        float dist = sqrtf(dx * dx + dy * dy);

        if (b_type > 0.95f) { // Galaxy
          float inner_r = b_radius * GALAXY_BELT_INNER_MULT;
          float outer_r = b_radius * GALAXY_BELT_OUTER_MULT;
          float mid_r = (inner_r + outer_r) * 0.5f;
          float half_width = (outer_r - inner_r) * 0.5f;
          
          float delta = fabsf(dist - mid_r);
          if (delta < half_width) {
              // Smooth cosine falloff
              float falloff = 0.5f + 0.5f * cosf((delta / half_width) * 3.14159f);
              val += DENSITY_GALAXY_WEIGHT * falloff;
          }
        } else { // Planet
          float inner_r = b_radius * PLANET_BELT_INNER_MULT;
          float outer_r = b_radius * PLANET_BELT_OUTER_MULT;
          float mid_r = (inner_r + outer_r) * 0.5f;
          float half_width = (outer_r - inner_r) * 0.5f;

          float delta = fabsf(dist - mid_r);
          if (delta < half_width) {
              float falloff = 0.5f + 0.5f * cosf((delta / half_width) * 3.14159f);
              val += DENSITY_PLANET_WEIGHT * falloff;
          }
        }
      }
    }
  }

  // --- Global Asteroid Belts (Linear) ---
  // Rotate coordinates for primary belts
  float rx = p.x * cosf(GLOBAL_BELT_ANGLE) - p.y * sinf(GLOBAL_BELT_ANGLE);
  float ry = p.x * sinf(GLOBAL_BELT_ANGLE) + p.y * cosf(GLOBAL_BELT_ANGLE);

  float period = 2.0f * 3.14159f;

  // Primary Sine wave bands
  float phase1 = ry * GLOBAL_BELT_FREQ;
  int belt_idx1 = (int)floorf(phase1 / period);
  
  if (DeterministicHash(belt_idx1, 123) < GLOBAL_BELT_KEEP_PROB) {
      float wave = sinf(phase1);
      if (wave > GLOBAL_BELT_WIDTH) {
          val += (wave - GLOBAL_BELT_WIDTH) * (1.0f / (1.0f - GLOBAL_BELT_WIDTH)) * GLOBAL_BELT_WEIGHT;
      }
  }

  // Orthogonal Sine wave bands (rotated 90 degrees relative to primary)
  float phase2 = rx * GLOBAL_BELT_SEC_FREQ;
  int belt_idx2 = (int)floorf(phase2 / period);

  if (DeterministicHash(belt_idx2, 456) < GLOBAL_BELT_KEEP_PROB) {
      float wave2 = sinf(phase2);
      if (wave2 > GLOBAL_BELT_SEC_WIDTH) {
          val += (wave2 - GLOBAL_BELT_SEC_WIDTH) * (1.0f / (1.0f - GLOBAL_BELT_SEC_WIDTH)) * GLOBAL_BELT_SEC_WEIGHT;
      }
  }
  
  if (val <= 0.0001f)
    return 0.0f;

  // Return normalized 0-1
  float norm = val / DENSITY_MAX;

  // Add noise for less uniform distribution (only subtractive)
  float noise_val = PerlinNoise2D(p.x * 0.0005f, p.y * 0.0005f); // 0.0 to 1.0
  norm += norm * (noise_val - 1.0f) * 0.5f; // -0.5 to 0.0 contribution

  return fmaxf(0.0f, fminf(1.0f, norm));
}

void LogTransaction(AppState *s, float val, const char *label) {
    for (int i = MAX_LOGS - 1; i > 0; i--) {
        s->ui.transaction_log[i] = s->ui.transaction_log[i - 1];
    }
    s->ui.transaction_log[0].val = val;
    SDL_strlcpy(s->ui.transaction_log[0].label, label, 32);
    s->ui.transaction_log[0].life = 5.0f;
}

void Utils_DrawCircle(SDL_Renderer *r, float cx, float cy, float radius, int segments) {
    float angle_step = (2.0f * 3.14159f) / (float)segments;
    for (int i = 0; i < segments; i++) {
        float a1 = (float)i * angle_step;
        float a2 = (float)(i + 1) * angle_step;
        SDL_RenderLine(r, cx + cosf(a1) * radius, cy + sinf(a1) * radius, cx + cosf(a2) * radius, cy + sinf(a2) * radius);
    }
}
