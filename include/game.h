#ifndef GAME_H
#define GAME_H

#include "structs.h"

// System functions
void Game_Update(AppState *s, float dt);
void Renderer_Init(AppState *s);
void Renderer_Draw(AppState *s);
void Input_ProcessEvent(AppState *s, SDL_Event *event);

// Math helpers
Vec2 Vector_Sub(Vec2 a, Vec2 b);
float Vector_Length(Vec2 v);
Vec2 Vector_Normalize(Vec2 v);
float Noise2D(float x, float y);

#endif