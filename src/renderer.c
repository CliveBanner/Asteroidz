#include "constants.h"
#include "game.h"
#include <math.h>
#include <stdio.h>

#define BG_SCALE_FACTOR 16

typedef struct { float pos; float r, g, b; } ColorStop;

typedef struct {
    int gx, gy;
    float seed;
    float screen_x, screen_y;
} LayerCell;

typedef void (*LayerDrawFn)(SDL_Renderer *r, const AppState *s, const LayerCell *cell);

static Vec2 GetParallaxCam(const AppState *s, float parallax, int win_w, int win_h) {
    float cx = win_w / 2.0f, cy = win_h / 2.0f;
    return (Vec2){
        s->camera_pos.x * parallax + (cx / s->zoom) * (parallax - 1.0f),
        s->camera_pos.y * parallax + (cy / s->zoom) * (parallax - 1.0f)
    };
}

static Vec2 WorldToScreen(Vec2 world_pos, Vec2 parallax_cam, float zoom) {
    return (Vec2){
        (world_pos.x - parallax_cam.x) * zoom,
        (world_pos.y - parallax_cam.y) * zoom
    };
}

static bool IsVisible(float sx, float sy, float radius, int win_w, int win_h) {
    return (sx + radius >= 0 && sx - radius <= win_w && 
            sy + radius >= 0 && sy - radius <= win_h);
}

static void DrawParallaxLayer(SDL_Renderer *r, const AppState *s, int win_w, int win_h, int cell_size, float parallax, float seed_offset, LayerDrawFn draw_fn) {
    Vec2 p_cam = GetParallaxCam(s, parallax, win_w, win_h);
    int sgx = (int)floorf(p_cam.x / cell_size), sgy = (int)floorf(p_cam.y / cell_size);
    int egx = (int)ceilf((p_cam.x + win_w / s->zoom) / cell_size);
    int egy = (int)ceilf((p_cam.y + win_h / s->zoom) / cell_size);
    for (int gy = sgy; gy <= egy; gy++) {
        for (int gx = sgx; gx <= egx; gx++) {
            LayerCell cell;
            cell.gx = gx; cell.gy = gy;
            cell.seed = DeterministicHash(gx, gy + (int)seed_offset);
            Vec2 screen_pos = WorldToScreen((Vec2){(float)gx * cell_size, (float)gy * cell_size}, p_cam, s->zoom);
            cell.screen_x = screen_pos.x; cell.screen_y = screen_pos.y;
            draw_fn(r, s, &cell);
        }
    }
}

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
    // Scale planet radius down so atmosphere fits in the 512x512 buffer
    float radius = (size / 2.0f) * 0.18f; 
    float atmo_outer = (size / 2.0f) * 0.98f; 
    float theme = DeterministicHash((int)(seed * 1000), 42);
    float rm, gm, bm;
    if (theme > 0.75f) { rm = 0.9f; gm = 0.2f; bm = 0.2f; }
    else if (theme > 0.50f) { rm = 0.2f; gm = 0.8f; bm = 0.3f; }
    else if (theme > 0.25f) { rm = 0.6f; gm = 0.3f; bm = 0.9f; }
    else { rm = 0.3f; gm = 0.5f; bm = 0.8f; }

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float dx = (float)(x - center), dy = (float)(y - center);
            float dist = sqrtf(dx*dx + dy*dy);
            if (dist <= atmo_outer) {
                float nx = dx / radius, ny = dy / radius;
                float nz = (dist <= radius) ? sqrtf(fmaxf(0.0f, 1.0f - nx*nx - ny*ny)) : 0.0f;
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
                    float af = powf(1.0f - atmo_t, 1.5f) * (0.3f + an * 0.7f);
                    alpha = (Uint8)(af * 120); 
                    r = (Uint8)(r * 0.2f + 255 * rm * af); g = (Uint8)(g * 0.2f + 255 * gm * af); b = (Uint8)(b * 0.2f + 255 * bm * af);
                }
                pixels[y * size + x] = (alpha << 24) | (b << 16) | (g << 8) | r;
            }
        }
    }
}

static void DrawGalaxyToBuffer(Uint32 *pixels, int size, float seed) {
    int center = size / 2;
    int s_int = (int)(seed * 1000000.0f);
    
    float twist = 5.0f + DeterministicHash(s_int, 7) * 15.0f;
    int arms = 2 + (int)(DeterministicHash(s_int, 13) * 10);
    float arm_thickness = 1.0f + DeterministicHash(s_int, 88) * 3.0f;
    float core_rad = size * (0.002f + DeterministicHash(s_int, 19) * 0.01f);
    
    float theme = DeterministicHash(s_int, 42);
    float tr, tg, tb, tr2, tg2, tb2;

    if (theme > 0.90f) { // Rare: Gold/Red
        tr = 1.0f; tg = 0.8f; tb = 0.2f; tr2 = 1.0f; tg2 = 0.1f; tb2 = 0.1f;
    } else if (theme > 0.80f) { // Rare: Green/Yellow
        tr = 0.2f; tg = 1.0f; tb = 0.3f; tr2 = 1.0f; tg2 = 1.0f; tb2 = 0.2f;
    } else if (theme > 0.50f) { // Common: Deep Blue/Cyan
        tr = 0.1f; tg = 0.2f; tb = 1.0f; tr2 = 0.2f; tg2 = 0.9f; tb2 = 1.0f;
    } else if (theme > 0.25f) { // Common: Violet/Magenta
        tr = 0.5f; tg = 0.1f; tb = 1.0f; tr2 = 1.0f; tg2 = 0.2f; tb2 = 0.8f;
    } else { // Common: Teal/Blue
        tr = 0.1f; tg = 0.8f; tb = 0.8f; tr2 = 0.1f; tg2 = 0.3f; tb2 = 1.0f;
    }

    float cloud_scale = 0.5f + DeterministicHash(s_int, 55) * 2.0f;

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float dx = (float)(x - center), dy = (float)(y - center);
            float dist = sqrtf(dx*dx + dy*dy);
            float max_dist = size * 0.45f;
            if (dist > max_dist) continue;
            float angle = atan2f(dy, dx);
            float norm_dist = dist / max_dist;
            float twisted_angle = angle - norm_dist * twist;
            float arm_sin = powf(fabsf(cosf(twisted_angle * (float)arms / 2.0f)), arm_thickness);
            float n = PerlinNoise2D(x * 0.05f + seed, y * 0.05f);
            float cloud_n = PerlinNoise2D(x * 0.01f + seed + 200, y * 0.01f + 300);
            float cloud = powf(cloud_n, 1.1f) * cloud_scale; 
            float intensity = (arm_sin * 0.5f + 0.1f) * n + cloud;
            float falloff = 1.0f - powf(norm_dist, 0.3f);
            float core = expf(-dist / core_rad) * 4.0f;
            float final_val = fminf(1.0f, (intensity * falloff + core) * 1.5f);
            
            if (final_val > 0.02f) {
                float cn = PerlinNoise2D(x * 0.025f + seed + 500, y * 0.025f);
                float contrast_boost = 1.0f + norm_dist * 4.0f; 
                float mix_val = fmaxf(0.0f, fminf(1.0f, (cn - 0.5f) * contrast_boost + 0.5f));
                float cr = tr * (1.0f - mix_val) + tr2 * mix_val;
                float cg = tg * (1.0f - mix_val) + tg2 * mix_val;
                float cb = tb * (1.0f - mix_val) + tb2 * mix_val;
                float saturation = fminf(1.0f, norm_dist * 8.0f);
                cr = cr * saturation + (1.0f - saturation);
                cg = cg * saturation + (1.0f - saturation);
                cb = cb * saturation + (1.0f - saturation);
                Uint8 rv = (Uint8)fminf(255, final_val * (cr * 150 + core * 255));
                Uint8 gv = (Uint8)fminf(255, final_val * (cg * 150 + core * 255));
                Uint8 bv = (Uint8)fminf(255, final_val * (cb * 150 + core * 255));
                Uint8 av = (Uint8)fminf(255, final_val * 255 * fminf(1.0f, falloff * 5.0f));
                pixels[y * size + x] = (av << 24) | (bv << 16) | (gv << 8) | rv;
            }
        }
    }
}

static void DrawAsteroidToBuffer(Uint32 *pixels, int size, float seed) {
    int center = size / 2;
    float base_radius = size * 0.35f;
    
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float dx = (float)(x - center), dy = (float)(y - center);
            float dist = sqrtf(dx*dx + dy*dy);
            if (dist < 1.0f) continue;
            float angle = atan2f(dy, dx);
            
            // Multi-octave distortion for irregular rocky shapes
            float shape_n = 0.0f;
            float s_freq = 1.2f, s_amp = 0.6f;
            for(int o=0; o<3; o++) {
                shape_n += (PerlinNoise2D(cosf(angle) * s_freq + seed, sinf(angle) * s_freq + seed) - 0.5f) * s_amp;
                s_freq *= 2.0f; s_amp *= 0.5f;
            }
            float distorted_radius = base_radius * (1.0f + shape_n);
            
            if (dist <= distorted_radius) {
                // Layer 1: Large warped cracks
                float warp = ValueNoise2D(x * 0.02f + seed, y * 0.02f + seed) * 0.5f;
                float cracks1 = VoronoiCracks2D(x * 0.03f + seed + warp, y * 0.03f + seed + warp);
                
                // Layer 2: Smaller surface cracks
                float cracks2 = VoronoiCracks2D(x * 0.1f + seed * 2.0f, y * 0.1f + seed * 3.0f);
                
                float surface_v = ValueNoise2D(x * 0.05f + seed, y * 0.05f + seed);
                
                // Softened material mixing
                float mat_n = ValueNoise2D(x * 0.04f + seed * 4.0f, y * 0.04f + seed * 4.0f);
                float mix_val = fmaxf(0.0f, fminf(1.0f, (mat_n - 0.3f) * 1.5f)); // Smoother transition

                // Determine material colors (more desaturated, natural tones)
                float a_theme = DeterministicHash((int)(seed * 789.0f), 123);
                float r1, g1, b1, r2, g2, b2;
                if (a_theme > 0.7f) { // Iron-tinted
                    r1 = 0.5f; g1 = 0.45f; b1 = 0.42f; r2 = 0.75f; g2 = 0.4f; b2 = 0.35f;
                } else if (a_theme > 0.4f) { // Silicate-tinted
                    r1 = 0.55f; g1 = 0.55f; b1 = 0.6f; r2 = 0.45f; g2 = 0.5f; b2 = 0.75f;
                } else { // Carbonaceous-tinted
                    r1 = 0.35f; g1 = 0.35f; b1 = 0.38f; r2 = 0.55f; g2 = 0.55f; b2 = 0.58f;
                }

                // Interpolate materials with less vibrance
                float tr = r1 * (1.0f - mix_val) + r2 * mix_val;
                float tg = g1 * (1.0f - mix_val) + g2 * mix_val;
                float tb = b1 * (1.0f - mix_val) + b2 * mix_val;

                // Base brightness variation (slightly darker)
                float base_val = 30.0f + surface_v * 35.0f + shape_n * 15.0f;
                
                // Crack darkening
                float c1_factor = fmaxf(0.0f, fminf(1.0f, cracks1 / 0.4f));
                float soft_darken1 = 0.4f + 0.6f * powf(c1_factor, 0.5f);
                float c2_factor = fmaxf(0.0f, fminf(1.0f, cracks2 / 0.2f));
                float soft_darken2 = 0.7f + 0.3f * powf(c2_factor, 0.5f);
                
                Uint8 r = (Uint8)fminf(255, base_val * tr * soft_darken1 * soft_darken2);
                Uint8 g = (Uint8)fminf(255, base_val * tg * soft_darken1 * soft_darken2);
                Uint8 b = (Uint8)fminf(255, base_val * tb * soft_darken1 * soft_darken2);

                pixels[y * size + x] = (255 << 24) | (b << 16) | (g << 8) | r;
            }
        }
    }
}

void Renderer_GeneratePlanetStep(AppState *s) {
    int total_assets = PLANET_COUNT + GALAXY_COUNT + ASTEROID_TYPE_COUNT;
    if (s->assets_generated >= total_assets) { s->is_loading = false; return; }

    if (s->assets_generated < PLANET_COUNT) {
        int p_size = 512; Uint32 *pixels = SDL_malloc(p_size * p_size * sizeof(Uint32));
        SDL_memset(pixels, 0, p_size * p_size * sizeof(Uint32));
        DrawPlanetToBuffer(pixels, p_size, (float)s->assets_generated * 567.89f);
        s->planet_textures[s->assets_generated] = SDL_CreateTexture(s->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, p_size, p_size);
        SDL_SetTextureBlendMode(s->planet_textures[s->assets_generated], SDL_BLENDMODE_BLEND);
        SDL_UpdateTexture(s->planet_textures[s->assets_generated], NULL, pixels, p_size * 4);
        SDL_free(pixels);
    } else if (s->assets_generated < PLANET_COUNT + GALAXY_COUNT) {
        int g_idx = s->assets_generated - PLANET_COUNT;
        int g_size = 1024; Uint32 *pixels = SDL_malloc(g_size * g_size * sizeof(Uint32));
        SDL_memset(pixels, 0, g_size * g_size * sizeof(Uint32));
        DrawGalaxyToBuffer(pixels, g_size, (float)g_idx * 123.45f + 99.0f);
        s->galaxy_textures[g_idx] = SDL_CreateTexture(s->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, g_size, g_size);
        SDL_SetTextureBlendMode(s->galaxy_textures[g_idx], SDL_BLENDMODE_BLEND);
        SDL_UpdateTexture(s->galaxy_textures[g_idx], NULL, pixels, g_size * 4);
        SDL_free(pixels);
    } else {
        int a_idx = s->assets_generated - (PLANET_COUNT + GALAXY_COUNT);
        int a_size = 256; Uint32 *pixels = SDL_malloc(a_size * a_size * sizeof(Uint32));
        SDL_memset(pixels, 0, a_size * a_size * sizeof(Uint32));
        DrawAsteroidToBuffer(pixels, a_size, (float)a_idx * 432.1f + 11.0f);
        s->asteroid_textures[a_idx] = SDL_CreateTexture(s->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, a_size, a_size);
        SDL_SetTextureBlendMode(s->asteroid_textures[a_idx], SDL_BLENDMODE_BLEND);
        SDL_UpdateTexture(s->asteroid_textures[a_idx], NULL, pixels, a_size * 4);
        SDL_free(pixels);
    }
    
    s->assets_generated++;
    if (s->assets_generated >= total_assets) s->is_loading = false;
}

void Renderer_DrawLoading(AppState *s) {
    int win_w, win_h; SDL_GetRenderOutputSize(s->renderer, &win_w, &win_h);
    SDL_SetRenderDrawColor(s->renderer, 0, 0, 0, 255); SDL_RenderClear(s->renderer);
    float progress = (float)s->assets_generated / (float)(PLANET_COUNT + GALAXY_COUNT + ASTEROID_TYPE_COUNT);
    float bar_w = 400.0f, bar_h = 20.0f; 
    float x = (win_w - bar_w) / 2.0f, y = (win_h - bar_h) / 2.0f;
    float title_scale = 4.0f;
    SDL_SetRenderScale(s->renderer, title_scale, title_scale);
    const char *title = "Asteroidz";
    float char_size = 8.0f; 
    float scaled_win_w = (float)win_w / title_scale;
    float scaled_y = (y / title_scale);
    float title_x = (scaled_win_w - (SDL_strlen(title) * char_size)) / 2.0f;
    SDL_SetRenderDrawColor(s->renderer, 255, 255, 255, 255);
    SDL_RenderDebugText(s->renderer, title_x, scaled_y - 30, title);
    SDL_SetRenderScale(s->renderer, 1.0f, 1.0f);
    SDL_SetRenderDrawColor(s->renderer, 200, 200, 200, 255);
    SDL_RenderDebugText(s->renderer, x, y - 25, "Loading ...");
    SDL_SetRenderDrawColor(s->renderer, 50, 50, 50, 255); 
    SDL_RenderFillRect(s->renderer, &(SDL_FRect){ x, y, bar_w, bar_h });
    SDL_SetRenderDrawColor(s->renderer, 100, 200, 255, 255); 
    SDL_RenderFillRect(s->renderer, &(SDL_FRect){ x, y, bar_w * progress, bar_h });
    SDL_RenderPresent(s->renderer);
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
        float wx = cam_pos.x + (x * BG_SCALE_FACTOR) / zoom;
        float wy = cam_pos.y + (y * BG_SCALE_FACTOR) / zoom;
        
        float n = ValueNoise2D(wx * s_macro + dmx, wy * s_macro + dmy) * w_macro +
                  ValueNoise2D(wx * s_low   + ddx, wy * s_low   + ddy) * w_low +
                  ValueNoise2D(wx * s_med   + ddx, wy * s_med   + ddy) * w_med;
        
        float variation = ValueNoise2D(wx * color_scale + 12345.0f, wy * color_scale + 67890.0f);
        if (n > 1.0f) n = 1.0f; float r_f, g_f, b_f; GetNebulaColor(n, &r_f, &g_f, &b_f);
        r_f *= (0.95f + variation * 0.1f); g_f *= (0.95f + (1.0f - variation) * 0.1f);
        float intensity = 0.5f + n * 0.5f;
        Uint8 r = (Uint8)fminf(r_f * intensity, 255.0f), g = (Uint8)fminf(g_f * intensity, 255.0f), b = (Uint8)fminf(b_f * intensity, 255.0f);
        
        float af = (n - 0.1f) / 0.6f; af = fmaxf(0.0f, fminf(1.0f, af)); 
        Uint8 a = (Uint8)(af * 160); 
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
  s->is_loading = true; s->assets_generated = 0;
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

static void StarLayerFn(SDL_Renderer *r, const AppState *s, const LayerCell *cell) {
    if (cell->seed > 0.85f) {
        int win_w, win_h; SDL_GetRenderOutputSize(r, &win_w, &win_h);
        int cell_size = 128;
        float ox = DeterministicHash(cell->gx + 10, cell->gy + 20) * cell_size;
        float oy = DeterministicHash(cell->gx + 30, cell->gy + 40) * cell_size;
        float dx = sinf(s->current_time * 0.2f + cell->seed * 10.0f) * 15.0f;
        float dy = cosf(s->current_time * 0.15f + cell->seed * 5.0f) * 15.0f;
        float sx = cell->screen_x + (ox + dx) * s->zoom;
        float sy = cell->screen_y + (oy + dy) * s->zoom;
        float sz_s = DeterministicHash(cell->gx + 55, cell->gy + 66);
        float sz = (sz_s > 0.98f) ? 3.0f : (sz_s > 0.90f ? 2.0f : 1.0f);
        float s_sz = sz * s->zoom;
        if (!IsVisible(sx, sy, s_sz / 2, win_w, win_h)) return;
        float b_s = DeterministicHash(cell->gx + 77, cell->gy + 88), c_s = DeterministicHash(cell->gx + 99, cell->gy + 11);
        Uint8 val = (Uint8)(100 + b_s * 155), rv = val, gv = val, bv = val;
        if (c_s > 0.90f) { rv = (Uint8)(val * 0.8f); gv = (Uint8)(val * 0.9f); bv = 255; }
        else if (c_s > 0.80f) { rv = 220; gv = (Uint8)(val * 0.7f); bv = 255; }
        SDL_SetRenderDrawColor(r, rv, gv, bv, 200);
        if (s_sz <= 1.0f) SDL_RenderPoint(r, sx, sy);
        else { SDL_FRect rct = { sx - s_sz / 2, sy - s_sz / 2, s_sz, s_sz }; SDL_RenderFillRect(r, &rct); }
    }
}

static void SystemLayerFn(SDL_Renderer *r, const AppState *s, const LayerCell *cell) {
    // Rarer spawning: 1 in 50 cells (seed > 0.98)
    if (cell->seed > 0.98f || (cell->gx == 0 && cell->gy == 0)) {
        int win_w, win_h; SDL_GetRenderOutputSize(r, &win_w, &win_h);
        int cell_size = 5000;
        
        // Centered position with slight jitter (+/- 1000px)
        float jitter_x = (DeterministicHash(cell->gx + 5, cell->gy + 9) - 0.5f) * 2000.0f;
        float jitter_y = (DeterministicHash(cell->gx + 12, cell->gy + 3) - 0.5f) * 2000.0f;
        float ox = (cell_size / 2.0f) + jitter_x;
        float oy = (cell_size / 2.0f) + jitter_y;
        
        float sx = cell->screen_x + ox * s->zoom, sy = cell->screen_y + oy * s->zoom;
        float type_seed = DeterministicHash(cell->gx, cell->gy + 2000);
        
        if (type_seed > 0.95f) { // 5% chance for Galaxy
            float r_s = DeterministicHash(cell->gx + 3, cell->gy + 5), rad = (2000.0f + r_s * 4000.0f) * s->zoom;
            if (!IsVisible(sx, sy, rad, win_w, win_h)) return;
            int g_idx = (int)(DeterministicHash(cell->gx + 9, cell->gy + 2) * GALAXY_COUNT);
            SDL_RenderTexture(r, s->galaxy_textures[g_idx], NULL, &(SDL_FRect){ sx - rad, sy - rad, rad * 2, rad * 2 });
        } else { // 95% chance for Planet
            float r_s = DeterministicHash(cell->gx + 7, cell->gy + 11), rad = (150.0f + r_s * 450.0f) * s->zoom;
            if (!IsVisible(sx, sy, rad, win_w, win_h)) return;
            int p_idx = (int)(DeterministicHash(cell->gx + 1, cell->gy + 1) * PLANET_COUNT);
            float t_sz = rad * 4.5f; 
            SDL_RenderTexture(r, s->planet_textures[p_idx], NULL, &(SDL_FRect){ sx - t_sz / 2, sy - t_sz / 2, t_sz, t_sz });
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

static void DrawMinimap(SDL_Renderer *r, const AppState *s, int win_w, int win_h) {
    float mm_x = win_w - MINIMAP_SIZE - MINIMAP_MARGIN; float mm_y = win_h - MINIMAP_SIZE - MINIMAP_MARGIN;
    float world_to_mm = MINIMAP_SIZE / MINIMAP_RANGE;
    SDL_SetRenderDrawColor(r, 20, 20, 30, 180); SDL_RenderFillRect(r, &(SDL_FRect){ mm_x, mm_y, MINIMAP_SIZE, MINIMAP_SIZE });
    SDL_SetRenderDrawColor(r, 80, 80, 100, 255); SDL_RenderRect(r, &(SDL_FRect){ mm_x, mm_y, MINIMAP_SIZE, MINIMAP_SIZE });
    float parallax = 0.7f; Vec2 p_cam = GetParallaxCam(s, parallax, win_w, win_h);
    float p_cam_center_x = p_cam.x + (win_w / 2.0f) / s->zoom; float p_cam_center_y = p_cam.y + (win_h / 2.0f) / s->zoom;
    int cell_size = 5000; int r_cells = (int)(MINIMAP_RANGE / cell_size) + 1;
    int scx = (int)floorf((p_cam_center_x - MINIMAP_RANGE/2) / cell_size);
    int scy = (int)floorf((p_cam_center_y - MINIMAP_RANGE/2) / cell_size);
    for (int gy = scy; gy <= scy + r_cells; gy++) {
        for (int gx = scx; gx <= scx + r_cells; gx++) {
            float seed = DeterministicHash(gx, gy + 1000);
            if (seed > 0.98f || (gx == 0 && gy == 0)) {
                float jitter_x = (DeterministicHash(gx + 5, gy + 9) - 0.5f) * 2000.0f;
                float jitter_y = (DeterministicHash(gx + 12, gy + 3) - 0.5f) * 2000.0f;
                float wx = gx * cell_size + (cell_size / 2.0f) + jitter_x;
                float wy = gy * cell_size + (cell_size / 2.0f) + jitter_y;
                float dx = (wx - p_cam_center_x); float dy = (wy - p_cam_center_y);
                if (fabsf(dx) < MINIMAP_RANGE/2 && fabsf(dy) < MINIMAP_RANGE/2) {
                    float px = mm_x + MINIMAP_SIZE/2 + dx * world_to_mm; float py = mm_y + MINIMAP_SIZE/2 + dy * world_to_mm;
                    float type_seed = DeterministicHash(gx, gy + 2000);
                    if (type_seed > 0.95f) SDL_SetRenderDrawColor(r, 200, 150, 255, 255); // Galaxy
                    else SDL_SetRenderDrawColor(r, 100, 200, 255, 255); // Planet
                    SDL_RenderFillRect(r, &(SDL_FRect){ px, py, 4, 4 });
                }
            }
        }
    }
    float view_w = (win_w / s->zoom) * world_to_mm; float view_h = (win_h / s->zoom) * world_to_mm;
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255); SDL_RenderRect(r, &(SDL_FRect){ mm_x + (MINIMAP_SIZE - view_w)/2, mm_y + (MINIMAP_SIZE - view_h)/2, view_w, view_h });
}

static void AsteroidLayerFn(SDL_Renderer *r, const AppState *s, const LayerCell *cell) {
    if (cell->seed > 0.70f) { // Dense spawning: 30% of cells
        int win_w, win_h; SDL_GetRenderOutputSize(r, &win_w, &win_h);
        int cell_size = 1000;
        
        float ox = DeterministicHash(cell->gx + 7, cell->gy + 3) * cell_size;
        float oy = DeterministicHash(cell->gx + 1, cell->gy + 9) * cell_size;
        float sx = cell->screen_x + ox * s->zoom, sy = cell->screen_y + oy * s->zoom;
        
        // Increased radius range: 30 to 230 units (was 20 to 80)
        float r_s = DeterministicHash(cell->gx + 4, cell->gy + 2);
        float rad = (30.0f + r_s * 200.0f) * s->zoom;
        
        if (!IsVisible(sx, sy, rad, win_w, win_h)) return;

        int a_idx = (int)(DeterministicHash(cell->gx + 5, cell->gy + 5) * ASTEROID_TYPE_COUNT);
        float rot = DeterministicHash(cell->gx, cell->gy) * 360.0f + s->current_time * 10.0f * (r_s - 0.5f);
        
        SDL_RenderTextureRotated(r, s->asteroid_textures[a_idx], NULL, 
                                 &(SDL_FRect){ sx - rad, sy - rad, rad * 2, rad * 2 }, 
                                 rot, NULL, SDL_FLIP_NONE);
    }
}

void Renderer_Draw(AppState *s) {
  int ww, wh; SDL_GetRenderOutputSize(s->renderer, &ww, &wh);
  SDL_SetRenderDrawColor(s->renderer, 0, 0, 0, 255); SDL_RenderClear(s->renderer);
  UpdateBackground(s);
  if (s->bg_texture) { SDL_RenderTexture(s->renderer, s->bg_texture, NULL, NULL); }
  DrawParallaxLayer(s->renderer, s, ww, wh, 128, 0.4f, 0, StarLayerFn);
  DrawParallaxLayer(s->renderer, s, ww, wh, 5000, 0.7f, 1000, SystemLayerFn);
  
  // 4. Primary Layer (Asteroids, Units, Grid)
  DrawParallaxLayer(s->renderer, s, ww, wh, 1000, 1.0f, 2000, AsteroidLayerFn);
  
  if (s->show_grid) { DrawGrid(s->renderer, s, ww, wh); }
  DrawDebugInfo(s->renderer, s, ww);
  DrawMinimap(s->renderer, s, ww, wh);
  SDL_RenderPresent(s->renderer);
}