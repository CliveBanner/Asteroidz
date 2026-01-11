#ifndef GAME_H
#define GAME_H

#include "structs.h"

// System functions
void Game_Init(AppState *s);
void Game_Update(AppState *s, float dt);
float GetAsteroidDensity(Vec2 p, Vec2 cam_center);
bool GetCelestialBodyInfo(int gx, int gy, Vec2 *out_pos, float *out_type_seed);
Vec2 WorldToParallax(Vec2 world_pos, float parallax);
void Renderer_Init(AppState *s);
void Renderer_GeneratePlanetStep(AppState *s);
void Renderer_DrawLoading(AppState *s);
void Renderer_Draw(AppState *s);
void Input_ProcessEvent(AppState *s, SDL_Event *event);

// Math helpers
Vec2 Vector_Sub(Vec2 a, Vec2 b);
float Vector_Length(Vec2 v);
Vec2 Vector_Normalize(Vec2 v);
float ValueNoise2D(float x, float y);
float PerlinNoise2D(float x, float y);
float VoronoiNoise2D(float x, float y);
float VoronoiCracks2D(float x, float y);
float DeterministicHash(int x, int y);

#endif