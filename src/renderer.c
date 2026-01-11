#include "constants.h"
#include "game.h"
#include <math.h>
#include <stdio.h>

// Faster resolution for planets
#define BG_SCALE_FACTOR 16

typedef struct { float pos; float r, g, b; } ColorStop;

static void GetNebulaColor(float t, float *r, float *g, float *b) {
  static const ColorStop stops[] = { {0.00f, 2, 2, 6}, {0.40f, 10, 10, 25}, {0.70f, 25, 15, 45}, {0.90f, 35, 60, 80}, {1.00f, 50, 80, 100} };
  if (t <= stops[0].pos) { *r = stops[0].r; *g = stops[0].g; *b = stops[0].b; return; }
  for (int i = 0; i < 4; i++) {
    if (t >= stops[i].pos && t <= stops[i+1].pos) {
      float s_t = (t - stops[i].pos) / (stops[i+1].pos - stops[i].pos);
      float st = s_t * s_t * (3.0f - 2.0f * s_t);
      *r = stops[i].r * (1-st) + stops[i+1].r * st; *g = stops[i].g * (1-st) + stops[i+1].g * st; *b = stops[i].b * (1-st) + stops[i+1].b * st;
      return;
    }
  }
}

static void DrawPlanetToBuffer(Uint32 *pixels, int size, float seed) {
    int center = size / 2;
    float radius = (size / 2.0f) * 0.4f; 
    float atmo_outer = radius * 2.5f;
    float theme = DeterministicHash((int)(seed * 1000), 42);
    float rm, gm, bm;
    if (theme > 0.8f) { rm = 0.8f; gm = 0.4f; bm = 0.3f; }
    else if (theme > 0.6f) { rm = 0.4f; gm = 0.5f; bm = 0.7f; }
    else if (theme > 0.4f) { rm = 0.2f; gm = 0.6f; bm = 0.4f; }
    else { rm = 0.5f; gm = 0.5f; bm = 0.6f; }

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float dx = (float)(x - center), dy = (float)(y - center);
            float dist = sqrtf(dx*dx + dy*dy);
            if (dist <= atmo_outer) {
                float nx = dx / radius, ny = dy / radius;
                float nz = (dist <= radius) ? sqrtf(fmaxf(0.0f, 1.0f - nx*nx - ny*ny)) : 0.0f;
                
                // Optimized PerlinNoise2D for planet surface
                float noise = 0.0f, amp = 0.7f, freq = 0.03f;
                for (int o = 0; o < 3; o++) {
                    noise += PerlinNoise2D((float)x * freq + seed, (float)y * freq + seed * 2.0f) * amp;
                    amp *= 0.3f; freq *= 2.0f;
                }
                
                float dot = fmaxf(0.05f, nx * -0.6f + ny * -0.6f + nz * 0.5f);
                float shading = powf(dot, 0.8f);
                Uint8 r = (Uint8)fminf(255.0f, (80 + noise * 175) * rm * shading);
                Uint8 g = (Uint8)fminf(255.0f, (80 + noise * 175) * gm * shading);
                Uint8 b = (Uint8)fminf(255.0f, (80 + noise * 175) * bm * shading);
                Uint8 alpha = 255;
                if (dist > radius) {
                    float atmo_t = (dist - radius) / (atmo_outer - radius);
                    float an = PerlinNoise2D((float)x * 0.01f + seed, (float)y * 0.01f);
                    float af = (1.0f - atmo_t) * (0.2f + an * 0.8f);
                    alpha = (Uint8)(af * 80);
                    r = (Uint8)(r * 0.2f + 50 * af); g = (Uint8)(g * 0.2f + 150 * af); b = (Uint8)(b * 0.2f + 255 * af);
                }
                pixels[y * size + x] = (alpha << 24) | (b << 16) | (g << 8) | r;
            }
        }
    }
}

void Renderer_GeneratePlanetStep(AppState *s) {
    if (s->planets_generated >= PLANET_COUNT) { s->is_loading = false; return; }
    int p_size = 512; // Lower res for much faster generation
    Uint32 *pixels = SDL_malloc(p_size * p_size * sizeof(Uint32));
    SDL_memset(pixels, 0, p_size * p_size * sizeof(Uint32));
    DrawPlanetToBuffer(pixels, p_size, (float)s->planets_generated * 567.89f);
    s->planet_textures[s->planets_generated] = SDL_CreateTexture(s->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, p_size, p_size);
    SDL_SetTextureBlendMode(s->planet_textures[s->planets_generated], SDL_BLENDMODE_BLEND);
    SDL_UpdateTexture(s->planet_textures[s->planets_generated], NULL, pixels, p_size * 4);
    SDL_free(pixels);
    s->planets_generated++;
}

void Renderer_DrawLoading(AppState *s) {
    int win_w, win_h; SDL_GetRenderOutputSize(s->renderer, &win_w, &win_h);
    SDL_SetRenderDrawColor(s->renderer, 0, 0, 0, 255); SDL_RenderClear(s->renderer);
    float progress = (float)s->planets_generated / (float)PLANET_COUNT;
    float bar_w = 400.0f, bar_h = 20.0f; float x = (win_w - bar_w) / 2.0f, y = (win_h - bar_h) / 2.0f;
    SDL_SetRenderDrawColor(s->renderer, 50, 50, 50, 255); SDL_RenderFillRect(s->renderer, &(SDL_FRect){ x, y, bar_w, bar_h });
    SDL_SetRenderDrawColor(s->renderer, 100, 200, 255, 255); SDL_RenderFillRect(s->renderer, &(SDL_FRect){ x, y, bar_w * progress, bar_h });
    SDL_RenderDebugText(s->renderer, x, y - 25, "Generating Universe..."); SDL_RenderPresent(s->renderer);
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
      float dmx = time * 0.05f, dmy = time * 0.03f, ddx = time * 0.15f, ddy = time * 0.1f;
      for (int i = 0; i < s->bg_w * s->bg_h; i++) {
        int x = i % s->bg_w, y = i / s->bg_w;
        float wx = cam_pos.x + (x * BG_SCALE_FACTOR) / zoom, wy = cam_pos.y + (y * BG_SCALE_FACTOR) / zoom;
        float n = ValueNoise2D(wx * s_macro + dmx, wy * s_macro + dmy) * w_macro +
                  ValueNoise2D(wx * s_low   + ddx, wy * s_low   + ddy) * w_low +
                  ValueNoise2D(wx * s_med   + ddx, wy * s_med   + ddy) * w_med;
        float variation = ValueNoise2D(wx * color_scale + 12345.0f, wy * color_scale + 67890.0f);
        if (n > 1.0f) n = 1.0f; float r_f, g_f, b_f; GetNebulaColor(n, &r_f, &g_f, &b_f);
        r_f *= (0.95f + variation * 0.1f); g_f *= (0.95f + (1.0f - variation) * 0.1f);
        float intensity = 0.5f + n * 0.5f;
        Uint8 r = (Uint8)fminf(r_f * intensity, 255.0f), g = (Uint8)fminf(g_f * intensity, 255.0f), b = (Uint8)fminf(b_f * intensity, 255.0f);
        float af = (n - 0.1f) / 0.6f; af = fmaxf(0.0f, fminf(1.0f, af)); Uint8 a = (Uint8)(af * 255);
        s->bg_pixel_buffer[i] = (a << 24) | (b << 16) | (g << 8) | r;
      }
      for (int pass = 0; pass < 1; pass++) {
        for (int y = 0; y < s->bg_h; y++) {
          for (int x = 0; x < s->bg_w; x++) {
            int i = y * s->bg_w + x; int rs = 0, gs = 0, bs = 0, as = 0, count = 0;
            #define ADD_PIXEL(idx) do { Uint32 p = s->bg_pixel_buffer[idx]; rs += (p & 0xFF); gs += ((p >> 8) & 0xFF); bs += ((p >> 16) & 0xFF); as += ((p >> 24) & 0xFF); count++; } while(0)
            ADD_PIXEL(i); if (x > 0) ADD_PIXEL(i - 1); if (x < s->bg_w - 1) ADD_PIXEL(i + 1); if (y > 0) ADD_PIXEL(i - s->bg_w); if (y < s->bg_h - 1) ADD_PIXEL(i + s->bg_w);
            #undef ADD_PIXEL
            s->bg_pixel_buffer[i] = ((as/count) << 24) | ((bs/count) << 16) | ((gs/count) << 8) | (rs/count);
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
  s->is_loading = true; s->planets_generated = 0;
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
    float cx = win_w/2.0f, cy = win_h/2.0f;
    float cxp = s->camera_pos.x * parallax + (cx/s->zoom)*(parallax-1.0f), cyp = s->camera_pos.y * parallax + (cy/s->zoom)*(parallax-1.0f);
    int sgx = (int)floorf(cxp/cell_size), sgy = (int)floorf(cyp/cell_size);
    int egx = (int)ceilf((cxp+win_w/s->zoom)/cell_size), egy = (int)ceilf((cyp+win_h/s->zoom)/cell_size);
    for (int gy = sgy; gy <= egy; gy++) {
        for (int gx = sgx; gx <= egx; gx++) {
            float seed = DeterministicHash(gx, gy);
            if (seed > 0.85f) { 
                float ox = DeterministicHash(gx+10, gy+20)*cell_size, oy = DeterministicHash(gx+30, gy+40)*cell_size;
                float dx = sinf(s->current_time*0.2f + seed*10.0f)*15.0f, dy = cosf(s->current_time*0.15f + seed*5.0f)*15.0f;
                float sx = (gx*cell_size + ox + dx - cxp)*s->zoom, sy = (gy*cell_size + oy + dy - cyp)*s->zoom;
                float sz_s = DeterministicHash(gx+55, gy+66), sz = (sz_s > 0.98f) ? 3.0f : (sz_s > 0.90f ? 2.0f : 1.0f);
                float b_s = DeterministicHash(gx+77, gy+88), c_s = DeterministicHash(gx+99, gy+11);
                Uint8 val = (Uint8)(100 + b_s*155), r = val, g = val, b = val;
                if (c_s > 0.90f) { r = (Uint8)(val*0.8f); g = (Uint8)(val*0.9f); b = 255; }
                else if (c_s > 0.80f) { r = 220; g = (Uint8)(val*0.7f); b = 255; }
                SDL_SetRenderDrawColor(renderer, r, g, b, 200);
                float s_sz = sz * s->zoom;
                if (s_sz <= 1.0f) SDL_RenderPoint(renderer, sx, sy);
                else { SDL_FRect rct = { sx-s_sz/2, sy-s_sz/2, s_sz, s_sz }; SDL_RenderFillRect(renderer, &rct); }
            }
        }
    }
}

static void DrawDistantPlanets(SDL_Renderer *renderer, const AppState *s, int win_w, int win_h) {
    int cell_size = 5000; float parallax = 0.7f;
    float cx = win_w/2.0f, cy = win_h/2.0f;
    float cxp = s->camera_pos.x * parallax + (cx/s->zoom)*(parallax-1.0f), cyp = s->camera_pos.y * parallax + (cy/s->zoom)*(parallax-1.0f);
    int sgx = (int)floorf(cxp/cell_size), sgy = (int)floorf(cyp/cell_size);
    int egx = (int)ceilf((cxp+win_w/s->zoom)/cell_size), egy = (int)ceilf((cyp+win_h/s->zoom)/cell_size);
    for (int gy = sgy; gy <= egy; gy++) {
        for (int gx = sgx; gx <= egx; gx++) {
            float seed = DeterministicHash(gx, gy + 1000);
            if (seed > 0.95f || (gx == 0 && gy == 0)) {
                float ox = (gx==0&&gy==0) ? cell_size/2 : DeterministicHash(gx+5, gy+9)*cell_size;
                float oy = (gx==0&&gy==0) ? cell_size/2 : DeterministicHash(gx+12, gy+3)*cell_size;
                float sx = (gx*cell_size + ox - cxp)*s->zoom, sy = (gy*cell_size + oy - cyp)*s->zoom;
                float r_s = DeterministicHash(gx+7, gy+11), rad = (150.0f + r_s*450.0f)*s->zoom; 
                if (sx + rad < 0 || sx - rad > win_w || sy + rad < 0 || sy - rad > win_h) continue;
                int p_idx = (int)(DeterministicHash(gx+1, gy+1) * PLANET_COUNT);
                float t_sz = rad * 2.5f;
                SDL_FRect dst = { sx - t_sz/2, sy - t_sz/2, t_sz, t_sz };
                SDL_RenderTexture(renderer, s->planet_textures[p_idx], NULL, &dst);
            }
        }
    }
}

static void DrawGrid(SDL_Renderer *renderer, const AppState *s, int win_w, int win_h) {
  SDL_SetRenderDrawColor(renderer, 50, 50, 50, 40);
  int gs = 200;
  int stx = (int)floorf(s->camera_pos.x/gs)*gs, sty = (int)floorf(s->camera_pos.y/gs)*gs;
  for (float x = stx; x < s->camera_pos.x + win_w/s->zoom + gs; x += gs) { float sx = (x - s->camera_pos.x)*s->zoom; SDL_RenderLine(renderer, sx, 0, sx, (float)win_h); }
  for (float y = sty; y < s->camera_pos.y + win_h/s->zoom + gs; y += gs) { float sy = (y - s->camera_pos.y)*s->zoom; SDL_RenderLine(renderer, 0, sy, (float)win_w, sy); }
  SDL_SetRenderDrawColor(renderer, 100, 100, 100, 80);
  int gl = 1000;
  int slx = (int)floorf(s->camera_pos.x/gl)*gl, sly = (int)floorf(s->camera_pos.y/gl)*gl;
  for (float x = slx; x < s->camera_pos.x + win_w/s->zoom + gl; x += gl) { float sx = (x - s->camera_pos.x)*s->zoom; SDL_RenderLine(renderer, sx, 0, sx, (float)win_h); }
  for (float y = sly; y < s->camera_pos.y + win_h/s->zoom + gl; y += gl) { float sy = (y - s->camera_pos.y)*s->zoom; SDL_RenderLine(renderer, 0, sy, (float)win_w, sy); }
  SDL_SetRenderDrawColor(renderer, 150, 150, 150, 150);
  for (float x = slx; x < s->camera_pos.x + win_w/s->zoom + gl; x += gl) {
    for (float y = sly; y < s->camera_pos.y + win_h / s->zoom + gl; y += gl) {
        float sx = (x - s->camera_pos.x) * s->zoom, sy = (y - s->camera_pos.y) * s->zoom;
        if (sx >= -10 && sx < win_w && sy >= -10 && sy < win_h) {
            char l[32]; snprintf(l, sizeof(l), "(%.0fk,%.0fk)", x/1000.0f, y/1000.0f);
            SDL_RenderDebugText(renderer, sx + 5, sy + 5, l);
        }
    }
  }
}

static void DrawDebugInfo(SDL_Renderer *renderer, const AppState *s, int win_w) {
  char ct[64]; snprintf(ct, sizeof(ct), "Cam: %.1f, %.1f (x%.4f)", s->camera_pos.x, s->camera_pos.y, s->zoom);
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
  SDL_RenderDebugText(renderer, win_w - (SDL_strlen(ct)*8) - 20, 20, ct);
  char ft[32]; snprintf(ft, sizeof(ft), "FPS: %.0f", s->current_fps);
  SDL_RenderDebugText(renderer, 20, 20, ft);
}

void Renderer_Draw(AppState *s) {
  int ww, wh; SDL_GetRenderOutputSize(s->renderer, &ww, &wh);
  SDL_SetRenderDrawColor(s->renderer, 0, 0, 0, 255); SDL_RenderClear(s->renderer);
  UpdateBackground(s);
  DrawInfiniteStars(s->renderer, s, ww, wh);
  DrawDistantPlanets(s->renderer, s, ww, wh);
  if (s->bg_texture) { SDL_RenderTexture(s->renderer, s->bg_texture, NULL, NULL); }
  if (s->show_grid) { DrawGrid(s->renderer, s, ww, wh); }
  DrawDebugInfo(s->renderer, s, ww);
  SDL_RenderPresent(s->renderer);
}
