#include "workers.h"
#include "constants.h"
#include "utils.h"
#include <math.h>

typedef struct {
  float pos;
  float r, g, b;
} ColorStop;

static void GetNebulaColor(float t, float *r, float *g, float *b) {
  static const ColorStop stops[] = {{0.00f, 2, 2, 6}, {0.40f, 10, 10, 25}, {0.70f, 25, 15, 45}, {0.90f, 35, 60, 80}, {1.00f, 50, 80, 100}};
  if (t <= stops[0].pos) { *r = stops[0].r; *g = stops[0].g; *b = stops[0].b; return; }
  for (int i = 0; i < 4; i++) {
    if (t >= stops[i].pos && t <= stops[i + 1].pos) {
      float s_t = (t - stops[i].pos) / (stops[i + 1].pos - stops[i].pos);
      float st = s_t * s_t * (3.0f - 2.0f * s_t);
      *r = stops[i].r * (1 - st) + stops[i + 1].r * st;
      *g = stops[i].g * (1 - st) + stops[i + 1].g * st;
      *b = stops[i].b * (1 - st) + stops[i + 1].b * st;
      return;
    }
  }
}

static int SDLCALL BackgroundGenerationThread(void *data) {
  AppState *s = (AppState *)data;
  while (SDL_GetAtomicInt(&s->threads.bg_should_quit) == 0) {
    if (SDL_GetAtomicInt(&s->threads.bg_request_update) == 1) {
      SDL_LockMutex(s->threads.bg_mutex); Vec2 cam_pos = s->threads.bg_target_cam_pos; float zoom = s->threads.bg_target_zoom, time = s->threads.bg_target_time; SDL_UnlockMutex(s->threads.bg_mutex);
      for (int i = 0; i < s->textures.bg_w * s->textures.bg_h; i++) {
        int x = i % s->textures.bg_w, y = i / s->textures.bg_w; float wx = cam_pos.x + (x * BG_SCALE_FACTOR) / zoom, wy = cam_pos.y + (y * BG_SCALE_FACTOR) / zoom;
        float n = ValueNoise2D(wx * 0.0002f + time * 0.05f, wy * 0.0002f + time * 0.03f) * 0.6f + ValueNoise2D(wx * 0.001f + time * 0.15f, wy * 0.001f + time * 0.1f) * 0.3f + ValueNoise2D(wx * 0.003f + time * 0.15f, wy * 0.003f + time * 0.1f) * 0.1f;
        float variation = ValueNoise2D(wx * 0.00001f + 12345.0f, wy * 0.00001f + 67890.0f);
        float rf, gf, bf; GetNebulaColor(fminf(1.0f, n), &rf, &gf, &bf); rf *= (0.95f + variation * 0.1f); gf *= (0.95f + (1.0f - variation) * 0.1f);
        float intensity = 0.5f + n * 0.5f; Uint8 r = (Uint8)fminf(rf * intensity, 255.0f), g = (Uint8)fminf(gf * intensity, 255.0f), b = (Uint8)fminf(bf * intensity, 255.0f);
        Uint8 a = (Uint8)(fmaxf(0.0f, fminf(1.0f, (n - 0.1f) / 0.6f)) * 160);
        s->threads.bg_pixel_buffer[i] = (a << 24) | (b << 16) | (g << 8) | r;
      }
      SDL_SetAtomicInt(&s->threads.bg_request_update, 0); SDL_SetAtomicInt(&s->threads.bg_data_ready, 1);
    } else SDL_Delay(10);
  }
  return 0;
}

static int SDLCALL DensityGenerationThread(void *data) {
  AppState *s = (AppState *)data;
  while (SDL_GetAtomicInt(&s->threads.density_should_quit) == 0) {
    if (SDL_GetAtomicInt(&s->threads.density_request_update) == 1) {
      SDL_LockMutex(s->threads.density_mutex); Vec2 cam_pos = s->threads.density_target_cam_pos; SDL_UnlockMutex(s->threads.density_mutex);
      float range = MINIMAP_RANGE, start_x = cam_pos.x - range / 2.0f, start_y = cam_pos.y - range / 2.0f, cell_sz = (float)DENSITY_CELL_SIZE / (float)GRID_DENSITY_SUB_RES;
      for (int i = 0; i < s->threads.density_w * s->threads.density_h; i++) {
          int x = i % s->threads.density_w, y = i / s->threads.density_w; float d = GetAsteroidDensity((Vec2){start_x + (float)x * cell_sz, start_y + (float)y * cell_sz});
          s->threads.density_pixel_buffer[i] = (d > 0.01f) ? (((Uint8)fminf(40.0f, d * 60.0f + 5.0f)) << 24) | ((Uint8)fminf(255.0f, d * 255.0f)) : 0;
      }
      SDL_SetAtomicInt(&s->threads.density_request_update, 0); SDL_SetAtomicInt(&s->threads.density_data_ready, 1);
    } else SDL_Delay(20);
  }
  return 0;
}

void Workers_Start(AppState *s) {
  SDL_SetAtomicInt(&s->threads.bg_request_update, 1);
  s->threads.bg_thread = SDL_CreateThread(BackgroundGenerationThread, "BG_Gen", s);
  s->threads.density_thread = SDL_CreateThread(DensityGenerationThread, "Density_Gen", s);
}

void Workers_UpdateBackground(AppState *s) {
  if (!s->textures.bg_texture) return;
  if (SDL_GetAtomicInt(&s->threads.bg_data_ready) == 1) {
    void *pixels; int pitch;
    if (SDL_LockTexture(s->textures.bg_texture, NULL, &pixels, &pitch)) {
      for (int i = 0; i < s->textures.bg_h; ++i) SDL_memcpy((Uint8 *)pixels + i * pitch, (Uint8 *)s->threads.bg_pixel_buffer + i * s->textures.bg_w * 4, s->textures.bg_w * 4);
      SDL_UnlockTexture(s->textures.bg_texture);
    }
    SDL_SetAtomicInt(&s->threads.bg_data_ready, 0);
  }
  if (SDL_GetAtomicInt(&s->threads.bg_request_update) == 0) {
    SDL_LockMutex(s->threads.bg_mutex); s->threads.bg_target_cam_pos = s->camera.pos; s->threads.bg_target_zoom = s->camera.zoom; s->threads.bg_target_time = s->current_time; SDL_UnlockMutex(s->threads.bg_mutex);
    SDL_SetAtomicInt(&s->threads.bg_request_update, 1);
  }
}

void Workers_UpdateDensityMap(AppState *s) {
  if (!s->textures.density_texture) return;
  if (SDL_GetAtomicInt(&s->threads.density_data_ready) == 1) {
    void *pixels; int pitch;
    if (SDL_LockTexture(s->textures.density_texture, NULL, &pixels, &pitch)) {
      for (int i = 0; i < s->threads.density_h; ++i) SDL_memcpy((Uint8 *)pixels + i * pitch, (Uint8 *)s->threads.density_pixel_buffer + i * s->threads.density_w * 4, s->threads.density_w * 4);
      SDL_UnlockTexture(s->textures.density_texture);
      SDL_LockMutex(s->threads.density_mutex); s->threads.density_texture_cam_pos = s->threads.density_target_cam_pos; SDL_UnlockMutex(s->threads.density_mutex);
    }
    SDL_SetAtomicInt(&s->threads.density_data_ready, 0);
  }
  if (SDL_GetAtomicInt(&s->threads.density_request_update) == 0) {
    int ww, wh; SDL_GetRenderOutputSize(s->renderer, &ww, &wh);
    float cx = s->camera.pos.x + (ww / 2.0f) / s->camera.zoom;
    float cy = s->camera.pos.y + (wh / 2.0f) / s->camera.zoom;
    float cell_sz = (float)DENSITY_CELL_SIZE / (float)GRID_DENSITY_SUB_RES;
    SDL_LockMutex(s->threads.density_mutex); s->threads.density_target_cam_pos.x = floorf(cx / cell_sz) * cell_sz; s->threads.density_target_cam_pos.y = floorf(cy / cell_sz) * cell_sz; SDL_UnlockMutex(s->threads.density_mutex);
    SDL_SetAtomicInt(&s->threads.density_request_update, 1);
  }
}
