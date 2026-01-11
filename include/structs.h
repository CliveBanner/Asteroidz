#ifndef STRUCTS_H
#define STRUCTS_H

#include <SDL3/SDL.h>
#include <stdbool.h>
#include "constants.h"

typedef struct {
    float x, y;
} Vec2;

typedef struct {
    // Camera
    Vec2 camera_pos;      // Top-left position of the camera in the world
    float zoom;
    
    // Input State
    Vec2 mouse_pos;       // Screen coordinates
    
    SDL_Renderer *renderer;
    SDL_Window *window;
} AppState;

#endif
