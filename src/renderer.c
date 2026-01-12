#include "constants.h"
#include "game.h"
#include <math.h>
#include <stdio.h>

void Renderer_StartBackgroundThreads(AppState *s);

typedef struct {
  float pos;
  float r, g, b;
} ColorStop;

typedef struct {
  int gx, gy;
  float seed;
  float screen_x, screen_y;
  int win_w, win_h;
  int cell_size;
  float parallax;
} LayerCell;

typedef void (*LayerDrawFn)(SDL_Renderer *r, const AppState *s,
                            const LayerCell *cell);

static Vec2 WorldToScreenParallax(Vec2 world_pos, float parallax,
                                  const AppState *s, int win_w, int win_h) {
  float sw = (float)win_w, sh = (float)win_h;
  float zoom = s->zoom;
  float cx = s->camera_pos.x + (sw / 2.0f) / zoom;
  float cy = s->camera_pos.y + (sh / 2.0f) / zoom;
  return (Vec2){(sw / 2.0f) + (world_pos.x - cx) * parallax * zoom,
                (sh / 2.0f) + (world_pos.y - cy) * parallax * zoom};
}

static bool IsVisible(float sx, float sy, float radius, int win_w, int win_h) {
  return (sx + radius >= 0 && sx - radius <= win_w && sy + radius >= 0 &&
          sy - radius <= win_h);
}

static void DrawParallaxLayer(SDL_Renderer *r, const AppState *s, int win_w,
                              int win_h, int cell_size, float parallax,
                              float seed_offset, LayerDrawFn draw_fn) {
  float sw = (float)win_w, sh = (float)win_h;
  float zoom = s->zoom;
  float cx = s->camera_pos.x + (sw / 2.0f) / zoom;
  float cy = s->camera_pos.y + (sh / 2.0f) / zoom;
  float visible_w = sw / (zoom * parallax);
  float visible_h = sh / (zoom * parallax);
  float min_wx = cx - visible_w / 2.0f;
  float min_wy = cy - visible_h / 2.0f;
  float max_wx = cx + visible_w / 2.0f;
  float max_wy = cy + visible_h / 2.0f;
  int sgx = (int)floorf(min_wx / (float)cell_size);
  int sgy = (int)floorf(min_wy / (float)cell_size);
  int egx = (int)ceilf(max_wx / (float)cell_size);
  int egy = (int)ceilf(max_wy / (float)cell_size);
  float half_sw = sw / 2.0f;
  float half_sh = sh / 2.0f;
  float scale = parallax * zoom;
  for (int gy = sgy; gy <= egy; gy++) {
    for (int gx = sgx; gx <= egx; gx++) {
      LayerCell cell;
      cell.gx = gx; cell.gy = gy;
      cell.seed = DeterministicHash(gx, gy + (int)seed_offset);
      cell.win_w = win_w; cell.win_h = win_h;
      cell.cell_size = cell_size; cell.parallax = parallax;
      cell.screen_x = half_sw + ((float)gx * cell_size - cx) * scale;
      cell.screen_y = half_sh + ((float)gy * cell_size - cy) * scale;
      draw_fn(r, s, &cell);
    }
  }
}

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

static void GetGreenBlueGradient(float t, Uint8 *r, Uint8 *g, Uint8 *b) {
    // High contrast Green-to-Blue
    float ct = (t < 0.5f) ? powf(t * 2.0f, 3.0f) * 0.5f : 1.0f - powf((1.0f - t) * 2.0f, 3.0f) * 0.5f;
    *r = 0;
    *g = (Uint8)(255 * (1.0f - ct) + 10 * ct);
    *b = (Uint8)(40 * (1.0f - ct) + 255 * ct);
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
      float dist = sqrtf(dx * dx + dy * dy);
      if (dist <= atmo_outer) {
        float nx = dx / radius, ny = dy / radius;
        float nz = (dist <= radius) ? sqrtf(fmaxf(0.0f, 1.0f - nx * nx - ny * ny)) : 0.0f;
        float noise = 0.0f, amp = 0.7f, freq = 0.03f;
        for (int o = 0; o < 3; o++) { noise += PerlinNoise2D((float)x * freq + seed, (float)y * freq + seed * 2.0f) * amp; amp *= 0.3f; freq *= 2.0f; }
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
  if (theme > 0.90f) { tr = 1.0f; tg = 0.8f; tb = 0.2f; tr2 = 1.0f; tg2 = 0.1f; tb2 = 0.1f; }
  else if (theme > 0.80f) { tr = 0.2f; tg = 1.0f; tb = 0.3f; tr2 = 1.0f; tg2 = 1.0f; tb2 = 0.2f; }
  else if (theme > 0.50f) { tr = 0.1f; tg = 0.2f; tb = 1.0f; tr2 = 0.2f; tg2 = 0.9f; tb2 = 1.0f; }
  else if (theme > 0.25f) { tr = 0.5f; tg = 0.1f; tb = 1.0f; tr2 = 1.0f; tg2 = 0.2f; tb2 = 0.8f; }
  else { tr = 0.1f; tg = 0.8f; tb = 0.8f; tr2 = 0.1f; tg2 = 0.3f; tb2 = 1.0f; }
  float cloud_scale = 0.5f + DeterministicHash(s_int, 55) * 2.0f;
  for (int y = 0; y < size; y++) {
    for (int x = 0; x < size; x++) {
      float dx = (float)(x - center), dy = (float)(y - center);
      float dist = sqrtf(dx * dx + dy * dy);
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
        float mix_val = fmaxf(0.0f, fminf(1.0f, (cn - 0.5f) * (1.0f + norm_dist * 4.0f) + 0.5f));
        float cr = tr * (1.0f - mix_val) + tr2 * mix_val, cg = tg * (1.0f - mix_val) + tg2 * mix_val, cb = tb * (1.0f - mix_val) + tb2 * mix_val;
        float saturation = fminf(1.0f, norm_dist * 8.0f);
        cr = cr * saturation + (1.0f - saturation); cg = cg * saturation + (1.0f - saturation); cb = cb * saturation + (1.0f - saturation);
        Uint8 rv = (Uint8)fminf(255, final_val * (cr * 150 + core * 255)), gv = (Uint8)fminf(255, final_val * (cg * 150 + core * 255)), bv = (Uint8)fminf(255, final_val * (cb * 150 + core * 255)), av = (Uint8)fminf(255, final_val * 255 * fminf(1.0f, falloff * 5.0f));
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
      float dist = sqrtf(dx * dx + dy * dy);
      if (dist < 1.0f) continue;
      float angle = atan2f(dy, dx);
      float shape_n = 0.0f, s_freq = 1.2f, s_amp = 0.6f;
      for (int o = 0; o < 3; o++) { shape_n += (PerlinNoise2D(cosf(angle) * s_freq + seed, sinf(angle) * s_freq + seed) - 0.5f) * s_amp; s_freq *= 2.0f; s_amp *= 0.5f; }
      float distorted_radius = base_radius * (1.0f + shape_n);
      if (dist <= distorted_radius) {
        float warp = ValueNoise2D(x * 0.02f + seed, y * 0.02f + seed) * 0.5f;
        float cracks1 = VoronoiCracks2D(x * 0.03f + seed + warp, y * 0.03f + seed + warp);
        float cracks2 = VoronoiCracks2D(x * 0.1f + seed * 2.0f, y * 0.1f + seed * 3.0f);
        float mix_val = fmaxf(0.0f, fminf(1.0f, (ValueNoise2D(x * 0.04f + seed * 4.0f, y * 0.04f + seed * 4.0f) - 0.3f) * 1.5f));
        float a_theme = DeterministicHash((int)(seed * 789.0f), 123), r1, g1, b1, r2, g2, b2;
        if (a_theme > 0.7f) { r1 = 0.5f; g1 = 0.45f; b1 = 0.42f; r2 = 0.75f; g2 = 0.4f; b2 = 0.35f; }
        else if (a_theme > 0.4f) { r1 = 0.55f; g1 = 0.55f; b1 = 0.6f; r2 = 0.45f; g2 = 0.5f; b2 = 0.75f; }
        else { r1 = 0.35f; g1 = 0.35f; b1 = 0.38f; r2 = 0.55f; g2 = 0.55f; b2 = 0.58f; }
        float tr = r1 * (1.0f - mix_val) + r2 * mix_val, tg = g1 * (1.0f - mix_val) + g2 * mix_val, tb = b1 * (1.0f - mix_val) + b2 * mix_val;
        float base_val = 30.0f + ValueNoise2D(x * 0.05f + seed, y * 0.05f + seed) * 35.0f + shape_n * 15.0f;
        float darken = (0.4f + 0.6f * powf(fmaxf(0.0f, fminf(1.0f, cracks1 / 0.4f)), 0.5f)) * (0.7f + 0.3f * powf(fmaxf(0.0f, fminf(1.0f, cracks2 / 0.2f)), 0.5f));
        Uint8 r = (Uint8)fminf(255, base_val * tr * darken), g = (Uint8)fminf(255, base_val * tg * darken), b = (Uint8)fminf(255, base_val * tb * darken);
        pixels[y * size + x] = (255 << 24) | (b << 16) | (g << 8) | r;
      } else {
        float dust_noise = PerlinNoise2D(cosf(angle) * 0.8f + seed + 100, sinf(angle) * 0.8f + seed + 100);
        float dust_outer = base_radius * (1.5f + dust_noise * 3.0f);
        if (dist <= dust_outer) {
          float dust_t = (dist - distorted_radius) / (dust_outer - distorted_radius);
          float detail_n = PerlinNoise2D(x * 0.02f + seed + 500, y * 0.02f + seed);
          float edge_falloff = fminf(1.0f, fminf(fminf(x, (size - 1) - x), fminf(y, (size - 1) - y)) / 30.0f);
          float alpha_f = powf(1.0f - dust_t, 2.5f) * detail_n * edge_falloff;
          if (alpha_f > 0.01f) { Uint8 val = (Uint8)(35 + detail_n * 25); pixels[y * size + x] = ((Uint8)(alpha_f * 100) << 24) | (val << 16) | (val << 8) | val; }
        }
      }
    }
  }
}

static void DrawExplosionPuffToBuffer(Uint32 *pixels, int size, float seed) {
  int center = size / 2; float max_rad = size * 0.45f;
  for (int y = 0; y < size; y++) {
    for (int x = 0; x < size; x++) {
      float dx = (float)(x - center), dy = (float)(y - center);
      float dist = sqrtf(dx * dx + dy * dy); if (dist > max_rad) continue;
      float noise = (PerlinNoise2D(x * 0.05f + seed, y * 0.05f + seed) * 0.7f + PerlinNoise2D(x * 0.1f + seed * 2, y * 0.1f + seed * 2) * 0.3f);
      float alpha_f = powf(1.0f - (dist / max_rad), 1.5f) * noise;
      if (alpha_f > 0.05f) { Uint8 val = (Uint8)(100 + noise * 100); pixels[y * size + x] = ((Uint8)(alpha_f * 255) << 24) | (val << 16) | (val << 8) | val; }
    }
  }
}

static void DrawMothershipHullToBuffer(Uint32 *pixels, int size) {
  int center = size / 2;
  for (int y = 0; y < size; y++) {
    for (int x = 0; x < size; x++) {
      float dx = (float)(x - center), dy = (float)(y - center);
      float dist = sqrtf(dx * dx + dy * dy);
      float angle = atan2f(dy, dx);
      float noise = PerlinNoise2D(cosf(angle) * 0.2f, sinf(angle) * 0.2f) * 0.7f + PerlinNoise2D(cosf(angle * 3.0f), sinf(angle * 3.0f)) * 0.3f;
      float radius = (size * 0.25f) * (0.7f + noise * 0.3f);
      if (dist <= radius) {
        float intensity = 1.0f - (dist / radius);
        float color_t = (PerlinNoise2D(x * 0.005f, y * 0.005f) + PerlinNoise2D(y * 0.005f, -x * 0.005f)) * 0.5f;
        float cracks = VoronoiCracks2D(x * 0.02f, y * 0.02f);
        Uint8 r_v, g_v, b_v; GetGreenBlueGradient(color_t, &r_v, &g_v, &b_v);
        float shading = (0.4f + 0.6f * powf(intensity, 0.5f)) * (0.7f + 0.3f * fmaxf(0, fminf(1, cracks * 5.0f)));
        if (cracks < 0.05f) shading += (0.05f - cracks) * 4.0f;
        pixels[y * size + x] = (255 << 24) | ((Uint8)fminf(255, b_v * shading) << 16) | ((Uint8)fminf(255, g_v * shading) << 8) | (Uint8)fminf(255, r_v * shading);
      }
    }
  }
}

static void DrawMothershipArmToBuffer(Uint32 *pixels, int size) {
  int center = size / 2;
  for (int y = 0; y < size; y++) {
    for (int x = 0; x < size; x++) {
      float dx = (float)(x - center), dy = (float)(center - y);
      float dist = sqrtf(dx * dx + dy * dy);
      float angle = atan2f(dy, dx);
      float diff = angle - 1.5708f;
      float noise = PerlinNoise2D(x * 0.05f, y * 0.05f);
      float beam_width = powf(fmaxf(0, cosf(diff)), 128.0f) * (0.8f + noise * 0.4f);
      float radius = (size * 0.48f) * beam_width;
      if (dist <= radius && dy > 0) {
        float intensity = 1.0f - (dist / (size * 0.48f));
        Uint8 r_v, g_v, b_v; GetGreenBlueGradient(0.3f, &r_v, &g_v, &b_v); // Static high-contrast color
        float shading = 0.6f + 0.4f * intensity;
        pixels[y * size + x] = (255 << 24) | ((Uint8)fminf(255, b_v * shading) << 16) | ((Uint8)fminf(255, g_v * shading) << 8) | (Uint8)fminf(255, r_v * shading);
      }
    }
  }
}

void Renderer_GenerateAssetStep(AppState *s) {
  int total_assets = PLANET_COUNT + GALAXY_COUNT + ASTEROID_TYPE_COUNT + 3;
  if (s->assets_generated >= total_assets) { s->is_loading = false; return; }
  if (s->assets_generated < PLANET_COUNT) {
    int sz = 1024; Uint32 *p = SDL_malloc(sz * sz * 4); SDL_memset(p, 0, sz * sz * 4);
    DrawPlanetToBuffer(p, sz, (float)s->assets_generated * 567.89f);
    s->planet_textures[s->assets_generated] = SDL_CreateTexture(s->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, sz, sz);
    SDL_SetTextureBlendMode(s->planet_textures[s->assets_generated], SDL_BLENDMODE_BLEND);
    SDL_UpdateTexture(s->planet_textures[s->assets_generated], NULL, p, sz * 4); SDL_free(p);
  } else if (s->assets_generated < PLANET_COUNT + GALAXY_COUNT) {
    int g_idx = s->assets_generated - PLANET_COUNT, sz = 1024; Uint32 *p = SDL_malloc(sz * sz * 4); SDL_memset(p, 0, sz * sz * 4);
    DrawGalaxyToBuffer(p, sz, (float)g_idx * 123.45f + 99.0f);
    s->galaxy_textures[g_idx] = SDL_CreateTexture(s->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, sz, sz);
    SDL_SetTextureBlendMode(s->galaxy_textures[g_idx], SDL_BLENDMODE_BLEND);
    SDL_UpdateTexture(s->galaxy_textures[g_idx], NULL, p, sz * 4); SDL_free(p);
  } else if (s->assets_generated < PLANET_COUNT + GALAXY_COUNT + ASTEROID_TYPE_COUNT) {
    int a_idx = s->assets_generated - (PLANET_COUNT + GALAXY_COUNT), sz = 512; Uint32 *p = SDL_malloc(sz * sz * 4); SDL_memset(p, 0, sz * sz * 4);
    DrawAsteroidToBuffer(p, sz, (float)a_idx * 432.1f + 11.0f);
    s->asteroid_textures[a_idx] = SDL_CreateTexture(s->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, sz, sz);
    SDL_SetTextureBlendMode(s->asteroid_textures[a_idx], SDL_BLENDMODE_BLEND);
    SDL_UpdateTexture(s->asteroid_textures[a_idx], NULL, p, sz * 4); SDL_free(p);
  } else if (s->assets_generated == PLANET_COUNT + GALAXY_COUNT + ASTEROID_TYPE_COUNT) {
    int sz = 256; Uint32 *p = SDL_malloc(sz * sz * 4); SDL_memset(p, 0, sz * sz * 4);
    DrawExplosionPuffToBuffer(p, sz, 777.7f);
    s->explosion_puff_texture = SDL_CreateTexture(s->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, sz, sz);
    SDL_SetTextureBlendMode(s->explosion_puff_texture, SDL_BLENDMODE_BLEND);
    SDL_UpdateTexture(s->explosion_puff_texture, NULL, p, sz * 4); SDL_free(p);
  } else if (s->assets_generated == total_assets - 2) {
    int sz = 1024; Uint32 *p = SDL_malloc(sz * sz * 4); SDL_memset(p, 0, sz * sz * 4);
    DrawMothershipHullToBuffer(p, sz);
    s->mothership_hull_texture = SDL_CreateTexture(s->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, sz, sz);
    SDL_SetTextureBlendMode(s->mothership_hull_texture, SDL_BLENDMODE_BLEND);
    SDL_UpdateTexture(s->mothership_hull_texture, NULL, p, sz * 4); SDL_free(p);
  } else {
    int sz = 1024; Uint32 *p = SDL_malloc(sz * sz * 4); SDL_memset(p, 0, sz * sz * 4);
    DrawMothershipArmToBuffer(p, sz);
    s->mothership_arm_texture = SDL_CreateTexture(s->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, sz, sz);
    SDL_SetTextureBlendMode(s->mothership_arm_texture, SDL_BLENDMODE_BLEND);
    SDL_UpdateTexture(s->mothership_arm_texture, NULL, p, sz * 4); SDL_free(p);
  }
  s->assets_generated++;
  if (s->assets_generated >= total_assets) s->is_loading = false;
}

void Renderer_DrawLoading(AppState *s) {
  int win_w, win_h; SDL_GetRenderLogicalPresentation(s->renderer, &win_w, &win_h, NULL);
  if (win_w == 0 || win_h == 0) SDL_GetRenderOutputSize(s->renderer, &win_w, &win_h);
  SDL_SetRenderDrawColor(s->renderer, 0, 0, 0, 255); SDL_RenderClear(s->renderer);
  float progress = (float)s->assets_generated / (float)(PLANET_COUNT + GALAXY_COUNT + ASTEROID_TYPE_COUNT + 2);
  float bar_w = 400.0f, bar_h = 20.0f, x = (win_w - bar_w) / 2.0f, y = (win_h - bar_h) / 2.0f;
  SDL_SetRenderScale(s->renderer, 4.0f, 4.0f);
  SDL_SetRenderDrawColor(s->renderer, 255, 255, 255, 255); SDL_RenderDebugText(s->renderer, (win_w / 8.0f) - 36, (y / 4.0f) - 30, "Asteroidz");
  SDL_SetRenderScale(s->renderer, 1.0f, 1.0f);
  SDL_SetRenderDrawColor(s->renderer, 200, 200, 200, 255); SDL_RenderDebugText(s->renderer, x, y - 25, "Loading ...");
  SDL_SetRenderDrawColor(s->renderer, 50, 50, 50, 255); SDL_RenderFillRect(s->renderer, &(SDL_FRect){x, y, bar_w, bar_h});
  SDL_SetRenderDrawColor(s->renderer, 100, 200, 255, 255); SDL_RenderFillRect(s->renderer, &(SDL_FRect){x, y, bar_w * progress, bar_h});
  SDL_RenderPresent(s->renderer);
}

void Renderer_DrawLauncher(AppState *s) {
    int w, h; SDL_GetRenderLogicalPresentation(s->renderer, &w, &h, NULL);
    if (w == 0 || h == 0) SDL_GetRenderOutputSize(s->renderer, &w, &h);
    SDL_SetRenderDrawColor(s->renderer, 20, 20, 30, 255); SDL_RenderClear(s->renderer);
    float cx = w / 2.0f, cy = h / 2.0f;
    SDL_SetRenderScale(s->renderer, 4.0f, 4.0f); SDL_SetRenderDrawColor(s->renderer, 255, 255, 255, 255); SDL_RenderDebugText(s->renderer, (cx / 4.0f) - 36, (cy / 4.0f) - 40, "Asteroidz");
    SDL_SetRenderScale(s->renderer, 1.0f, 1.0f);
    SDL_FRect res_rect = {cx - 150, cy - 20, 300, 40}; SDL_SetRenderDrawColor(s->renderer, s->launcher.res_hovered ? 60 : 40, 60, 80, 255); SDL_RenderFillRect(s->renderer, &res_rect);
    SDL_SetRenderDrawColor(s->renderer, 255, 255, 255, 255); const char* res_text = s->launcher.selected_res_index == 0 ? "1280 x 720" : "1920 x 1080"; SDL_RenderDebugText(s->renderer, cx - (SDL_strlen(res_text) * 4), cy - 20 + (40 - 8) / 2.0f, res_text);
    SDL_FRect fs_rect = {cx - 150, cy + 40, 300, 40}; SDL_SetRenderDrawColor(s->renderer, s->launcher.fs_hovered ? 60 : 40, 60, 80, 255); SDL_RenderFillRect(s->renderer, &fs_rect);
    SDL_SetRenderDrawColor(s->renderer, 255, 255, 255, 255); const char* fs_text = s->launcher.fullscreen ? "Fullscreen: ON" : "Fullscreen: OFF"; SDL_RenderDebugText(s->renderer, cx - (SDL_strlen(fs_text) * 4), cy + 40 + (40 - 8) / 2.0f, fs_text);
    SDL_FRect start_rect = {cx - 150, cy + 120, 300, 50}; SDL_SetRenderDrawColor(s->renderer, s->launcher.start_hovered ? 80 : 50, 180, 80, 255); SDL_RenderFillRect(s->renderer, &start_rect);
    SDL_SetRenderDrawColor(s->renderer, 255, 255, 255, 255); SDL_RenderDebugText(s->renderer, cx - (SDL_strlen("START GAME") * 4), cy + 120 + (50 - 8) / 2.0f, "START GAME");
    SDL_RenderPresent(s->renderer);
}

static int SDLCALL BackgroundGenerationThread(void *data) {
  AppState *s = (AppState *)data;
  while (SDL_GetAtomicInt(&s->bg_should_quit) == 0) {
    if (SDL_GetAtomicInt(&s->bg_request_update) == 1) {
      SDL_LockMutex(s->bg_mutex); Vec2 cam_pos = s->bg_target_cam_pos; float zoom = s->bg_target_zoom, time = s->bg_target_time; SDL_UnlockMutex(s->bg_mutex);
      for (int i = 0; i < s->bg_w * s->bg_h; i++) {
        int x = i % s->bg_w, y = i / s->bg_w; float wx = cam_pos.x + (x * BG_SCALE_FACTOR) / zoom, wy = cam_pos.y + (y * BG_SCALE_FACTOR) / zoom;
        float n = ValueNoise2D(wx * 0.0002f + time * 0.05f, wy * 0.0002f + time * 0.03f) * 0.6f + ValueNoise2D(wx * 0.001f + time * 0.15f, wy * 0.001f + time * 0.1f) * 0.3f + ValueNoise2D(wx * 0.003f + time * 0.15f, wy * 0.003f + time * 0.1f) * 0.1f;
        float variation = ValueNoise2D(wx * 0.00001f + 12345.0f, wy * 0.00001f + 67890.0f);
        float rf, gf, bf; GetNebulaColor(fminf(1.0f, n), &rf, &gf, &bf); rf *= (0.95f + variation * 0.1f); gf *= (0.95f + (1.0f - variation) * 0.1f);
        float intensity = 0.5f + n * 0.5f; Uint8 r = (Uint8)fminf(rf * intensity, 255.0f), g = (Uint8)fminf(gf * intensity, 255.0f), b = (Uint8)fminf(bf * intensity, 255.0f);
        Uint8 a = (Uint8)(fmaxf(0.0f, fminf(1.0f, (n - 0.1f) / 0.6f)) * 160);
        s->bg_pixel_buffer[i] = (a << 24) | (b << 16) | (g << 8) | r;
      }
      SDL_SetAtomicInt(&s->bg_request_update, 0); SDL_SetAtomicInt(&s->bg_data_ready, 1);
    } else SDL_Delay(10);
  }
  return 0;
}

static int SDLCALL DensityGenerationThread(void *data) {
  AppState *s = (AppState *)data;
  while (SDL_GetAtomicInt(&s->density_should_quit) == 0) {
    if (SDL_GetAtomicInt(&s->density_request_update) == 1) {
      SDL_LockMutex(s->density_mutex); Vec2 cam_pos = s->density_target_cam_pos; SDL_UnlockMutex(s->density_mutex);
      float range = MINIMAP_RANGE, start_x = cam_pos.x - range / 2.0f, start_y = cam_pos.y - range / 2.0f, cell_sz = (float)DENSITY_CELL_SIZE / (float)GRID_DENSITY_SUB_RES;
      for (int i = 0; i < s->density_w * s->density_h; i++) {
          int x = i % s->density_w, y = i / s->density_w; float d = GetAsteroidDensity((Vec2){start_x + (float)x * cell_sz, start_y + (float)y * cell_sz});
          s->density_pixel_buffer[i] = (d > 0.01f) ? (((Uint8)fminf(40.0f, d * 60.0f + 5.0f)) << 24) | ((Uint8)fminf(255.0f, d * 255.0f)) : 0;
      }
      SDL_SetAtomicInt(&s->density_request_update, 0); SDL_SetAtomicInt(&s->density_data_ready, 1);
    } else SDL_Delay(20);
  }
  return 0;
}

static void UpdateDensityMap(AppState *s) {
  if (!s->density_texture) return;
  if (SDL_GetAtomicInt(&s->density_data_ready) == 1) {
    void *pixels; int pitch;
    if (SDL_LockTexture(s->density_texture, NULL, &pixels, &pitch)) {
      for (int i = 0; i < s->density_h; ++i) SDL_memcpy((Uint8 *)pixels + i * pitch, (Uint8 *)s->density_pixel_buffer + i * s->density_w * 4, s->density_w * 4);
      SDL_UnlockTexture(s->density_texture);
      SDL_LockMutex(s->density_mutex); s->density_texture_cam_pos = s->density_target_cam_pos; SDL_UnlockMutex(s->density_mutex);
    }
    SDL_SetAtomicInt(&s->density_data_ready, 0);
  }
  if (SDL_GetAtomicInt(&s->density_request_update) == 0) {
    float cell_sz = (float)DENSITY_CELL_SIZE / (float)GRID_DENSITY_SUB_RES;
    SDL_LockMutex(s->density_mutex); s->density_target_cam_pos.x = floorf(s->camera_pos.x / cell_sz) * cell_sz; s->density_target_cam_pos.y = floorf(s->camera_pos.y / cell_sz) * cell_sz; SDL_UnlockMutex(s->density_mutex);
    SDL_SetAtomicInt(&s->density_request_update, 1);
  }
}

static int SDLCALL RadarGenerationThread(void *data) {
  AppState *s = (AppState *)data;
  while (SDL_GetAtomicInt(&s->radar_should_quit) == 0) {
    if (SDL_GetAtomicInt(&s->radar_request_update) == 1) {
      SDL_LockMutex(s->radar_mutex); Vec2 m_pos = s->radar_mothership_pos; SDL_UnlockMutex(s->radar_mutex);
      int count = 0; float rsq = MOTHERSHIP_RADAR_RANGE * MOTHERSHIP_RADAR_RANGE;
      for (int i = 0; i < MAX_ASTEROIDS; i++) {
          if (!s->asteroids[i].active) continue; if (count >= MAX_RADAR_BLIPS) break;
          float dx = s->asteroids[i].pos.x - m_pos.x, dy = s->asteroids[i].pos.y - m_pos.y;
          if (dx * dx + dy * dy < rsq) { s->radar_blips[count++].pos = s->asteroids[i].pos; }
      }
      SDL_LockMutex(s->radar_mutex); s->radar_blip_count = count; SDL_UnlockMutex(s->radar_mutex);
      SDL_SetAtomicInt(&s->radar_request_update, 0); SDL_SetAtomicInt(&s->radar_data_ready, 1);
    } else SDL_Delay(50);
  }
  return 0;
}

static void UpdateRadar(AppState *s) {
    Vec2 m_pos = {0, 0}; bool found = false;
    for (int i = 0; i < MAX_UNITS; i++) if (s->units[i].active && s->units[i].type == UNIT_MOTHERSHIP) { m_pos = s->units[i].pos; found = true; break; }
    if (found && SDL_GetAtomicInt(&s->radar_request_update) == 0) {
        SDL_LockMutex(s->radar_mutex); s->radar_mothership_pos = m_pos; SDL_UnlockMutex(s->radar_mutex);
        SDL_SetAtomicInt(&s->radar_request_update, 1);
    }
}

static int SDLCALL UnitFXGenerationThread(void *data) {
  AppState *s = (AppState *)data;
  while (SDL_GetAtomicInt(&s->unit_fx_should_quit) == 0) {
    for (int i = 0; i < MAX_UNITS; i++) {
        if (!s->units[i].active) continue;
        Unit *u = &s->units[i];
        int best_l = -1; float bl_dsq = LARGE_CANNON_RANGE * LARGE_CANNON_RANGE;
        int best_s[4] = {-1, -1, -1, -1}; float bs_dsq[4]; for(int c=0; c<4; c++) bs_dsq[c] = SMALL_CANNON_RANGE * SMALL_CANNON_RANGE;
        for (int a = 0; a < MAX_ASTEROIDS; a++) {
            if (!s->asteroids[a].active) continue;
            float dx = s->asteroids[a].pos.x - u->pos.x, dy = s->asteroids[a].pos.y - u->pos.y, dsq = dx * dx + dy * dy;
            if (dsq < bl_dsq) { bl_dsq = dsq; best_l = a; }
            for (int c = 0; c < 4; c++) if (dsq < bs_dsq[c]) { bs_dsq[c] = dsq; best_s[c] = a; break; }
        }
        SDL_LockMutex(s->unit_fx_mutex); u->large_target_idx = best_l; for(int c=0; c<4; c++) u->small_target_idx[c] = best_s[c]; SDL_UnlockMutex(s->unit_fx_mutex);
    }
    SDL_Delay(33); 
  }
  return 0;
}

static void UpdateUnitFX(AppState *s) { (void)s; }

void Renderer_Init(AppState *s) {
  int w, h; SDL_GetRenderOutputSize(s->renderer, &w, &h);
  s->bg_w = w / BG_SCALE_FACTOR; s->bg_h = h / BG_SCALE_FACTOR; s->bg_pixel_buffer = SDL_calloc(s->bg_w * s->bg_h, 4);
  s->bg_texture = SDL_CreateTexture(s->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, s->bg_w, s->bg_h);
  if (s->bg_texture) { SDL_SetTextureScaleMode(s->bg_texture, SDL_SCALEMODE_LINEAR); SDL_SetTextureBlendMode(s->bg_texture, SDL_BLENDMODE_BLEND); }
  float cell_sz = (float)DENSITY_CELL_SIZE / (float)GRID_DENSITY_SUB_RES;
  s->density_w = (int)(MINIMAP_RANGE / cell_sz); s->density_h = (int)(MINIMAP_RANGE / cell_sz); s->density_pixel_buffer = SDL_calloc(s->density_w * s->density_h, 4);
  s->density_texture = SDL_CreateTexture(s->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, s->density_w, s->density_h);
  if (s->density_texture) { SDL_SetTextureScaleMode(s->density_texture, SDL_SCALEMODE_LINEAR); SDL_SetTextureBlendMode(s->density_texture, SDL_BLENDMODE_BLEND); }
  s->bg_mutex = SDL_CreateMutex(); s->density_mutex = SDL_CreateMutex(); s->radar_mutex = SDL_CreateMutex(); s->unit_fx_mutex = SDL_CreateMutex();
  s->mothership_fx_size = 1024; s->is_loading = true; s->assets_generated = 0;
}

void Renderer_StartBackgroundThreads(AppState *s) {
  SDL_SetAtomicInt(&s->bg_request_update, 1);
  s->bg_thread = SDL_CreateThread(BackgroundGenerationThread, "BG_Gen", s);
  s->density_thread = SDL_CreateThread(DensityGenerationThread, "Density_Gen", s);
  s->radar_thread = SDL_CreateThread(RadarGenerationThread, "Radar_Gen", s);
  s->unit_fx_thread = SDL_CreateThread(UnitFXGenerationThread, "UnitFX_Gen", s);
}

static void UpdateBackground(AppState *s) {
  if (!s->bg_texture) return;
  if (SDL_GetAtomicInt(&s->bg_data_ready) == 1) {
    void *pixels; int pitch;
    if (SDL_LockTexture(s->bg_texture, NULL, &pixels, &pitch)) {
      for (int i = 0; i < s->bg_h; ++i) SDL_memcpy((Uint8 *)pixels + i * pitch, (Uint8 *)s->bg_pixel_buffer + i * s->bg_w * 4, s->bg_w * 4);
      SDL_UnlockTexture(s->bg_texture);
    }
    SDL_SetAtomicInt(&s->bg_data_ready, 0);
  }
  if (SDL_GetAtomicInt(&s->bg_request_update) == 0) {
    SDL_LockMutex(s->bg_mutex); s->bg_target_cam_pos = s->camera_pos; s->bg_target_zoom = s->zoom; s->bg_target_time = s->current_time; SDL_UnlockMutex(s->bg_mutex);
    SDL_SetAtomicInt(&s->bg_request_update, 1);
  }
}

static void StarLayerFn(SDL_Renderer *r, const AppState *s, const LayerCell *cell) {
  if (cell->seed > 0.85f) {
    float jx = DeterministicHash(cell->gx + 7, cell->gy + 3) * (float)cell->cell_size, jy = DeterministicHash(cell->gx + 1, cell->gy + 9) * (float)cell->cell_size;
    float sx = cell->screen_x + (jx + sinf(s->current_time * 0.2f + cell->seed * 10.0f) * 5.0f) * cell->parallax * s->zoom, sy = cell->screen_y + (jy + cosf(s->current_time * 0.15f + cell->seed * 5.0f) * 5.0f) * cell->parallax * s->zoom;
    float sz_s = DeterministicHash(cell->gx + 55, cell->gy + 66); float s_sz = ((sz_s > 0.98f) ? 3.0f : (sz_s > 0.90f ? 2.0f : 1.0f)) * s->zoom;
    if (!IsVisible(sx, sy, s_sz / 2, cell->win_w, cell->win_h)) return;
    float b_s = DeterministicHash(cell->gx + 77, cell->gy + 88), c_s = DeterministicHash(cell->gx + 99, cell->gy + 11); Uint8 val = (Uint8)(100 + b_s * 155), rv = val, gv = val, bv = val;
    if (c_s > 0.90f) { rv = (Uint8)(val * 0.8f); gv = (Uint8)(val * 0.9f); bv = 255; } else if (c_s > 0.80f) { rv = 220; gv = (Uint8)(val * 0.7f); bv = 255; }
    SDL_SetRenderDrawColor(r, rv, gv, bv, 200); if (s_sz <= 1.0f) SDL_RenderPoint(r, sx, sy); else SDL_RenderFillRect(r, &(SDL_FRect){sx - s_sz / 2, sy - s_sz / 2, s_sz, s_sz});
  }
}

static void SystemLayerFn(SDL_Renderer *r, const AppState *s, const LayerCell *cell) {
  Vec2 b_pos; float type_seed, b_radius;
  if (GetCelestialBodyInfo(cell->gx, cell->gy, &b_pos, &type_seed, &b_radius)) {
    Vec2 screen_pos = WorldToScreenParallax(b_pos, cell->parallax, s, cell->win_w, cell->win_h); float sx = screen_pos.x, sy = screen_pos.y, rad = b_radius * s->zoom;
    if (type_seed > 0.95f) { if (IsVisible(sx, sy, rad, cell->win_w, cell->win_h)) SDL_RenderTexture(r, s->galaxy_textures[(int)(DeterministicHash(cell->gx + 9, cell->gy + 2) * GALAXY_COUNT)], NULL, &(SDL_FRect){sx - rad, sy - rad, rad * 2, rad * 2}); }
    else { if (IsVisible(sx, sy, rad, cell->win_w, cell->win_h)) { float tsz = rad * 4.5f; SDL_RenderTexture(r, s->planet_textures[(int)(DeterministicHash(cell->gx + 1, cell->gy + 1) * PLANET_COUNT)], NULL, &(SDL_FRect){sx - tsz / 2, sy - tsz / 2, tsz, tsz}); } }
  }
}

static void Renderer_DrawAsteroids(SDL_Renderer *r, const AppState *s, int win_w, int win_h) {
  for (int i = 0; i < MAX_ASTEROIDS; i++) {
    if (!s->asteroids[i].active) continue;
    Vec2 sx_y = WorldToScreenParallax(s->asteroids[i].pos, 1.0f, s, win_w, win_h); float rad = s->asteroids[i].radius * s->zoom;
    if (!IsVisible(sx_y.x, sx_y.y, rad * 1.5f, win_w, win_h)) continue;
    SDL_RenderTextureRotated(r, s->asteroid_textures[s->asteroids[i].tex_idx], NULL, &(SDL_FRect){sx_y.x - rad * 1.5f, sx_y.y - rad * 1.5f, rad * 3.0f, rad * 3.0f}, s->asteroids[i].rotation, NULL, SDL_FLIP_NONE);
    if (s->asteroids[i].targeted) { float hp_pct = s->asteroids[i].health / s->asteroids[i].max_health, bw = rad * 2.0f; SDL_FRect rct = {sx_y.x - rad, sx_y.y + rad * 1.6f, bw, 4.0f}; SDL_SetRenderDrawColor(r, 50, 0, 0, 200); SDL_RenderFillRect(r, &rct); rct.w *= hp_pct; SDL_SetRenderDrawColor(r, 255, 50, 50, 255); SDL_RenderFillRect(r, &rct); }
  }
}

static void Renderer_DrawParticles(SDL_Renderer *r, const AppState *s, int win_w, int win_h) {
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (!s->particles[i].active) continue;
    Vec2 sx_y = WorldToScreenParallax(s->particles[i].pos, 1.0f, s, win_w, win_h); float sz = s->particles[i].size * s->zoom;
    if (!IsVisible(sx_y.x, sx_y.y, sz, win_w, win_h)) continue;
    if (s->particles[i].type == PARTICLE_PUFF && s->explosion_puff_texture) { SDL_SetTextureColorMod(s->explosion_puff_texture, s->particles[i].color.r, s->particles[i].color.g, s->particles[i].color.b); SDL_SetTextureAlphaMod(s->explosion_puff_texture, (Uint8)(s->particles[i].life * 180)); SDL_RenderTexture(r, s->explosion_puff_texture, NULL, &(SDL_FRect){sx_y.x - sz / 2, sx_y.y - sz / 2, sz, sz}); }
    else if (s->particles[i].type == PARTICLE_TRACER) {
        Vec2 tsx_y = WorldToScreenParallax(s->particles[i].target_pos, 1.0f, s, win_w, win_h); SDL_SetRenderDrawColor(r, s->particles[i].color.r, s->particles[i].color.g, s->particles[i].color.b, (Uint8)(s->particles[i].life * 255 * 4.0f));
        float th = s->particles[i].size * s->zoom; if (th <= 1.0f) SDL_RenderLine(r, sx_y.x, sx_y.y, tsx_y.x, tsx_y.y); else for (float o = -th/2.0f; o <= th/2.0f; o += 1.0f) SDL_RenderLine(r, sx_y.x + o, sx_y.y + o, tsx_y.x + o, tsx_y.y + o);
    } else { SDL_SetRenderDrawColor(r, s->particles[i].color.r, s->particles[i].color.g, s->particles[i].color.b, (Uint8)(s->particles[i].life * 255)); SDL_RenderFillRect(r, &(SDL_FRect){sx_y.x - sz / 2, sx_y.y - sz / 2, sz, sz}); }
  }
}

static void DrawGrid(SDL_Renderer *renderer, const AppState *s, int win_w, int win_h) {
  if (s->density_texture) {
      SDL_LockMutex(s->density_mutex); Vec2 tc = s->density_texture_cam_pos; SDL_UnlockMutex(s->density_mutex);
      float range = MINIMAP_RANGE, twx = tc.x - range / 2.0f, twy = tc.y - range / 2.0f;
      Vec2 stl = WorldToScreenParallax((Vec2){twx, twy}, 1.0f, s, win_w, win_h); float ss = range * s->zoom;
      SDL_RenderTexture(renderer, s->density_texture, NULL, &(SDL_FRect){stl.x, stl.y, ss, ss});
  }
  SDL_SetRenderDrawColor(renderer, 50, 50, 50, 40); int gs = GRID_SIZE_SMALL, stx = (int)floorf(s->camera_pos.x / gs) * gs, sty = (int)floorf(s->camera_pos.y / gs) * gs;
  for (float x = stx; x < s->camera_pos.x + win_w / s->zoom + gs; x += gs) { Vec2 s1 = WorldToScreenParallax((Vec2){x, 0}, 1.0f, s, win_w, win_h); SDL_RenderLine(renderer, s1.x, 0, s1.x, (float)win_h); }
  for (float y = sty; y < s->camera_pos.y + win_h / s->zoom + gs; y += gs) { Vec2 s1 = WorldToScreenParallax((Vec2){0, y}, 1.0f, s, win_w, win_h); SDL_RenderLine(renderer, 0, s1.y, (float)win_w, s1.y); }
  SDL_SetRenderDrawColor(renderer, 100, 100, 100, 80); int gl = GRID_SIZE_LARGE, slx = (int)floorf(s->camera_pos.x / gl) * gl, sly = (int)floorf(s->camera_pos.y / gl) * gl;
  for (float x = slx; x < s->camera_pos.x + win_w / s->zoom + gl; x += gl) { float sx = (x - s->camera_pos.x) * s->zoom; SDL_RenderLine(renderer, sx, 0, sx, (float)win_h); }
  for (float y = sly; y < s->camera_pos.y + win_h / s->zoom + gl; y += gl) { float sy = (y - s->camera_pos.y) * s->zoom; SDL_RenderLine(renderer, 0, sy, (float)win_w, sy); }
  SDL_SetRenderDrawColor(renderer, 150, 150, 150, 150);
  for (float x = slx; x < s->camera_pos.x + win_w / s->zoom + gl; x += gl) for (float y = sly; y < s->camera_pos.y + win_h / s->zoom + gl; y += gl) {
      float sx = (x - s->camera_pos.x) * s->zoom, sy = (y - s->camera_pos.y) * s->zoom;
      if (sx >= -10 && sx < win_w && sy >= -10 && sy < win_h) { char l[32]; snprintf(l, 32, "(%.0fk,%.0fk)", x / 1000.0f, y / 1000.0f); SDL_RenderDebugText(renderer, sx + 5, sy + 5, l); }
  }
}

static void DrawDebugInfo(SDL_Renderer *renderer, const AppState *s, int win_w) {
  char ct[64]; snprintf(ct, 64, "Cam: %.1f, %.1f (x%.4f)", s->camera_pos.x, s->camera_pos.y, s->zoom);
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderDebugText(renderer, (float)win_w - (SDL_strlen(ct) * 8) - 20, 20, ct);
  char ft[32]; snprintf(ft, 32, "FPS: %.0f", s->current_fps); SDL_RenderDebugText(renderer, 20, 20, ft);
}

static void DrawHUD(SDL_Renderer *r, AppState *s, int ww, int wh) {
    float hp = 0, max_hp = 1; bool found = false;
    for (int i = 0; i < MAX_UNITS; i++) if (s->units[i].active && s->units[i].type == UNIT_MOTHERSHIP) { hp = s->units[i].health; max_hp = s->units[i].max_health; found = true; break; }
    if (!found) return;
    float gx = 20.0f, csz = 55.0f, pad = 5.0f, gh = (3.0f * csz) + (2.0f * pad), gy = wh - gh - 20.0f;
    const char *lb[12] = {"Q", "W", "E", "R", "", "", "", "", "", "", "", ""}, *ac[12] = {"PATROL", "MOVE", "ATTACK", "HOLD", "", "", "", "", "", "", "", ""};
    for (int row = 0; row < 3; row++) for (int col = 0; col < 4; col++) {
            int idx = row * 4 + col; SDL_FRect cell = {gx + col * (csz + pad), gy + row * (csz + pad), csz, csz};
            SDL_SetRenderDrawColor(r, 40, 40, 40, 200); SDL_RenderFillRect(r, &cell); SDL_SetRenderDrawColor(r, 80, 80, 80, 255); SDL_RenderRect(r, &cell);
            if (lb[idx][0] != '\0') { SDL_SetRenderDrawColor(r, 255, 255, 255, 255); SDL_RenderDebugText(r, cell.x + 5, cell.y + 5, lb[idx]); SDL_SetRenderDrawColor(r, 150, 150, 150, 255); SDL_RenderDebugText(r, cell.x + 5, cell.y + 35, ac[idx]); }
    }
    float bw = 235.0f, bh = 15.0f, bx = 20.0f, by = gy - 40.0f;
    SDL_SetRenderDrawColor(r, 20, 20, 40, 180); SDL_RenderFillRect(r, &(SDL_FRect){bx, by, bw, bh});
    float ep = s->energy / INITIAL_ENERGY; SDL_SetRenderDrawColor(r, 100, 150, 255, 255); SDL_RenderFillRect(r, &(SDL_FRect){bx, by, bw * (ep < 0 ? 0 : ep), bh});
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255); SDL_RenderDebugText(r, bx, by - 12, "ENERGY RESERVES");
    by -= 40.0f; SDL_SetRenderDrawColor(r, 20, 40, 20, 180); SDL_RenderFillRect(r, &(SDL_FRect){bx, by, bw, bh});
    float hpp = hp / max_hp; SDL_SetRenderDrawColor(r, 100, 255, 100, 255); SDL_RenderFillRect(r, &(SDL_FRect){bx, by, bw * (hpp < 0 ? 0 : hpp), bh});
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255); SDL_RenderDebugText(r, bx, by - 12, "HULL INTEGRITY");
    float sx = ww / 2.0f, sy = wh - 60.0f, isz = 40.0f; int sc = 0;
    for (int i = 0; i < MAX_UNITS; i++) if (s->unit_selected[i]) sc++;
    if (sc > 0) {
        float tw = sc * (isz + 5.0f), cx = sx - tw / 2.0f;
        for (int i = 0; i < MAX_UNITS; i++) {
            if (!s->unit_selected[i]) continue;
            SDL_FRect ir = {cx, sy, isz, isz}; SDL_SetRenderDrawColor(r, 40, 40, 40, 200); SDL_RenderFillRect(r, &ir); SDL_SetRenderDrawColor(r, 100, 255, 100, 255); SDL_RenderRect(r, &ir);
            SDL_RenderDebugText(r, cx + 5, sy + 15, s->units[i].type == UNIT_MOTHERSHIP ? "M" : "U"); cx += isz + 5.0f;
        }
    }
}

static void DrawMinimap(SDL_Renderer *r, const AppState *s, int win_w, int win_h) {
  float mm_x = (float)win_w - MINIMAP_SIZE - MINIMAP_MARGIN, mm_y = (float)win_h - MINIMAP_SIZE - MINIMAP_MARGIN, wmm = MINIMAP_SIZE / MINIMAP_RANGE;
  float cx = s->camera_pos.x + (win_w / 2.0f) / s->zoom, cy = s->camera_pos.y + (win_h / 2.0f) / s->zoom;
  SDL_SetRenderDrawColor(r, 20, 20, 30, 180); SDL_RenderFillRect(r, &(SDL_FRect){mm_x, mm_y, MINIMAP_SIZE, MINIMAP_SIZE}); SDL_SetRenderDrawColor(r, 80, 80, 100, 255); SDL_RenderRect(r, &(SDL_FRect){mm_x, mm_y, MINIMAP_SIZE, MINIMAP_SIZE});
  if (s->density_texture) {
      SDL_LockMutex(s->density_mutex); Vec2 tc = s->density_texture_cam_pos; SDL_UnlockMutex(s->density_mutex);
      float mdx = (tc.x - cx) * wmm, mdy = (tc.y - cy) * wmm, tmx = MINIMAP_SIZE / 2 + mdx, tmy = MINIMAP_SIZE / 2 + mdy, tms = MINIMAP_RANGE * wmm;
      SDL_SetRenderClipRect(r, &(SDL_Rect){(int)mm_x, (int)mm_y, (int)MINIMAP_SIZE, (int)MINIMAP_SIZE}); SDL_RenderTexture(r, s->density_texture, NULL, &(SDL_FRect){mm_x + tmx - tms / 2, mm_y + tmy - tms / 2, tms, tms}); SDL_SetRenderClipRect(r, NULL);
  }
  int cs = SYSTEM_LAYER_CELL_SIZE, rc = (int)(MINIMAP_RANGE / cs) + 1, scx = (int)floorf((cx - MINIMAP_RANGE / 2) / cs), scy = (int)floorf((cy - MINIMAP_RANGE / 2) / cs);
  for (int gy = scy; gy <= scy + rc; gy++) for (int gx = scx; gx <= scx + rc; gx++) {
      Vec2 bp; float ts, br; if (GetCelestialBodyInfo(gx, gy, &bp, &ts, &br)) {
        float dx = (bp.x - cx), dy = (bp.y - cy);
        if (fabsf(dx) < MINIMAP_RANGE / 2 && fabsf(dy) < MINIMAP_RANGE / 2) {
            float px = mm_x + MINIMAP_SIZE / 2 + dx * wmm, py = mm_y + MINIMAP_SIZE / 2 + dy * wmm, ds = (br > MINIMAP_LARGE_BODY_THRESHOLD) ? 6 : 4;
            SDL_SetRenderDrawColor(r, ts > 0.95f ? 200 : 100, ts > 0.95f ? 150 : 200, 255, 255); SDL_RenderFillRect(r, &(SDL_FRect){px - ds / 2, py - ds / 2, ds, ds});
        }
      }
  }
  for (int i = 0; i < MAX_UNITS; i++) if (s->units[i].active) {
      float dx = (s->units[i].pos.x - cx), dy = (s->units[i].pos.y - cy);
      if (fabsf(dx) < MINIMAP_RANGE / 2 && fabsf(dy) < MINIMAP_RANGE / 2) {
          float px = mm_x + MINIMAP_SIZE / 2 + dx * wmm, py = mm_y + MINIMAP_SIZE / 2 + dy * wmm;
          if (s->units[i].type == UNIT_MOTHERSHIP) { float rpx = MOTHERSHIP_RADAR_RANGE * wmm; SDL_SetRenderDrawColor(r, 0, 255, 0, 40); SDL_RenderRect(r, &(SDL_FRect){px - rpx, py - rpx, rpx * 2, rpx * 2}); SDL_SetRenderDrawColor(r, 100, 255, 100, 255); SDL_RenderFillRect(r, &(SDL_FRect){px - 4, py - 4, 8, 8}); }
      }
  }
  SDL_LockMutex(s->radar_mutex); int bc = s->radar_blip_count; SDL_SetRenderDrawColor(r, 0, 255, 0, 180);
  for (int i = 0; i < bc; i++) {
      float dx = s->radar_blips[i].pos.x - cx, dy = s->radar_blips[i].pos.y - cy;
      if (fabsf(dx) < MINIMAP_RANGE / 2 && fabsf(dy) < MINIMAP_RANGE / 2) SDL_RenderPoint(r, mm_x + MINIMAP_SIZE / 2 + dx * wmm, mm_y + MINIMAP_SIZE / 2 + dy * wmm);
  }
  SDL_UnlockMutex(s->radar_mutex);
  float vw = ((float)win_w / s->zoom) * wmm, vh = ((float)win_h / s->zoom) * wmm;
  SDL_SetRenderDrawColor(r, 255, 255, 255, 255); SDL_RenderRect(r, &(SDL_FRect){mm_x + (MINIMAP_SIZE - vw) / 2, mm_y + (MINIMAP_SIZE - vh) / 2, vw, vh});
}

// Wobbly mesh rendering helper using SDL_RenderGeometry.
static void DrawWobblyTexture(SDL_Renderer *r, SDL_Texture *tex, SDL_FRect dst, float rotation, float time, float intensity) {
    const int G = 6; // 6x6 cells
    SDL_Vertex v[(G+1)*(G+1)];
    int indices[G*G*6];
    
    float angle = rotation * (M_PI / 180.0f);
    float cos_a = cosf(angle);
    float sin_a = sinf(angle);

    for (int j = 0; j <= G; j++) {
        for (int i = 0; i <= G; i++) {
            float u = (float)i / G;
            float v_coord = (float)j / G;
            
            // Local coordinates (-0.5 to 0.5)
            float lx = u - 0.5f;
            float ly = v_coord - 0.5f;
            
            // Wobble logic
            float wobble_x = sinf(time * 2.0f + ly * 5.0f) * intensity * 0.05f;
            float wobble_y = cosf(time * 1.5f + lx * 5.0f) * intensity * 0.05f;
            lx += wobble_x;
            ly += wobble_y;

            // Rotate and Scale
            float rx = (lx * cos_a - ly * sin_a) * dst.w;
            float ry = (lx * sin_a + ly * cos_a) * dst.h;

            int idx = j * (G + 1) + i;
            v[idx].position = (SDL_FPoint){ (dst.x + dst.w/2.0f) + rx, (dst.y + dst.h/2.0f) + ry };
            v[idx].tex_coord = (SDL_FPoint){ u, v_coord };
            v[idx].color = (SDL_FColor){ 1.0f, 1.0f, 1.0f, 1.0f };
        }
    }

    int k = 0;
    for (int j = 0; j < G; j++) {
        for (int i = 0; i < G; i++) {
            int r1 = j * (G + 1) + i;
            int r2 = (j + 1) * (G + 1) + i;
            indices[k++] = r1; indices[k++] = r1 + 1; indices[k++] = r2;
            indices[k++] = r1 + 1; indices[k++] = r2 + 1; indices[k++] = r2;
        }
    }
    SDL_RenderGeometry(r, tex, v, (G+1)*(G+1), indices, G*G*6);
}

static void Renderer_DrawUnits(SDL_Renderer *r, const AppState *s, int win_w, int win_h) {
  for (int i = 0; i < MAX_UNITS; i++) {
    if (!s->units[i].active) continue;
    Vec2 sx_y = WorldToScreenParallax(s->units[i].pos, 1.0f, s, win_w, win_h); float rad = s->units[i].radius * s->zoom;
        if (s->units[i].type == UNIT_MOTHERSHIP && s->mothership_hull_texture) {
            float dr = rad * 2.6f; if (!IsVisible(sx_y.x, sx_y.y, dr, win_w, win_h)) continue;
    
            // Draw Hull with Wobble
            DrawWobblyTexture(r, s->mothership_hull_texture, 
                (SDL_FRect){sx_y.x - dr, sx_y.y - dr, dr * 2, dr * 2},
                s->units[i].rotation, s->current_time, 1.0f);
            
            // Draw 5 Arms with Wobble
            int ti[5]; ti[0] = s->units[i].large_target_idx; for(int c=0; c<4; c++) ti[c+1] = s->units[i].small_target_idx[c];
    
            for (int k = 0; k < 5; k++) if (ti[k] != -1) {
                float dx = s->asteroids[ti[k]].pos.x - s->units[i].pos.x, dy = s->asteroids[ti[k]].pos.y - s->units[i].pos.y, ang = atan2f(dy, dx) * 180.0f / 3.14159f + 90.0f;
                for(int p=0; p<k; p++) if (ti[p] == ti[k]) ang += 15.0f;
    
                DrawWobblyTexture(r, s->mothership_arm_texture, 
                    (SDL_FRect){sx_y.x - dr, sx_y.y - dr, dr * 2, dr * 2},
                    ang, s->current_time, 0.6f);
            }
            SDL_SetRenderDrawColor(r, 255, 255, 255, 50);
            if (s->units[i].large_target_idx != -1) { Vec2 tp = s->asteroids[s->units[i].large_target_idx].pos, tsx = WorldToScreenParallax(tp, 1.0f, s, win_w, win_h); SDL_RenderLine(r, sx_y.x, sx_y.y, tsx.x, tsx.y); }
            for (int c = 0; c < 4; c++) if (s->units[i].small_target_idx[c] != -1) { Vec2 tp = s->asteroids[s->units[i].small_target_idx[c]].pos, tsx = WorldToScreenParallax(tp, 1.0f, s, win_w, win_h); SDL_RenderLine(r, sx_y.x, sx_y.y, tsx.x, tsx.y); }
        }    if (s->selected_unit_idx == i) {
        SDL_SetRenderDrawColor(r, 255, 255, 255, 150); SDL_RenderRect(r, &(SDL_FRect){sx_y.x - rad - 5, sx_y.y - rad - 5, rad * 2 + 10, rad * 2 + 10});
        if (s->units[i].has_target) {
            Vec2 lp = sx_y; for (int q = s->units[i].command_current_idx; q < s->units[i].command_count; q++) {
                Vec2 wt = s->units[i].command_queue[q].pos, tsx = WorldToScreenParallax(wt, 1.0f, s, win_w, win_h); CommandType ct = s->units[i].command_queue[q].type;
                SDL_SetRenderDrawColor(r, ct == CMD_PATROL ? 100 : (ct == CMD_ATTACK ? 255 : 100), ct == CMD_PATROL ? 100 : (ct == CMD_ATTACK ? 100 : 255), ct == CMD_PATROL ? 255 : (ct == CMD_ATTACK ? 100 : 100), 150);
                SDL_RenderLine(r, lp.x, lp.y, tsx.x, tsx.y); float dx = wt.x - s->units[i].pos.x, dy = wt.y - s->units[i].pos.y, d = sqrtf(dx * dx + dy * dy);
                if (d > 0.1f) { Vec2 dir = {dx / d, dy / d}; float ranges[2] = {SMALL_CANNON_RANGE, LARGE_CANNON_RANGE}; for (int ri = 0; ri < 2; ri++) if (d > ranges[ri]) { Vec2 rw = {s->units[i].pos.x + dir.x * ranges[ri], s->units[i].pos.y + dir.y * ranges[ri]}, rs = WorldToScreenParallax(rw, 1.0f, s, win_w, win_h); SDL_RenderRect(r, &(SDL_FRect){rs.x - 4, rs.y - 4, 8, 8}); } }
                lp = tsx; SDL_RenderRect(r, &(SDL_FRect){tsx.x - 3, tsx.y - 3, 6, 6});
            }
        }
    }
  }
}

void Renderer_Draw(AppState *s) {
  int ww, wh; SDL_GetRenderLogicalPresentation(s->renderer, &ww, &wh, NULL); if (ww == 0 || wh == 0) SDL_GetRenderOutputSize(s->renderer, &ww, &wh);
  if (s->state == STATE_GAMEOVER) {
      SDL_SetRenderDrawColor(s->renderer, 50, 0, 0, 255); SDL_RenderClear(s->renderer); SDL_SetRenderDrawColor(s->renderer, 255, 255, 255, 255); SDL_SetRenderScale(s->renderer, 4.0f, 4.0f); SDL_RenderDebugText(s->renderer, (ww / 8.0f) - 40, (wh / 8.0f) - 10, "GAME OVER");
      SDL_SetRenderScale(s->renderer, 1.0f, 1.0f); SDL_RenderDebugText(s->renderer, (ww / 2.0f) - 80, (wh / 2.0f) + 40, "The Mothership has been destroyed."); SDL_RenderDebugText(s->renderer, (ww / 2.0f) - 60, (wh / 2.0f) + 60, "Press ESC to quit."); SDL_RenderPresent(s->renderer); return;
  }
  SDL_SetRenderDrawColor(s->renderer, 0, 0, 0, 255); SDL_RenderClear(s->renderer); UpdateBackground(s); UpdateDensityMap(s); UpdateRadar(s); UpdateUnitFX(s);
  if (s->bg_texture) SDL_RenderTexture(s->renderer, s->bg_texture, NULL, NULL);
  DrawParallaxLayer(s->renderer, s, ww, wh, STAR_LAYER_CELL_SIZE, STAR_LAYER_PARALLAX, 0, StarLayerFn); DrawParallaxLayer(s->renderer, s, ww, wh, SYSTEM_LAYER_CELL_SIZE, SYSTEM_LAYER_PARALLAX, 1000, SystemLayerFn);
  if (s->show_grid) DrawGrid(s->renderer, s, ww, wh);
  Renderer_DrawUnits(s->renderer, s, ww, wh); Renderer_DrawAsteroids(s->renderer, s, ww, wh); Renderer_DrawParticles(s->renderer, s, ww, wh);
  if (s->box_active) { float x1 = fminf(s->box_start.x, s->box_current.x), y1 = fminf(s->box_start.y, s->box_current.y), w = fabsf(s->box_start.x - s->box_current.x), h = fabsf(s->box_start.y - s->box_current.y); SDL_SetRenderDrawColor(s->renderer, 0, 255, 0, 50); SDL_RenderFillRect(s->renderer, &(SDL_FRect){x1, y1, w, h}); SDL_SetRenderDrawColor(s->renderer, 0, 255, 0, 200); SDL_RenderRect(s->renderer, &(SDL_FRect){x1, y1, w, h}); }
  DrawDebugInfo(s->renderer, s, ww); DrawMinimap(s->renderer, s, ww, wh); DrawHUD(s->renderer, s, ww, wh); SDL_RenderPresent(s->renderer);
}