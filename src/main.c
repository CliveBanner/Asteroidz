#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "game.h"
#include "constants.h"

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    AppState *s = SDL_calloc(1, sizeof(AppState));
    if (!s) return SDL_APP_FAILURE;
    *appstate = s;

    if (!SDL_Init(SDL_INIT_VIDEO)) return SDL_APP_FAILURE;

    if (!SDL_CreateWindowAndRenderer("Asteriodz RTS", WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_FULLSCREEN, &s->window, &s->renderer)) {
        return SDL_APP_FAILURE;
    }

    // Initialize Camera at 0, 0
    s->zoom = 1.0f;
    s->camera_pos.x = 0.0f;
    s->camera_pos.y = 0.0f;
    s->show_grid = true;

    Game_Init(s);
    Renderer_Init(s);

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    AppState *s = (AppState *)appstate;
    if (event->type == SDL_EVENT_QUIT) return SDL_APP_SUCCESS;
    
    if (event->type == SDL_EVENT_KEY_DOWN && event->key.key == SDLK_ESCAPE) {
        return SDL_APP_SUCCESS;
    }
    
    Input_ProcessEvent(s, event);
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    AppState *s = (AppState *)appstate;
    static Uint64 last_time = 0;
    static Uint64 fps_timer = 0;
    static int frame_count = 0;
    
    if (last_time == 0) {
        last_time = SDL_GetTicks();
        fps_timer = last_time;
    }
    Uint64 now = SDL_GetTicks();
    float dt = (now - last_time) / 1000.0f;
    last_time = now;

    // FPS Calculation
    frame_count++;
    Uint64 elapsed = now - fps_timer;
    if (elapsed >= 1000) {
        s->current_fps = (float)frame_count * (1000.0f / (float)elapsed);
        frame_count = 0;
        fps_timer = now;
    }

    s->current_time += dt;

    if (s->is_loading) {
        Renderer_GeneratePlanetStep(s);
        Renderer_DrawLoading(s);
    } else {
        Game_Update(s, dt);
        Renderer_Draw(s);
    }

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    AppState *s = (AppState *)appstate;
    if (s) {
        // Stop background thread
        if (s->bg_thread) {
            SDL_SetAtomicInt(&s->bg_should_quit, 1);
            SDL_WaitThread(s->bg_thread, NULL);
        }
        if (s->bg_mutex) {
            SDL_DestroyMutex(s->bg_mutex);
        }
        if (s->bg_pixel_buffer) {
            SDL_free(s->bg_pixel_buffer);
        }
        
        SDL_free(s);
    }
}