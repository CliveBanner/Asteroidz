#ifndef STRUCTS_H
#define STRUCTS_H

#include <SDL3/SDL.h>
#include <stdbool.h>
#include "constants.h"

typedef struct {
    float x, y;
} Vec2;

#define PLANET_COUNT 8
#define GALAXY_COUNT 4
#define ASTEROID_TYPE_COUNT 16
#define MAX_ASTEROIDS 1024

typedef struct {
    Vec2 pos;
    Vec2 velocity;
    float radius;
    float rotation;
    float rot_speed;
    int tex_idx;
    bool active;
} Asteroid;

typedef struct {
    // Camera
    Vec2 camera_pos;      // Top-left position of the camera in the world
    float zoom;
    
    // Input State
    Vec2 mouse_pos;       // Screen coordinates
    bool show_grid;
    
    // Game Entities
    Asteroid asteroids[MAX_ASTEROIDS];
    int asteroid_count;

    // Rendering
    SDL_Texture *bg_texture;
    int bg_w, bg_h;
    float current_fps;
    float current_time;

    SDL_Texture *planet_textures[PLANET_COUNT];
    SDL_Texture *galaxy_textures[GALAXY_COUNT];
    SDL_Texture *asteroid_textures[ASTEROID_TYPE_COUNT];
    
    bool is_loading;
    int assets_generated;

    // Background Threading
    SDL_Thread *bg_thread;
    SDL_Mutex *bg_mutex;
    SDL_AtomicInt bg_should_quit;
    SDL_AtomicInt bg_request_update;
    SDL_AtomicInt bg_data_ready;
    
    Uint32 *bg_pixel_buffer;
    Vec2 bg_target_cam_pos;
    float bg_target_zoom;
    float bg_target_time;

    SDL_Renderer *renderer;
    SDL_Window *window;
} AppState;

#endif
