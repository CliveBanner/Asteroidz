#ifndef STRUCTS_H
#define STRUCTS_H

#include <SDL3/SDL.h>
#include <stdbool.h>
#include "constants.h"

typedef struct {
    float x, y;
} Vec2;

typedef enum {
    STATE_LAUNCHER,
    STATE_GAME
} GameState;

typedef struct {
    int selected_res_index; // 0: 1280x720, 1: 1920x1080
    bool fullscreen;
    bool start_hovered;
    bool res_hovered;
    bool fs_hovered;
} LauncherState;

typedef enum {
    PARTICLE_SPARK,
    PARTICLE_PUFF
} ParticleType;

typedef struct {
    Vec2 pos;
    Vec2 velocity;
    float life;       // 1.0 down to 0.0
    float size;
    SDL_Color color;
    ParticleType type;
    bool active;
} Particle;

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
    // State
    GameState state;
    LauncherState launcher;

    // Camera
    Vec2 camera_pos;      // Top-left position of the camera in the world
    float zoom;
    
    // Input State
    Vec2 mouse_pos;       // Screen coordinates
    bool show_grid;
    bool show_density;
    
    // Game Entities
    Asteroid asteroids[MAX_ASTEROIDS];
    int asteroid_count;

    Particle particles[MAX_PARTICLES];
    int particle_next_idx;

    // Rendering
    SDL_Texture *bg_texture;
    SDL_Texture *explosion_puff_texture;
    int bg_w, bg_h;
    float current_fps;
    float current_time;

    SDL_Texture *planet_textures[PLANET_COUNT];
    SDL_Texture *galaxy_textures[GALAXY_COUNT];
    SDL_Texture *asteroid_textures[ASTEROID_TYPE_COUNT];
    
    bool is_loading;
    int assets_generated;

    // Background Threading (Nebula)
    SDL_Thread *bg_thread;
    SDL_Mutex *bg_mutex;
    SDL_AtomicInt bg_should_quit;
    SDL_AtomicInt bg_request_update;
    SDL_AtomicInt bg_data_ready;
    
    Uint32 *bg_pixel_buffer;
    Vec2 bg_target_cam_pos;
    float bg_target_zoom;
    float bg_target_time;

    // Density Threading
    SDL_Thread *density_thread;
    SDL_Mutex *density_mutex;
    SDL_AtomicInt density_should_quit;
    SDL_AtomicInt density_request_update;
    SDL_AtomicInt density_data_ready;

    Uint32 *density_pixel_buffer;
    int density_w, density_h;
    Vec2 density_target_cam_pos;
    Vec2 density_texture_cam_pos; // Position of the currently uploaded texture
    SDL_Texture *density_texture;

    SDL_Renderer *renderer;
    SDL_Window *window;
} AppState;

#endif
