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

static Vec2 WorldToScreenParallax(Vec2 world_pos, float parallax, const AppState *s, int win_w, int win_h) {
    float sw = (float)win_w, sh = (float)win_h;
    
    // World center of the camera
    float cx = s->camera_pos.x + (sw / 2.0f) / s->zoom;
    float cy = s->camera_pos.y + (sh / 2.0f) / s->zoom;

    // Displacement of object from camera center, scaled by parallax
    Vec2 p_obj = WorldToParallax(world_pos, parallax);
    Vec2 p_cam = WorldToParallax((Vec2){cx, cy}, parallax);
    float dx = p_obj.x - p_cam.x;
    float dy = p_obj.y - p_cam.y;

    // Final screen position (centered)
    return (Vec2){
        (sw / 2.0f) + dx * s->zoom,
        (sh / 2.0f) + dy * s->zoom
    };
}

static bool IsVisible(float sx, float sy, float radius, int win_w, int win_h) {
    return (sx + radius >= 0 && sx - radius <= win_w && 
            sy + radius >= 0 && sy - radius <= win_h);
}

static void DrawParallaxLayer(SDL_Renderer *r, const AppState *s, int win_w, int win_h, int cell_size, float parallax, float seed_offset, LayerDrawFn draw_fn) {
    float sw = (float)win_w, sh = (float)win_h;
    float cx = s->camera_pos.x + (sw / 2.0f) / s->zoom;
    float cy = s->camera_pos.y + (sh / 2.0f) / s->zoom;

    float visible_w = sw / (s->zoom * parallax);
    float visible_h = sh / (s->zoom * parallax);

    float min_wx = cx - visible_w / 2.0f;
    float min_wy = cy - visible_h / 2.0f;
    float max_wx = cx + visible_w / 2.0f;
    float max_wy = cy + visible_h / 2.0f;

    int sgx = (int)floorf(min_wx / (float)cell_size);
    int sgy = (int)floorf(min_wy / (float)cell_size);
    int egx = (int)ceilf(max_wx / (float)cell_size);
    int egy = (int)ceilf(max_wy / (float)cell_size);

    for (int gy = sgy; gy <= egy; gy++) {
        for (int gx = sgx; gx <= egx; gx++) {
            LayerCell cell;
            cell.gx = gx; cell.gy = gy;
            cell.seed = DeterministicHash(gx, gy + (int)seed_offset);
            
            Vec2 world_pos = {(float)gx * cell_size, (float)gy * cell_size};
            Vec2 screen_pos = WorldToScreenParallax(world_pos, parallax, s, win_w, win_h);
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

static void HueToRGB(float h, float *r, float *g, float *b) {
    float r_f = fabsf(h * 6.0f - 3.0f) - 1.0f;
    float g_f = 2.0f - fabsf(h * 6.0f - 2.0f);
    float b_f = 2.0f - fabsf(h * 6.0f - 4.0f);
    *r = fmaxf(0.0f, fminf(1.0f, r_f));
    *g = fmaxf(0.0f, fminf(1.0f, g_f));
    *b = fmaxf(0.0f, fminf(1.0f, b_f));
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
    if (theme > 0.90f) { tr = 1.0f; tg = 0.8f; tb = 0.2f; tr2 = 1.0f; tg2 = 0.1f; tb2 = 0.1f; }
    else if (theme > 0.80f) { tr = 0.2f; tg = 1.0f; tb = 0.3f; tr2 = 1.0f; tg2 = 1.0f; tb2 = 0.2f; }
    else if (theme > 0.50f) { tr = 0.1f; tg = 0.2f; tb = 1.0f; tr2 = 0.2f; tg2 = 0.9f; tb2 = 1.0f; }
    else if (theme > 0.25f) { tr = 0.5f; tg = 0.1f; tb = 1.0f; tr2 = 1.0f; tg2 = 0.2f; tb2 = 0.8f; }
    else { tr = 0.1f; tg = 0.8f; tb = 0.8f; tr2 = 0.1f; tg2 = 0.3f; tb2 = 1.0f; }
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
    float base_radius = size * 0.15f; 
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float dx = (float)(x - center), dy = (float)(y - center);
            float dist = sqrtf(dx*dx + dy*dy);
            if (dist < 1.0f) continue;
            float angle = atan2f(dy, dx);
            float shape_n = 0.0f;
            float s_freq = 1.2f, s_amp = 0.6f;
            for(int o=0; o<3; o++) {
                shape_n += (PerlinNoise2D(cosf(angle) * s_freq + seed, sinf(angle) * s_freq + seed) - 0.5f) * s_amp;
                s_freq *= 2.0f; s_amp *= 0.5f;
            }
            float distorted_radius = base_radius * (1.0f + shape_n);
            if (dist <= distorted_radius) {
                float warp = ValueNoise2D(x * 0.02f + seed, y * 0.02f + seed) * 0.5f;
                float cracks1 = VoronoiCracks2D(x * 0.03f + seed + warp, y * 0.03f + seed + warp);
                float cracks2 = VoronoiCracks2D(x * 0.1f + seed * 2.0f, y * 0.1f + seed * 3.0f);
                float surface_v = ValueNoise2D(x * 0.05f + seed, y * 0.05f + seed);
                float mat_n = ValueNoise2D(x * 0.04f + seed * 4.0f, y * 0.04f + seed * 4.0f);
                float mix_val = fmaxf(0.0f, fminf(1.0f, (mat_n - 0.3f) * 1.5f)); 
                float a_theme = DeterministicHash((int)(seed * 789.0f), 123);
                float r1, g1, b1, r2, g2, b2;
                if (a_theme > 0.7f) { r1 = 0.5f; g1 = 0.45f; b1 = 0.42f; r2 = 0.75f; g2 = 0.4f; b2 = 0.35f; }
                else if (a_theme > 0.4f) { r1 = 0.55f; g1 = 0.55f; b1 = 0.6f; r2 = 0.45f; g2 = 0.5f; b2 = 0.75f; }
                else { r1 = 0.35f; g1 = 0.35f; b1 = 0.38f; r2 = 0.55f; g2 = 0.55f; b2 = 0.58f; }
                float tr = r1 * (1.0f - mix_val) + r2 * mix_val;
                float tg = g1 * (1.0f - mix_val) + g2 * mix_val;
                float tb = b1 * (1.0f - mix_val) + b2 * mix_val;
                float base_val = 30.0f + surface_v * 35.0f + shape_n * 15.0f;
                float c1_factor = fmaxf(0.0f, fminf(1.0f, cracks1 / 0.4f));
                float soft_darken1 = 0.4f + 0.6f * powf(c1_factor, 0.5f);
                float c2_factor = fmaxf(0.0f, fminf(1.0f, cracks2 / 0.2f));
                float soft_darken2 = 0.7f + 0.3f * powf(c2_factor, 0.5f);
                Uint8 r = (Uint8)fminf(255, base_val * tr * soft_darken1 * soft_darken2);
                Uint8 g = (Uint8)fminf(255, base_val * tg * soft_darken1 * soft_darken2);
                Uint8 b = (Uint8)fminf(255, base_val * tb * soft_darken1 * soft_darken2);
                pixels[y * size + x] = (255 << 24) | (b << 16) | (g << 8) | r;
            } else {
                float dust_noise = PerlinNoise2D(cosf(angle) * 0.8f + seed + 100, sinf(angle) * 0.8f + seed + 100);
                float dust_outer = base_radius * (1.5f + dust_noise * 3.0f); 
                if (dist <= dust_outer) {
                    float dust_t = (dist - distorted_radius) / (dust_outer - distorted_radius);
                    float detail_n = PerlinNoise2D(x * 0.02f + seed + 500, y * 0.02f + seed);
                    float edge_dist = fminf(fminf(x, (size-1)-x), fminf(y, (size-1)-y));
                    float edge_falloff = fminf(1.0f, edge_dist / 30.0f);
                    float alpha_f = powf(1.0f - dust_t, 2.5f) * detail_n * edge_falloff;
                    if (alpha_f > 0.01f) {
                        Uint8 val = (Uint8)(35 + detail_n * 25); 
                        pixels[y * size + x] = ((Uint8)(alpha_f * 100) << 24) | (val << 16) | (val << 8) | val;
                    }
                }
            }
        }
    }
}

static void DrawExplosionPuffToBuffer(Uint32 *pixels, int size, float seed) {
    int center = size / 2;
    float max_rad = size * 0.45f;
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float dx = (float)(x - center), dy = (float)(y - center);
            float dist = sqrtf(dx*dx + dy*dy);
            if (dist > max_rad) continue;
            float norm_dist = dist / max_rad;
            float n1 = PerlinNoise2D(x * 0.05f + seed, y * 0.05f + seed);
            float n2 = PerlinNoise2D(x * 0.1f + seed * 2, y * 0.1f + seed * 2);
            float noise = (n1 * 0.7f + n2 * 0.3f);
            float alpha_f = powf(1.0f - norm_dist, 1.5f) * noise;
            if (alpha_f > 0.05f) {
                Uint8 val = (Uint8)(100 + noise * 100);
                pixels[y * size + x] = ((Uint8)(alpha_f * 255) << 24) | (val << 16) | (val << 8) | val;
            }
        }
    }
}

void Renderer_GeneratePlanetStep(AppState *s) {
    int total_assets = PLANET_COUNT + GALAXY_COUNT + ASTEROID_TYPE_COUNT + 1;
    if (s->assets_generated >= total_assets) { s->is_loading = false; return; }
    if (s->assets_generated < PLANET_COUNT) {
        int p_size = 1024; Uint32 *pixels = SDL_malloc(p_size * p_size * sizeof(Uint32));
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
    } else if (s->assets_generated < PLANET_COUNT + GALAXY_COUNT + ASTEROID_TYPE_COUNT) {
        int a_idx = s->assets_generated - (PLANET_COUNT + GALAXY_COUNT);
        int a_size = 512; Uint32 *pixels = SDL_malloc(a_size * a_size * sizeof(Uint32));
        SDL_memset(pixels, 0, a_size * a_size * sizeof(Uint32));
        DrawAsteroidToBuffer(pixels, a_size, (float)a_idx * 432.1f + 11.0f);
        s->asteroid_textures[a_idx] = SDL_CreateTexture(s->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, a_size, a_size);
        SDL_SetTextureBlendMode(s->asteroid_textures[a_idx], SDL_BLENDMODE_BLEND);
        SDL_UpdateTexture(s->asteroid_textures[a_idx], NULL, pixels, a_size * 4);
        SDL_free(pixels);
    } else {
        int e_size = 256; Uint32 *pixels = SDL_malloc(e_size * e_size * sizeof(Uint32));
        SDL_memset(pixels, 0, e_size * e_size * sizeof(Uint32));
        DrawExplosionPuffToBuffer(pixels, e_size, 777.7f);
        s->explosion_puff_texture = SDL_CreateTexture(s->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, e_size, e_size);
        SDL_SetTextureBlendMode(s->explosion_puff_texture, SDL_BLENDMODE_BLEND);
        SDL_UpdateTexture(s->explosion_puff_texture, NULL, pixels, e_size * 4);
        SDL_free(pixels);
    }
    s->assets_generated++;
    if (s->assets_generated >= total_assets) s->is_loading = false;
}

void Renderer_DrawLoading(AppState *s) {
    int win_w, win_h; SDL_GetRenderOutputSize(s->renderer, &win_w, &win_h);
    SDL_SetRenderDrawColor(s->renderer, 0, 0, 0, 255); SDL_RenderClear(s->renderer);
    float progress = (float)s->assets_generated / (float)(PLANET_COUNT + GALAXY_COUNT + ASTEROID_TYPE_COUNT + 1);
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
        float sz_s = DeterministicHash(cell->gx + 55, cell->gy + 66);
        float sz = (sz_s > 0.98f) ? 3.0f : (sz_s > 0.90f ? 2.0f : 1.0f);
        
        // Add jitter within the cell to prevent rectangular appearance
        float jx = DeterministicHash(cell->gx + 7, cell->gy + 3) * 128.0f;
        float jy = DeterministicHash(cell->gx + 1, cell->gy + 9) * 128.0f;
        Vec2 world_pos = {(float)cell->gx * 128 + jx, (float)cell->gy * 128 + jy};
        
        Vec2 sx_y = WorldToScreenParallax(world_pos, 0.4f, s, win_w, win_h);
        float s_sz = sz * s->zoom;
        if (!IsVisible(sx_y.x, sx_y.y, s_sz / 2, win_w, win_h)) return;
        
        float b_s = DeterministicHash(cell->gx + 77, cell->gy + 88), c_s = DeterministicHash(cell->gx + 99, cell->gy + 11);
        Uint8 val = (Uint8)(100 + b_s * 155), rv = val, gv = val, bv = val;
        if (c_s > 0.90f) { rv = (Uint8)(val * 0.8f); gv = (Uint8)(val * 0.9f); bv = 255; }
        else if (c_s > 0.80f) { rv = 220; gv = (Uint8)(val * 0.7f); bv = 255; }
        SDL_SetRenderDrawColor(r, rv, gv, bv, 200);
        if (s_sz <= 1.0f) SDL_RenderPoint(r, sx_y.x, sx_y.y);
        else { SDL_FRect rct = { sx_y.x - s_sz / 2, sx_y.y - s_sz / 2, s_sz, s_sz }; SDL_RenderFillRect(r, &rct); }
    }
}

static void SystemLayerFn(SDL_Renderer *r, const AppState *s, const LayerCell *cell) {
    Vec2 b_pos; float type_seed;
    if (GetCelestialBodyInfo(cell->gx, cell->gy, &b_pos, &type_seed)) {
        int win_w, win_h; SDL_GetRenderOutputSize(r, &win_w, &win_h);
        Vec2 screen_pos = WorldToScreenParallax(b_pos, 0.7f, s, win_w, win_h);
        float sx = screen_pos.x, sy = screen_pos.y;

        if (type_seed > 0.95f) {
            float r_s = DeterministicHash(cell->gx + 3, cell->gy + 5), rad = (2000.0f + r_s * 4000.0f) * s->zoom;
            if (!IsVisible(sx, sy, rad, win_w, win_h)) return;
            int g_idx = (int)(DeterministicHash(cell->gx + 9, cell->gy + 2) * GALAXY_COUNT);
            SDL_RenderTexture(r, s->galaxy_textures[g_idx], NULL, &(SDL_FRect){ sx - rad, sy - rad, rad * 2, rad * 2 });
        } else {
            float r_s = DeterministicHash(cell->gx + 7, cell->gy + 11), rad = (150.0f + r_s * 450.0f) * s->zoom;
            if (!IsVisible(sx, sy, rad, win_w, win_h)) return;
            int p_idx = (int)(DeterministicHash(cell->gx + 1, cell->gy + 1) * PLANET_COUNT);
            float t_sz = rad * 4.5f; 
            SDL_RenderTexture(r, s->planet_textures[p_idx], NULL, &(SDL_FRect){ sx - t_sz / 2, sy - t_sz / 2, t_sz, t_sz });
        }
    }
}

static void Renderer_DrawAsteroids(SDL_Renderer *r, const AppState *s, int win_w, int win_h) {
    for (int i = 0; i < MAX_ASTEROIDS; i++) {
        if (!s->asteroids[i].active) continue;
        Vec2 sx_y = WorldToScreenParallax(s->asteroids[i].pos, 1.0f, s, win_w, win_h);
        float rad = s->asteroids[i].radius * s->zoom;
        if (!IsVisible(sx_y.x, sx_y.y, rad * 1.5f, win_w, win_h)) continue;
        SDL_RenderTextureRotated(r, s->asteroid_textures[s->asteroids[i].tex_idx], NULL, 
                                 &(SDL_FRect){ sx_y.x - rad * 1.5f, sx_y.y - rad * 1.5f, rad * 3.0f, rad * 3.0f }, 
                                 s->asteroids[i].rotation, NULL, SDL_FLIP_NONE);
    }
}

static void Renderer_DrawParticles(SDL_Renderer *r, const AppState *s, int win_w, int win_h) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!s->particles[i].active) continue;
        Vec2 sx_y = WorldToScreenParallax(s->particles[i].pos, 1.0f, s, win_w, win_h);
        float sz = s->particles[i].size * s->zoom;
        if (!IsVisible(sx_y.x, sx_y.y, sz, win_w, win_h)) continue;
        if (s->particles[i].type == PARTICLE_PUFF && s->explosion_puff_texture) {
            SDL_SetTextureColorMod(s->explosion_puff_texture, s->particles[i].color.r, s->particles[i].color.g, s->particles[i].color.b);
            SDL_SetTextureAlphaMod(s->explosion_puff_texture, (Uint8)(s->particles[i].life * 180));
            SDL_RenderTexture(r, s->explosion_puff_texture, NULL, &(SDL_FRect){ sx_y.x - sz/2, sx_y.y - sz/2, sz, sz });
        } else {
            SDL_SetRenderDrawColor(r, s->particles[i].color.r, s->particles[i].color.g, s->particles[i].color.b, (Uint8)(s->particles[i].life * 255));
            SDL_RenderFillRect(r, &(SDL_FRect){ sx_y.x - sz/2, sx_y.y - sz/2, sz, sz });
        }
    }
}

static void DrawGrid(SDL_Renderer *renderer, const AppState *s, int win_w, int win_h) {
  SDL_SetRenderDrawColor(renderer, 50, 50, 50, 40);
  int gs = 200;
  int stx = (int)floorf(s->camera_pos.x/gs)*gs, sty = (int)floorf(s->camera_pos.y/gs)*gs;
  for (float x = stx; x < s->camera_pos.x + win_w/s->zoom + gs; x += gs) {
    Vec2 s1 = WorldToScreenParallax((Vec2){x, 0}, 1.0f, s, win_w, win_h);
    SDL_RenderLine(renderer, s1.x, 0, s1.x, (float)win_h); 
  }
  for (float y = sty; y < s->camera_pos.y + win_h/s->zoom + gs; y += gs) {
    Vec2 s1 = WorldToScreenParallax((Vec2){0, y}, 1.0f, s, win_w, win_h);
    SDL_RenderLine(renderer, 0, s1.y, (float)win_w, s1.y); 
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
    
    // Minimap uses world-center reference
    float sw = (float)win_w, sh = (float)win_h;
    float cx = s->camera_pos.x + (sw / 2.0f) / s->zoom;
    float cy = s->camera_pos.y + (sh / 2.0f) / s->zoom;

    SDL_SetRenderDrawColor(r, 20, 20, 30, 180); SDL_RenderFillRect(r, &(SDL_FRect){ mm_x, mm_y, MINIMAP_SIZE, MINIMAP_SIZE });
    SDL_SetRenderDrawColor(r, 80, 80, 100, 255); SDL_RenderRect(r, &(SDL_FRect){ mm_x, mm_y, MINIMAP_SIZE, MINIMAP_SIZE });

    int cell_size = 5000; int r_cells = (int)(MINIMAP_RANGE / cell_size) + 1;
    int scx = (int)floorf((cx - MINIMAP_RANGE/2) / cell_size);
    int scy = (int)floorf((cy - MINIMAP_RANGE/2) / cell_size);
    for (int gy = scy; gy <= scy + r_cells; gy++) {
        for (int gx = scx; gx <= scx + r_cells; gx++) {
            Vec2 b_pos; float type_seed;
            if (GetCelestialBodyInfo(gx, gy, &b_pos, &type_seed)) {
                // Adjust for parallax 0.7 in minimap? 
                // Let's draw celestial bodies relative to camera center
                float dx = (b_pos.x - cx); float dy = (b_pos.y - cy);
                if (fabsf(dx) < MINIMAP_RANGE/2 && fabsf(dy) < MINIMAP_RANGE/2) {
                    float px = mm_x + MINIMAP_SIZE/2 + dx * world_to_mm; 
                    float py = mm_y + MINIMAP_SIZE/2 + dy * world_to_mm;
                    if (type_seed > 0.95f) SDL_SetRenderDrawColor(r, 200, 150, 255, 255);
                    else SDL_SetRenderDrawColor(r, 100, 200, 255, 255);
                    SDL_RenderFillRect(r, &(SDL_FRect){ px, py, 4, 4 });
                }
            }
        }
    }
    float view_w = (win_w / s->zoom) * world_to_mm; float view_h = (win_h / s->zoom) * world_to_mm;
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255); SDL_RenderRect(r, &(SDL_FRect){ mm_x + (MINIMAP_SIZE - view_w)/2, mm_y + (MINIMAP_SIZE - view_h)/2, view_w, view_h });
}

static void DrawDensityGrid(SDL_Renderer *r, const AppState *s, int win_w, int win_h) {
    int cell_size = 5000;
    float parallax = 1.0f; // Everything is 1.0 now
    float sw = (float)win_w, sh = (float)win_h;
    
    // World center of the camera
    float cx = s->camera_pos.x + (sw / 2.0f) / s->zoom;
    float cy = s->camera_pos.y + (sh / 2.0f) / s->zoom;

    // View bounds in the 1.0 world
    float vw = sw / s->zoom, vh = sh / s->zoom;

    // Search celestial grid
    int sgx = (int)floorf((cx - vw/2)/cell_size), sgy = (int)floorf((cy - vh/2)/cell_size);
    int egx = (int)ceilf((cx + vw/2)/cell_size), egy = (int)ceilf((cy + vh/2)/cell_size);

    for (int gy = sgy; gy <= egy; gy++) {
        for (int gx = sgx; gx <= egx; gx++) {
            Vec2 b_pos; float b_type;
            if (GetCelestialBodyInfo(gx, gy, &b_pos, &b_type)) {
                // Cross at celestial body visual position
                Vec2 sx_y = WorldToScreenParallax(b_pos, parallax, s, win_w, win_h);
                if (IsVisible(sx_y.x, sx_y.y, 100, win_w, win_h)) {
                    SDL_SetRenderDrawColor(r, 0, 255, 0, 255);
                    SDL_RenderLine(r, sx_y.x - 20, sx_y.y, sx_y.x + 20, sx_y.y);
                    SDL_RenderLine(r, sx_y.x, sx_y.y - 20, sx_y.x, sx_y.y + 20);
                }
            }
        }
    }

    // Density visualization
    int d_cell = 2000;
    int dsgx = (int)floorf(s->camera_pos.x / d_cell), dsgy = (int)floorf(s->camera_pos.y / d_cell);
    int degx = (int)ceilf((s->camera_pos.x + vw) / d_cell), degy = (int)ceilf((s->camera_pos.y + vh) / d_cell);

    for (int gy = dsgy; gy <= degy; gy++) {
        for (int gx = dsgx; gx <= degx; gx++) {
            float wx = (float)gx * d_cell, wy = (float)gy * d_cell;
            Vec2 sc_pos = WorldToScreenParallax((Vec2){wx, wy}, 1.0f, s, win_w, win_h);
            float sz = (float)d_cell * s->zoom;

            int sub_res = 5; float sub_sz = sz / sub_res;
            for (int syy = 0; syy < sub_res; syy++) {
                for (int sxx = 0; sxx < sub_res; sxx++) {
                    float swx = wx + (sxx / (float)sub_res) * d_cell;
                    float swy = wy + (syy / (float)sub_res) * d_cell;
                    float d = GetAsteroidDensity((Vec2){swx, swy});
                    if (d > 0.05f) {
                        Uint8 rv = (Uint8)(fminf(1.0f, d / 1.5f) * 150.0f);
                        SDL_SetRenderDrawColor(r, rv, 0, 50, 40);
                        SDL_RenderFillRect(r, &(SDL_FRect){ sc_pos.x + sxx * sub_sz, sc_pos.y + syy * sub_sz, sub_sz + 1, sub_sz + 1 });
                    }
                }
            }
        }
    }
}

void Renderer_Draw(AppState *s) {
  int ww, wh; SDL_GetRenderOutputSize(s->renderer, &ww, &wh);
  SDL_SetRenderDrawColor(s->renderer, 0, 0, 0, 255); SDL_RenderClear(s->renderer);
  UpdateBackground(s);
  if (s->bg_texture) { SDL_RenderTexture(s->renderer, s->bg_texture, NULL, NULL); }
  DrawParallaxLayer(s->renderer, s, ww, wh, 128, 0.4f, 0, StarLayerFn);
  DrawParallaxLayer(s->renderer, s, ww, wh, 5000, 1.0f, 1000, SystemLayerFn);
  
  if (s->show_density) { DrawDensityGrid(s->renderer, s, ww, wh); }
  if (s->show_grid) { DrawGrid(s->renderer, s, ww, wh); }
  
  Renderer_DrawAsteroids(s->renderer, s, ww, wh);
  Renderer_DrawParticles(s->renderer, s, ww, wh);
  DrawDebugInfo(s->renderer, s, ww);
  DrawMinimap(s->renderer, s, ww, wh);
  SDL_RenderPresent(s->renderer);
}