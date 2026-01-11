#include "game.h"
#include <math.h>

Vec2 Vector_Sub(Vec2 a, Vec2 b) {
    return (Vec2){ a.x - b.x, a.y - b.y };
}

float Vector_Length(Vec2 v) {
    return sqrtf(v.x * v.x + v.y * v.y);
}

Vec2 Vector_Normalize(Vec2 v) {
    float len = Vector_Length(v);
    if (len == 0) return (Vec2){ 0, 0 };
    return (Vec2){ v.x / len, v.y / len };
}

// --- Noise Implementation ---

float DeterministicHash(int x, int y) {
    // Simple integer hash
    unsigned int h = (unsigned int)x * 374761393u + (unsigned int)y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (float)(h & 0x7FFFFFFF) / (float)0x7FFFFFFF;
}

// Simple pseudo-random hash for noise
static float Hash(float x, float y) {
    float dot = x * 12.9898f + y * 78.233f;
    float sin_val = sinf(dot) * 43758.5453f;
    return sin_val - floorf(sin_val);
}

// Smooth interpolation (Quintic)
static float Smooth(float t) {
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

// 2D Value Noise
float Noise2D(float x, float y) {
    float i_x = floorf(x);
    float i_y = floorf(y);
    float f_x = x - i_x;
    float f_y = y - i_y;
    float a = Hash(i_x,     i_y);
    float b = Hash(i_x + 1, i_y);
    float c = Hash(i_x,     i_y + 1);
    float d = Hash(i_x + 1, i_y + 1);
    float u = Smooth(f_x);
    float v = Smooth(f_y);
    return (a * (1 - u) + b * u) * (1 - v) + (c * (1 - u) + d * u) * v;
}