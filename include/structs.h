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
    bool show_grid;
    
    // Rendering
    SDL_Texture *bg_texture;
    int bg_w, bg_h;
    float current_fps;

    // Background Threading
    SDL_Thread *bg_thread;
    SDL_Mutex *bg_mutex;
    SDL_AtomicInt bg_should_quit; // Flag to stop the thread
    SDL_AtomicInt bg_request_update; // Flag to signal thread to work
    SDL_AtomicInt bg_data_ready; // Flag to signal main thread to upload
    
    Uint32 *bg_pixel_buffer; // RAM buffer shared between threads
    Vec2 bg_target_cam_pos; // The camera pos the thread should generate for
    float bg_target_zoom;   // The zoom level the thread should generate for

    SDL_Renderer *renderer;
    SDL_Window *window;
} AppState;

#endif
