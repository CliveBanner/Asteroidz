#include "constants.h"
#include "game.h"
#include <math.h>
#include <stdio.h>

// Reduced resolution for the noise background to keep FPS playable
#define BG_SCALE_FACTOR 16

typedef struct {
  float pos;
  float r, g, b;
} ColorStop;

static void GetNebulaColor(float t, float *r, float *g, float *b) {
  // Brighter Dark Nebula Gradient
  static const ColorStop stops[] = {
      {0.00f, 2, 2, 6},    // Absolute Void
      {0.40f, 10, 10, 25}, // Deep Space Blue
      {0.70f, 25, 15, 45}, // Atmospheric Violet
      {0.90f, 35, 60, 80}, // Dusty Cyan
      {1.00f, 50, 80, 100} // Brightest Cloud Edge
  };
  static const int num_stops = 5;

  if (t <= stops[0].pos) {
    *r = stops[0].r;
    *g = stops[0].g;
    *b = stops[0].b;
    return;
  }
  if (t >= stops[num_stops - 1].pos) {
    *r = stops[num_stops - 1].r;
    *g = stops[num_stops - 1].g;
    *b = stops[num_stops - 1].b;
    return;
  }

  for (int i = 0; i < num_stops - 1; i++) {
    if (t >= stops[i].pos && t <= stops[i + 1].pos) {
      float range = stops[i + 1].pos - stops[i].pos;
      float local_t = (t - stops[i].pos) / range;
      float s_t = local_t * local_t * (3.0f - 2.0f * local_t);
      *r = stops[i].r * (1.0f - s_t) + stops[i + 1].r * s_t;
      *g = stops[i].g * (1.0f - s_t) + stops[i + 1].g * s_t;
      *b = stops[i].b * (1.0f - s_t) + stops[i + 1].b * s_t;
      return;
    }
  }
}

static int SDLCALL BackgroundGenerationThread(void *data) {
  AppState *s = (AppState *)data;

  while (SDL_GetAtomicInt(&s->bg_should_quit) == 0) {
    if (SDL_GetAtomicInt(&s->bg_request_update) == 1) {

      SDL_LockMutex(s->bg_mutex);
      Vec2 cam_pos = s->bg_target_cam_pos;
      float zoom = s->bg_target_zoom;
      float time = s->bg_target_time;
      SDL_UnlockMutex(s->bg_mutex);

      float s_macro = 0.0002f;
      float s_low = 0.001f;
      float s_med = 0.003f;
      float color_scale = 0.00001f;

      float w_macro = 0.6f;
      float w_low = 0.3f;
      float w_med = 0.1f;

      // Even slower drift for almost stationary cosmic clouds
      float drift_macro_x = time * 0.05f;
      float drift_macro_y = time * 0.03f;
      float drift_detail_x = time * 0.15f;
      float drift_detail_y = time * 0.1f;

      for (int i = 0; i < s->bg_w * s->bg_h; i++) {
        int x = i % s->bg_w;
        int y = i / s->bg_w;

        float world_x = cam_pos.x + (x * BG_SCALE_FACTOR) / zoom;
        float world_y = cam_pos.y + (y * BG_SCALE_FACTOR) / zoom;

        // Apply different drift to different layers for a morphing effect
        float n = Noise2D(world_x * s_macro + drift_macro_x, world_y * s_macro + drift_macro_y) * w_macro +
                  Noise2D(world_x * s_low   + drift_detail_x, world_y * s_low   + drift_detail_y) * w_low +
                  Noise2D(world_x * s_med   + drift_detail_x, world_y * s_med   + drift_detail_y) * w_med;

        float variation = Noise2D(world_x * color_scale + 12345.0f,
                                  world_y * color_scale + 67890.0f);

        if (n > 1.0f) n = 1.0f; if (n < 0.0f) n = 0.0f;
        if (variation > 1.0f) variation = 1.0f; if (variation < 0.0f) variation = 0.0f;

        float r_f, g_f, b_f;
        GetNebulaColor(n, &r_f, &g_f, &b_f);

        r_f *= (0.95f + variation * 0.1f);
        g_f *= (0.95f + (1.0f - variation) * 0.1f);

        float intensity = 0.5f + n * 0.5f;
        Uint8 r = (Uint8)fminf(r_f * intensity, 255.0f);
        Uint8 g = (Uint8)fminf(g_f * intensity, 255.0f);
        Uint8 b = (Uint8)fminf(b_f * intensity, 255.0f);

        s->bg_pixel_buffer[i] = (255 << 24) | (b << 16) | (g << 8) | r;
      }

      // Blur
      for (int y = 0; y < s->bg_h; y++) {
        for (int x = 0; x < s->bg_w; x++) {
          int i = y * s->bg_w + x;
          int r_sum = 0, g_sum = 0, b_sum = 0;
          int count = 0;
#define ADD_PIXEL(idx) \
  do { \
    Uint32 p = s->bg_pixel_buffer[idx]; \
    r_sum += (p & 0xFF); \
    g_sum += ((p >> 8) & 0xFF); \
    b_sum += ((p >> 16) & 0xFF); \
    count++; \
  } while (0)
          ADD_PIXEL(i);
          if (x > 0) ADD_PIXEL(i - 1);
          if (x < s->bg_w - 1) ADD_PIXEL(i + 1);
          if (y > 0) ADD_PIXEL(i - s->bg_w);
          if (y < s->bg_h - 1) ADD_PIXEL(i + s->bg_w);
#undef ADD_PIXEL
          Uint8 r_val = r_sum / count;
          Uint8 g_val = g_sum / count;
          Uint8 b_val = b_sum / count;
          s->bg_pixel_buffer[i] = (255 << 24) | (b_val << 16) | (g_val << 8) | r_val;
        }
      }

      SDL_SetAtomicInt(&s->bg_request_update, 0);
      SDL_SetAtomicInt(&s->bg_data_ready, 1);
    } else {
      SDL_Delay(10);
    }
  }
  return 0;
}

void Renderer_Init(AppState *s) {
  int w, h;
  SDL_GetRenderOutputSize(s->renderer, &w, &h);
  s->bg_w = w / BG_SCALE_FACTOR;
  s->bg_h = h / BG_SCALE_FACTOR;
  s->bg_pixel_buffer = SDL_calloc(s->bg_w * s->bg_h, sizeof(Uint32));
  s->bg_texture = SDL_CreateTexture(s->renderer, SDL_PIXELFORMAT_ABGR8888,
                                    SDL_TEXTUREACCESS_STREAMING, s->bg_w, s->bg_h);
  if (s->bg_texture) {
    SDL_SetTextureScaleMode(s->bg_texture, SDL_SCALEMODE_LINEAR);
  }
  s->bg_mutex = SDL_CreateMutex();
  SDL_SetAtomicInt(&s->bg_should_quit, 0);
  SDL_SetAtomicInt(&s->bg_request_update, 0);
  SDL_SetAtomicInt(&s->bg_data_ready, 0);
  
  s->bg_target_cam_pos = s->camera_pos;
  s->bg_target_zoom = s->zoom;
  s->bg_target_time = s->current_time;
  SDL_SetAtomicInt(&s->bg_request_update, 1);

  s->bg_thread = SDL_CreateThread(BackgroundGenerationThread, "BG_Gen", s);
}

static void UpdateBackground(AppState *s) {
  if (!s->bg_texture) return;
  if (SDL_GetAtomicInt(&s->bg_data_ready) == 1) {
    void *pixels;
    int pitch;
    if (SDL_LockTexture(s->bg_texture, NULL, &pixels, &pitch)) {
      Uint8 *dst = (Uint8 *)pixels;
      Uint8 *src = (Uint8 *)s->bg_pixel_buffer;
      int src_pitch = s->bg_w * 4;
      for (int i = 0; i < s->bg_h; ++i) {
        SDL_memcpy(dst + i * pitch, src + i * src_pitch, src_pitch);
      }
      SDL_UnlockTexture(s->bg_texture);
    }
    SDL_SetAtomicInt(&s->bg_data_ready, 0);
  }
  if (SDL_GetAtomicInt(&s->bg_request_update) == 0) {
    SDL_LockMutex(s->bg_mutex);
    s->bg_target_cam_pos = s->camera_pos;
    s->bg_target_zoom = s->zoom;
    s->bg_target_time = s->current_time;
    SDL_UnlockMutex(s->bg_mutex);
    SDL_SetAtomicInt(&s->bg_request_update, 1);
  }
}

static void DrawGrid(SDL_Renderer *renderer, const AppState *s, int win_w, int win_h) {
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 60);
  int grid_size = 200;
  int start_x = (int)floorf(s->camera_pos.x / grid_size) * grid_size;
  int start_y = (int)floorf(s->camera_pos.y / grid_size) * grid_size;
  for (float x = start_x; x < s->camera_pos.x + win_w / s->zoom + grid_size; x += grid_size) {
    float screen_x = (x - s->camera_pos.x) * s->zoom;
    SDL_RenderLine(renderer, screen_x, 0, screen_x, (float)win_h);
  }
  for (float y = start_y; y < s->camera_pos.y + win_h / s->zoom + grid_size; y += grid_size) {
    float screen_y = (y - s->camera_pos.y) * s->zoom;
    SDL_RenderLine(renderer, 0, screen_y, (float)win_w, screen_y);
  }
}

static void DrawDebugInfo(SDL_Renderer *renderer, const AppState *s, int win_w) {
  char coords_text[64];
  snprintf(coords_text, sizeof(coords_text), "Cam: %.1f, %.1f (x%.4f)", s->camera_pos.x, s->camera_pos.y, s->zoom);
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
  float text_x = win_w - (SDL_strlen(coords_text) * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE) - 20;
  float text_y = 20;
  SDL_RenderDebugText(renderer, text_x, text_y, coords_text);
  char fps_text[32];
  snprintf(fps_text, sizeof(fps_text), "FPS: %.0f", s->current_fps);
  SDL_RenderDebugText(renderer, 20, 20, fps_text);
}

void Renderer_Draw(AppState *s) {
  int win_w, win_h;
  SDL_GetRenderOutputSize(s->renderer, &win_w, &win_h);
  SDL_SetRenderDrawColor(s->renderer, 0, 0, 0, 255);
  SDL_RenderClear(s->renderer);
  UpdateBackground(s);
  if (s->bg_texture) {
    SDL_RenderTexture(s->renderer, s->bg_texture, NULL, NULL);
  }
  if (s->show_grid) {
    DrawGrid(s->renderer, s, win_w, win_h);
  }
  DrawDebugInfo(s->renderer, s, win_w);
  SDL_RenderPresent(s->renderer);
}