#define SDL_MAIN_USE_CALLBACKS 1
#include "constants.h"
#include "game.h"
#include "input.h"
#include "ai.h"
#include "assets.h"
#include "ui.h"
#include "workers.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
  AppState *s = SDL_calloc(1, sizeof(AppState));
  if (!s)
    return SDL_APP_FAILURE;
  *appstate = s;

  if (!SDL_Init(SDL_INIT_VIDEO))
    return SDL_APP_FAILURE;

  // Start in Windowed mode for Launcher
  int init_w = 800;
  int init_h = 600;
  if (!SDL_CreateWindowAndRenderer("Asteriodz Launcher", init_w, init_h,
                                   SDL_WINDOW_RESIZABLE, &s->window,
                                   &s->renderer)) {
    return SDL_APP_FAILURE;
  }

  SDL_SetRenderLogicalPresentation(s->renderer, init_w, init_h, SDL_LOGICAL_PRESENTATION_LETTERBOX);

  s->game_state = STATE_LAUNCHER;
  s->launcher.selected_res_index = 0; // Default 1280x720
  s->launcher.fullscreen = false;

  // Initialize Camera at 0, 0
  s->camera.zoom = 1.0f;
  s->camera.pos.x = 0.0f;
  s->camera.pos.y = 0.0f;
  s->input.show_grid = true;
  s->selection.primary_unit_idx = -1;

  Game_Init(s);
  Renderer_Init(s); // Only sets up textures, doesn't start threads yet

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
  AppState *s = (AppState *)appstate;
  if (event->type == SDL_EVENT_QUIT)
    return SDL_APP_SUCCESS;

  if (s->game_state == STATE_LAUNCHER) {
      if (event->type == SDL_EVENT_MOUSE_MOTION) {
          float mx = event->motion.x;
          float my = event->motion.y;
          int w, h;
          SDL_GetRenderOutputSize(s->renderer, &w, &h);
          float cx = w / 2.0f;
          float cy = h / 2.0f;

          s->launcher.res_hovered = (mx >= cx - 150 && mx <= cx + 150 && my >= cy - 20 && my <= cy + 20);
          s->launcher.fs_hovered = (mx >= cx - 150 && mx <= cx + 150 && my >= cy + 40 && my <= cy + 80);
          s->launcher.start_hovered = (mx >= cx - 150 && mx <= cx + 150 && my >= cy + 120 && my <= cy + 170);
      }
      else if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
          if (s->launcher.res_hovered) {
              s->launcher.selected_res_index = (s->launcher.selected_res_index + 1) % 2;
          }
          if (s->launcher.fs_hovered) {
              s->launcher.fullscreen = !s->launcher.fullscreen;
          }
          if (s->launcher.start_hovered) {
              // Apply Settings
              int target_w = (s->launcher.selected_res_index == 0) ? 1280 : 1920;
              int target_h = (s->launcher.selected_res_index == 0) ? 720 : 1080;
              
              if (s->launcher.fullscreen) {
                  SDL_SetWindowFullscreen(s->window, true);
              } else {
                  SDL_SetWindowSize(s->window, target_w, target_h);
              }
              
              // Ensure window state is applied before setting logical presentation
              SDL_SyncWindow(s->window);
              SDL_SetRenderLogicalPresentation(s->renderer, target_w, target_h, SDL_LOGICAL_PRESENTATION_LETTERBOX);
              
              SDL_SetWindowTitle(s->window, "Asteriodz RTS");
              
              // Start Game
              Workers_Start(s);
              AI_StartThreads(s);
              s->game_state = STATE_LOADING;
          }
      }
      return SDL_APP_CONTINUE;
  }

  if (s->game_state == STATE_LOADING && event->type == SDL_EVENT_KEY_DOWN && event->key.key == SDLK_ESCAPE) {
      return SDL_APP_SUCCESS;
  }

  Input_ProcessEvent(s, event);
  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
  AppState *s = (AppState *)appstate;
  
  if (s->game_state == STATE_LAUNCHER) {
      UI_DrawLauncher(s);
      return SDL_APP_CONTINUE;
  }

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

  if (s->game_state == STATE_LOADING) {
    Asset_GenerateStep(s);
    Asset_DrawLoading(s);
  } else if (s->game_state == STATE_GAME || s->game_state == STATE_PAUSED || s->game_state == STATE_GAMEOVER) {
    Game_Update(s, dt);
    Renderer_Draw(s);
  }

  return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
  AppState *s = (AppState *)appstate;
  if (s) {
    // Stop all background threads
    if (s->threads.bg_thread) {
      SDL_SetAtomicInt(&s->threads.bg_should_quit, 1);
      SDL_WaitThread(s->threads.bg_thread, NULL);
    }
    if (s->threads.density_thread) {
      SDL_SetAtomicInt(&s->threads.density_should_quit, 1);
      SDL_WaitThread(s->threads.density_thread, NULL);
    }
    if (s->threads.radar_thread) {
      SDL_SetAtomicInt(&s->threads.radar_should_quit, 1);
      SDL_WaitThread(s->threads.radar_thread, NULL);
    }
    if (s->threads.unit_fx_thread) {
      SDL_SetAtomicInt(&s->threads.unit_fx_should_quit, 1);
      SDL_WaitThread(s->threads.unit_fx_thread, NULL);
    }

    // Destroy all mutexes
    if (s->threads.bg_mutex) SDL_DestroyMutex(s->threads.bg_mutex);
    if (s->threads.density_mutex) SDL_DestroyMutex(s->threads.density_mutex);
    if (s->threads.radar_mutex) SDL_DestroyMutex(s->threads.radar_mutex);
    if (s->threads.unit_fx_mutex) SDL_DestroyMutex(s->threads.unit_fx_mutex);

    // Free all pixel buffers
    if (s->threads.bg_pixel_buffer) SDL_free(s->threads.bg_pixel_buffer);
    if (s->threads.density_pixel_buffer) SDL_free(s->threads.density_pixel_buffer);
    if (s->threads.mothership_hull_buffer) SDL_free(s->threads.mothership_hull_buffer);
    if (s->threads.mothership_arm_buffer) SDL_free(s->threads.mothership_arm_buffer);

    SDL_free(s);
  }
}
