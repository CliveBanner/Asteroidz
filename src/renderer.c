#include "game.h"
#include "constants.h"
#include <math.h>
#include <stdio.h>

static void DrawGrid(SDL_Renderer *renderer, const AppState *s, int win_w, int win_h) {
    SDL_SetRenderDrawColor(renderer, 40, 40, 50, 255);
    int grid_size = 100;
    
    // Calculate visible grid range based on camera position
    int start_x = (int)floorf(s->camera_pos.x / grid_size) * grid_size;
    int start_y = (int)floorf(s->camera_pos.y / grid_size) * grid_size;
    
    // Vertical lines
    for (float x = start_x; x < s->camera_pos.x + win_w / s->zoom + grid_size; x += grid_size) {
        float screen_x = (x - s->camera_pos.x) * s->zoom;
        SDL_RenderLine(renderer, screen_x, 0, screen_x, (float)win_h);
    }
    
    // Horizontal lines
    for (float y = start_y; y < s->camera_pos.y + win_h / s->zoom + grid_size; y += grid_size) {
        float screen_y = (y - s->camera_pos.y) * s->zoom;
        SDL_RenderLine(renderer, 0, screen_y, (float)win_w, screen_y);
    }
}

static void DrawDebugInfo(SDL_Renderer *renderer, const AppState *s, int win_w) {
    char coords_text[64];
    snprintf(coords_text, sizeof(coords_text), "Cam: %.1f, %.1f (x%.1f)", s->camera_pos.x, s->camera_pos.y, s->zoom);
    
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    // SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE is 8 in SDL3
    float text_x = win_w - (SDL_strlen(coords_text) * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE) - 20;
    float text_y = 20;
    SDL_RenderDebugText(renderer, text_x, text_y, coords_text);
}

void Renderer_Draw(AppState *s) {
    int win_w, win_h;
    SDL_GetRenderOutputSize(s->renderer, &win_w, &win_h);

    // Clear Screen
    SDL_SetRenderDrawColor(s->renderer, 20, 20, 25, 255);
    SDL_RenderClear(s->renderer);

    // Draw Elements
    DrawGrid(s->renderer, s, win_w, win_h);
    DrawDebugInfo(s->renderer, s, win_w);

    // Present
    SDL_RenderPresent(s->renderer);
}
