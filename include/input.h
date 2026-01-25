#ifndef INPUT_H
#define INPUT_H

#include <SDL3/SDL.h>
#include "structs.h"

void Input_ProcessEvent(AppState *s, SDL_Event *event);

// Internal modular functions (could be static in input.c, but exposed here for clarity if needed)
void Input_HandleMouseWheel(AppState *s, SDL_MouseWheelEvent *event);
void Input_HandleMouseMove(AppState *s, SDL_MouseMotionEvent *event);
void Input_HandleMouseButtonDown(AppState *s, SDL_MouseButtonEvent *event);
void Input_HandleMouseButtonUp(AppState *s, SDL_MouseButtonEvent *event);
void Input_HandleKeyDown(AppState *s, SDL_KeyboardEvent *event);
void Input_HandleKeyUp(AppState *s, SDL_KeyboardEvent *event);

void Input_ScheduleCommand(AppState *s, Command cmd, bool queue);

#endif
