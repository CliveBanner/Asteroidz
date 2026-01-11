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
  static const ColorStop stops[] = {
      {0.00f, 2, 2, 6},    {0.40f, 10, 10, 25}, {0.70f, 25, 15, 45},
      {0.90f, 35, 60, 80}, {1.00f, 50, 80, 100}};
  static const int num_stops = 5;
  if (t <= stops[0].pos) { *r = stops[0].r; *g = stops[0].g; *b = stops[0].b; return; }
  if (t >= stops[num_stops - 1].pos) { *r = stops[num_stops - 1].r; *g = stops[num_stops - 1].g; *b = stops[num_stops - 1].b; return; }
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

static void DrawPlanetToBuffer(Uint32 *pixels, int size, float seed) {
    int center = size / 2;
    float radius = (size / 2.0f) - 10.0f;
    float theme = DeterministicHash((int)(seed * 1000), 42);
    float r_m, g_m, b_m;
    if (theme > 0.8f) { r_m = 0.8f; g_m = 0.4f; b_m = 0.3f; }
    else if (theme > 0.6f) { r_m = 0.4f; g_m = 0.5f; b_m = 0.7f; }
    else if (theme > 0.4f) { r_m = 0.2f; g_m = 0.6f; b_m = 0.4f; }
    else { r_m = 0.5f; g_m = 0.5f; b_m = 0.6f; }
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float dx = (float)(x - center), dy = (float)(y - center);
            float dist = sqrtf(dx*dx + dy*dy);
            if (dist <= radius) {
                float nx = dx / radius, ny = dy / radius;
                float nz = sqrtf(fmaxf(0.0f, 1.0f - nx*nx - ny*ny));
                float noise = Noise2D((float)x * 0.03f + seed, (float)y * 0.03f + seed * 2.0f) * 0.7f +
                              Noise2D((float)x * 0.15f + seed, (float)y * 0.15f) * 0.3f;
                float dot = fmaxf(0.05f, nx * -0.6f + ny * -0.6f + nz * 0.5f);
                float shading = powf(dot, 0.8f);
                Uint8 r = (Uint8)fminf(255.0f, (80 + noise * 175) * r_m * shading);
                Uint8 g = (Uint8)fminf(255.0f, (80 + noise * 175) * g_m * shading);
                Uint8 b = (Uint8)fminf(255.0f, (80 + noise * 175) * b_m * shading);
                pixels[y * size + x] = (255 << 24) | (b << 16) | (g << 8) | r;
            }
        }
    }
}

static void GeneratePlanetTextures(AppState *s) {
    int p_size = 512;
    Uint32 *pixels = SDL_malloc(p_size * p_size * sizeof(Uint32));
    for (int i = 0; i < 8; i++) {
        SDL_memset(pixels, 0, p_size * p_size * sizeof(Uint32));
        DrawPlanetToBuffer(pixels, p_size, (float)i * 567.89f);
        s->planet_textures[i] = SDL_CreateTexture(s->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, p_size, p_size);
        SDL_SetTextureBlendMode(s->planet_textures[i], SDL_BLENDMODE_BLEND);
        SDL_UpdateTexture(s->planet_textures[i], NULL, pixels, p_size * 4);
    }
    SDL_free(pixels);
}

static int SDLCALL BackgroundGenerationThread(void *data) {
  AppState *s = (AppState *)data;
  while (SDL_GetAtomicInt(&s->bg_should_quit) == 0) {
    if (SDL_GetAtomicInt(&s->bg_request_update) == 1) {
      SDL_LockMutex(s->bg_mutex);
      Vec2 cam_pos = s->bg_target_cam_pos; float zoom = s->bg_target_zoom; float time = s->bg_target_time;
      SDL_UnlockMutex(s->bg_mutex);
      float s_macro = 0.0002f, s_low = 0.001f, s_med = 0.003f, color_scale = 0.00001f;
      float w_macro = 0.6f, w_low = 0.3f, w_med = 0.1f;
      float drift_macro_x = time * 0.05f, drift_macro_y = time * 0.03f, drift_detail_x = time * 0.15f, drift_detail_y = time * 0.1f;
      for (int i = 0; i < s->bg_w * s->bg_h; i++) {
        int x = i % s->bg_w, y = i / s->bg_w;
        float world_x = cam_pos.x + (x * BG_SCALE_FACTOR) / zoom, world_y = cam_pos.y + (y * BG_SCALE_FACTOR) / zoom;
        float n = Noise2D(world_x * s_macro + drift_macro_x, world_y * s_macro + drift_macro_y) * w_macro +
                  Noise2D(world_x * s_low   + drift_detail_x, world_y * s_low   + drift_detail_y) * w_low +
                  Noise2D(world_x * s_med   + drift_detail_x, world_y * s_med   + drift_detail_y) * w_med;
        float variation = Noise2D(world_x * color_scale + 12345.0f, world_y * color_scale + 67890.0f);
        if (n > 1.0f) n = 1.0f; float r_f, g_f, b_f; GetNebulaColor(n, &r_f, &g_f, &b_f);
        r_f *= (0.95f + variation * 0.1f); g_f *= (0.95f + (1.0f - variation) * 0.1f);
        float intensity = 0.5f + n * 0.5f;
        Uint8 r = (Uint8)fminf(r_f * intensity, 255.0f); 
        Uint8 g = (Uint8)fminf(g_f * intensity, 255.0f); 
        Uint8 b = (Uint8)fminf(b_f * intensity, 255.0f);
        
        // DYNAMIC ALPHA: Increase visibility
        // Start fading in earlier (at 0.1 noise) and reach full opacity faster
        float alpha_f = (n - 0.1f) / 0.6f;
        alpha_f = fmaxf(0.0f, fminf(1.0f, alpha_f));
        Uint8 alpha = (Uint8)(alpha_f * 255);

        s->bg_pixel_buffer[i] = (alpha << 24) | (b << 16) | (g << 8) | r;
      }
      for (int pass = 0; pass < 1; pass++) {
        for (int y = 0; y < s->bg_h; y++) {
          for (int x = 0; x < s->bg_w; x++) {
            int i = y * s->bg_w + x; int r_sum = 0, g_sum = 0, b_sum = 0, a_sum = 0, count = 0;
            #define ADD_PIXEL(idx) do { Uint32 p = s->bg_pixel_buffer[idx]; r_sum += (p & 0xFF); g_sum += ((p >> 8) & 0xFF); b_sum += ((p >> 16) & 0xFF); a_sum += ((p >> 24) & 0xFF); count++; } while(0)
            ADD_PIXEL(i); if (x > 0) ADD_PIXEL(i - 1); if (x < s->bg_w - 1) ADD_PIXEL(i + 1); if (y > 0) ADD_PIXEL(i - s->bg_w); if (y < s->bg_h - 1) ADD_PIXEL(i + s->bg_w);
            #undef ADD_PIXEL
            s->bg_pixel_buffer[i] = ((a_sum/count) << 24) | ((b_sum/count) << 16) | ((g_sum/count) << 8) | (r_sum/count);
          }
        }
      }
      SDL_SetAtomicInt(&s->bg_request_update, 0); SDL_SetAtomicInt(&s->bg_data_ready, 1);
    } else { SDL_Delay(10); }
  }
  return 0;
}

void Renderer_Init(AppState *s) {
  int w, h; SDL_GetRenderOutputSize(s->renderer, &w, &h);
  s->bg_w = w / BG_SCALE_FACTOR; s->bg_h = h / BG_SCALE_FACTOR;
  s->bg_pixel_buffer = SDL_calloc(s->bg_w * s->bg_h, sizeof(Uint32));
  s->bg_texture = SDL_CreateTexture(s->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, s->bg_w, s->bg_h);
  if (s->bg_texture) { SDL_SetTextureScaleMode(s->bg_texture, SDL_SCALEMODE_LINEAR); SDL_SetTextureBlendMode(s->bg_texture, SDL_BLENDMODE_BLEND); }
  SDL_SetRenderDrawBlendMode(s->renderer, SDL_BLENDMODE_BLEND);
  s->bg_mutex = SDL_CreateMutex();
  SDL_SetAtomicInt(&s->bg_should_quit, 0); SDL_SetAtomicInt(&s->bg_request_update, 0); SDL_SetAtomicInt(&s->bg_data_ready, 0);
  s->bg_target_cam_pos = s->camera_pos; s->bg_target_zoom = s->zoom; s->bg_target_time = s->current_time;
  GeneratePlanetTextures(s);
  SDL_SetAtomicInt(&s->bg_request_update, 1);
  s->bg_thread = SDL_CreateThread(BackgroundGenerationThread, "BG_Gen", s);
}

static void UpdateBackground(AppState *s) {
  if (!s->bg_texture) return;
  if (SDL_GetAtomicInt(&s->bg_data_ready) == 1) {
    void *pixels; int pitch;
    if (SDL_LockTexture(s->bg_texture, NULL, &pixels, &pitch)) {
      Uint8 *dst = (Uint8 *)pixels; Uint8 *src = (Uint8 *)s->bg_pixel_buffer;
      for (int i = 0; i < s->bg_h; ++i) SDL_memcpy(dst + i * pitch, src + i * s->bg_w * 4, s->bg_w * 4);
      SDL_UnlockTexture(s->bg_texture);
    }
    SDL_SetAtomicInt(&s->bg_data_ready, 0);
  }
  if (SDL_GetAtomicInt(&s->bg_request_update) == 0) {
    SDL_LockMutex(s->bg_mutex);
    s->bg_target_cam_pos = s->camera_pos; s->bg_target_zoom = s->zoom; s->bg_target_time = s->current_time;
    SDL_UnlockMutex(s->bg_mutex);
    SDL_SetAtomicInt(&s->bg_request_update, 1);
  }
}

static void DrawInfiniteStars(SDL_Renderer *renderer, const AppState *s, int win_w, int win_h) {
    int cell_size = 128; float parallax = 0.4f; 
    float center_x = win_w / 2.0f, center_y = win_h / 2.0f;
    float cam_x_p = s->camera_pos.x * parallax + (center_x / s->zoom) * (parallax - 1.0f);
    float cam_y_p = s->camera_pos.y * parallax + (center_y / s->zoom) * (parallax - 1.0f);
    int start_gx = (int)floorf(cam_x_p / cell_size), start_gy = (int)floorf(cam_y_p / cell_size);
    int end_gx = (int)ceilf((cam_x_p + win_w / s->zoom) / cell_size), end_gy = (int)ceilf((cam_y_p + win_h / s->zoom) / cell_size);
    for (int gy = start_gy; gy <= end_gy; gy++) {
        for (int gx = start_gx; gx <= end_gx; gx++) {
            float seed = DeterministicHash(gx, gy);
            if (seed > 0.85f) { 
                float off_x = DeterministicHash(gx + 10, gy + 20) * cell_size, off_y = DeterministicHash(gx + 30, gy + 40) * cell_size;
                float drift_x = sinf(s->current_time * 0.2f + seed * 10.0f) * 15.0f, drift_y = cosf(s->current_time * 0.15f + seed * 5.0f) * 15.0f;
                float world_x = gx * cell_size + off_x + drift_x, world_y = gy * cell_size + off_y + drift_y;
                float screen_x = (world_x - cam_x_p) * s->zoom, screen_y = (world_y - cam_y_p) * s->zoom;
                float size_seed = DeterministicHash(gx + 55, gy + 66), size = 1.0f;
                if (size_seed > 0.98f) size = 3.0f; else if (size_seed > 0.90f) size = 2.0f;
                float bright_seed = DeterministicHash(gx + 77, gy + 88);
                Uint8 color_val = (Uint8)(100 + bright_seed * 155), color_seed = DeterministicHash(gx + 99, gy + 11);
                Uint8 r = color_val, g = color_val, b = color_val;
                if (color_seed > 0.90f) { r = (Uint8)(color_val * 0.8f); g = (Uint8)(color_val * 0.9f); b = 255; }
                else if (color_seed > 0.80f) { r = 220; g = (Uint8)(color_val * 0.7f); b = 255; }
                SDL_SetRenderDrawColor(renderer, r, g, b, 200);
                float s_size = size * s->zoom;
                if (s_size <= 1.0f) { SDL_RenderPoint(renderer, screen_x, screen_y); }
                else { SDL_FRect star_rect = { screen_x - s_size/2.0f, screen_y - s_size/2.0f, s_size, s_size }; SDL_RenderFillRect(renderer, &star_rect); }
            }
        }
    }
}

static void DrawDistantPlanets(SDL_Renderer *renderer, const AppState *s, int win_w, int win_h) {
    int cell_size = 5000; float parallax = 0.7f;
    float center_x = win_w / 2.0f, center_y = win_h / 2.0f;
    float cam_x_p = s->camera_pos.x * parallax + (center_x / s->zoom) * (parallax - 1.0f);
    float cam_y_p = s->camera_pos.y * parallax + (center_y / s->zoom) * (parallax - 1.0f);
    int start_gx = (int)floorf(cam_x_p / cell_size), start_gy = (int)floorf(cam_y_p / cell_size);
    int end_gx = (int)ceilf((cam_x_p + win_w / s->zoom) / cell_size), end_gy = (int)ceilf((cam_y_p + win_h / s->zoom) / cell_size);
    for (int gy = start_gy; gy <= end_gy; gy++) {
        for (int gx = start_gx; gx <= end_gx; gx++) {
            float seed = DeterministicHash(gx, gy + 1000);
            bool is_origin = (gx == 0 && gy == 0);
            if (seed > 0.95f || is_origin) {
                float off_x = is_origin ? cell_size/2 : DeterministicHash(gx + 5, gy + 9) * cell_size;
                float off_y = is_origin ? cell_size/2 : DeterministicHash(gx + 12, gy + 3) * cell_size;
                float world_x = gx * cell_size + off_x, world_y = gy * cell_size + off_y;
                float screen_x = (world_x - cam_x_p) * s->zoom, screen_y = (world_y - cam_y_p) * s->zoom;
                float radius = (200.0f + DeterministicHash(gx, gy) * 300.0f) * s->zoom;
                if (screen_x + radius < 0 || screen_x - radius > win_w || screen_y + radius < 0 || screen_y - radius > win_h) continue;
                int planet_idx = (int)(DeterministicHash(gx + 1, gy + 1) * 8);
                SDL_FRect dest = { screen_x - radius, screen_y - radius, radius * 2, radius * 2 };
                SDL_RenderTexture(renderer, s->planet_textures[planet_idx], NULL, &dest);
            }
        }
    }
}

static void DrawGrid(SDL_Renderer *renderer, const AppState *s, int win_w, int win_h) {
  SDL_SetRenderDrawColor(renderer, 50, 50, 50, 40);
  int grid_small = 200;
  int start_x_s = (int)floorf(s->camera_pos.x / grid_small) * grid_small;
  int start_y_s = (int)floorf(s->camera_pos.y / grid_small) * grid_small;
  for (float x = start_x_s; x < s->camera_pos.x + win_w / s->zoom + grid_small; x += grid_small) {
    float sx = (x - s->camera_pos.x) * s->zoom; SDL_RenderLine(renderer, sx, 0, sx, (float)win_h);
  }
  for (float y = start_y_s; y < s->camera_pos.y + win_h / s->zoom + grid_small; y += grid_small) {
    float sy = (y - s->camera_pos.y) * s->zoom; SDL_RenderLine(renderer, 0, sy, (float)win_w, sy);
  }
  SDL_SetRenderDrawColor(renderer, 100, 100, 100, 80);
  int grid_large = 1000;
  int start_x_l = (int)floorf(s->camera_pos.x / grid_large) * grid_large;
  int start_y_l = (int)floorf(s->camera_pos.y / grid_large) * grid_large;
  for (float x = start_x_l; x < s->camera_pos.x + win_w / s->zoom + grid_large; x += grid_large) {
    float sx = (x - s->camera_pos.x) * s->zoom; SDL_RenderLine(renderer, sx, 0, sx, (float)win_h);
  }
  for (float y = start_y_l; y < s->camera_pos.y + win_h / s->zoom + grid_large; y += grid_large) {
    float sy = (y - s->camera_pos.y) * s->zoom; SDL_RenderLine(renderer, 0, sy, (float)win_w, sy);
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

  // 1. STARS & PLANETS (Now deep background)
  DrawInfiniteStars(s->renderer, s, win_w, win_h);
  DrawDistantPlanets(s->renderer, s, win_w, win_h);

  // 2. NEBULA (On top, but with per-pixel alpha holes!)
  if (s->bg_texture) { 
    SDL_RenderTexture(s->renderer, s->bg_texture, NULL, NULL); 
  }
  
  if (s->show_grid) { DrawGrid(s->renderer, s, win_w, win_h); }
  DrawDebugInfo(s->renderer, s, win_w);
  SDL_RenderPresent(s->renderer);
}
