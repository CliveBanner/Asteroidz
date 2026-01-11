#include "game.h"
#include <math.h>

Vec2 Vector_Sub(Vec2 a, Vec2 b) { return (Vec2){ a.x - b.x, a.y - b.y }; }
float Vector_Length(Vec2 v) { return sqrtf(v.x * v.x + v.y * v.y); }
Vec2 Vector_Normalize(Vec2 v) { float len = Vector_Length(v); return (len == 0) ? (Vec2){0,0} : (Vec2){v.x/len, v.y/len}; }

float DeterministicHash(int x, int y) {
    unsigned int h = (unsigned int)x * 374761393u + (unsigned int)y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
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
    unsigned int h = (unsigned int)ix * 374761393u + (unsigned int)iy * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    float angle = ((float)(h & 0xFFFF) / 65535.0f) * 6.283185f;
    return (Vec2){ cosf(angle), sinf(angle) };
}

static float DotGridGradient2D(int ix, int iy, float x, float y) {
    Vec2 g = GetGradient2D(ix, iy);
    return ((x - (float)ix) * g.x + (y - (float)iy) * g.y);
}

float PerlinNoise2D(float x, float y) {
    int x0 = (int)floorf(x), x1 = x0 + 1;
    int y0 = (int)floorf(y), y1 = y0 + 1;
    float sx = Smooth(x - (float)x0), sy = Smooth(y - (float)y0);
    float n0 = DotGridGradient2D(x0, y0, x, y), n1 = DotGridGradient2D(x1, y0, x, y);
    float ix0 = n0 * (1.0f - sx) + n1 * sx;
    n0 = DotGridGradient2D(x0, y1, x, y); n1 = DotGridGradient2D(x1, y1, x, y);
    float ix1 = n0 * (1.0f - sx) + n1 * sx;
    return (ix0 * (1.0f - sy) + ix1 * sy) * 0.5f + 0.5f;
}

// --- Perlin 3D ---
typedef struct { float x, y, z; } Vec3;

static Vec3 GetGradient3D(int ix, int iy, int iz) {
    unsigned int h = (unsigned int)ix * 374761393u + (unsigned int)iy * 668265263u + (unsigned int)iz * 1274126177u;
    h = (h ^ (h >> 13)) * 1274126177u;
    float longitude = ((float)(h & 0xFFFF) / 65535.0f) * 6.283185f;
    float latitude = ((float)((h >> 16) & 0xFFFF) / 65535.0f) * 3.141592f - 1.570796f;
    return (Vec3){ cosf(latitude) * cosf(longitude), cosf(latitude) * sinf(longitude), sinf(latitude) };
}

static float DotGridGradient3D(int ix, int iy, int iz, float x, float y, float z) {
    Vec3 g = GetGradient3D(ix, iy, iz);
    return ((x - (float)ix) * g.x + (y - (float)iy) * g.y + (z - (float)iz) * g.z);
}

float PerlinNoise3D(float x, float y, float z) {
    int x0 = (int)floorf(x), x1 = x0 + 1;
    int y0 = (int)floorf(y), y1 = y0 + 1;
    int z0 = (int)floorf(z), z1 = z0 + 1;
    float sx = Smooth(x - (float)x0), sy = Smooth(y - (float)y0), sz = Smooth(z - (float)z0);
    float n0, n1, ix0, ix1, iy0, iy1;
    
    n0 = DotGridGradient3D(x0, y0, z0, x, y, z); n1 = DotGridGradient3D(x1, y0, z0, x, y, z); ix0 = n0*(1-sx)+n1*sx;
    n0 = DotGridGradient3D(x0, y1, z0, x, y, z); n1 = DotGridGradient3D(x1, y1, z0, x, y, z); ix1 = n0*(1-sx)+n1*sx;
    iy0 = ix0*(1-sy)+ix1*sy;
    
    n0 = DotGridGradient3D(x0, y0, z1, x, y, z); n1 = DotGridGradient3D(x1, y0, z1, x, y, z); ix0 = n0*(1-sx)+n1*sx;
    n0 = DotGridGradient3D(x0, y1, z1, x, y, z); n1 = DotGridGradient3D(x1, y1, z1, x, y, z); ix1 = n0*(1-sx)+n1*sx;
    iy1 = ix0*(1-sy)+ix1*sy;
    
    return (iy0 * (1.0f - sz) + iy1 * sz) * 0.5f + 0.5f;
}