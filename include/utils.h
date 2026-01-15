#ifndef UTILS_H
#define UTILS_H

#include "structs.h"

// Vector Math
Vec2 Vector_Add(Vec2 a, Vec2 b);
Vec2 Vector_Sub(Vec2 a, Vec2 b);
Vec2 Vector_Scale(Vec2 v, float s);
float Vector_Length(Vec2 v);
float Vector_DistanceSq(Vec2 a, Vec2 b);
Vec2 Vector_Normalize(Vec2 v);

// Noise & Hashing
float DeterministicHash(int x, int y);
float ValueNoise2D(float x, float y);
float PerlinNoise2D(float x, float y);
float VoronoiNoise2D(float x, float y);
float VoronoiCracks2D(float x, float y);

// World Info
bool GetCelestialBodyInfo(int gx, int gy, Vec2 *out_pos, float *out_type_seed, float *out_radius);
float GetAsteroidDensity(Vec2 p);
Vec2 WorldToParallax(Vec2 world_pos, float parallax);

#endif
