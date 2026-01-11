#include "game.h"
#include <math.h>

Vec2 Vector_Sub(Vec2 a, Vec2 b) { return (Vec2){a.x - b.x, a.y - b.y}; }
float Vector_Length(Vec2 v) { return sqrtf(v.x * v.x + v.y * v.y); }
Vec2 Vector_Normalize(Vec2 v) {
  float len = Vector_Length(v);
  return (len == 0) ? (Vec2){0, 0} : (Vec2){v.x / len, v.y / len};
}

float DeterministicHash(int x, int y) {
    unsigned int h = (unsigned int)x * 0x37476139u + (unsigned int)y * 0x668265263u;
    h ^= h >> 16;
    h *= 0x85ebca6bu;
    h ^= h >> 13;
    h *= 0xc2b2ae35u;
    h ^= h >> 16;
    return (float)(h & 0x7FFFFFFF) / (float)0x7FFFFFFF;
}

static float Smooth(float t) {
  return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

// --- Value Noise ---
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
            float dist = sqrtf(dx*dx + dy*dy);
            if (dist < minDist) minDist = dist;
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
            float d = dx*dx + dy*dy;

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

bool GetCelestialBodyInfo(int gx, int gy, Vec2 *out_pos, float *out_type_seed) {
    float seed = DeterministicHash(gx, gy + 1000);
    if (seed > 0.98f || (gx == 0 && gy == 0)) {
        float jx = (DeterministicHash(gx + 5, gy + 9) - 0.5f) * 2000.0f;
        float jy = (DeterministicHash(gx + 12, gy + 3) - 0.5f) * 2000.0f;
        out_pos->x = (float)gx * 5000.0f + 2500.0f + jx;
        out_pos->y = (float)gy * 5000.0f + 2500.0f + jy;
        *out_type_seed = DeterministicHash(gx, gy + 2000);
        return true;
    }
    return false;
}

Vec2 WorldToParallax(Vec2 world_pos, float parallax) {
    return (Vec2){ world_pos.x * parallax, world_pos.y * parallax };
}

float GetAsteroidDensity(Vec2 p) {
    float density = 0.02f; 
    const float body_grid = 5000.0f;
    
    // Both p and celestial bodies are in the 1.0 world.
    int gx_center = (int)floorf(p.x / body_grid);
    int gy_center = (int)floorf(p.y / body_grid);
    
    for (int oy = -1; oy <= 1; oy++) { 
        for (int ox = -1; ox <= 1; ox++) {
            Vec2 b_pos; float b_type;
            if (GetCelestialBodyInfo(gx_center + ox, gy_center + oy, &b_pos, &b_type)) {
                float dx = p.x - b_pos.x, dy = p.y - b_pos.y;
                float dist_sq = dx*dx + dy*dy;
                
                if (b_type > 0.95f) { // Galaxy
                    if (dist_sq > 3000.0f*3000.0f && dist_sq < 8000.0f*8000.0f) density += 1.5f;
                } else { // Planet
                    if (dist_sq > 1500.0f*1500.0f && dist_sq < 4500.0f*4500.0f) density += 1.2f;
                }
            }
        }
    }
    return density;
}