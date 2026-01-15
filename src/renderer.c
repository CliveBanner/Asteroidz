#include "renderer.h"
#include "constants.h"
#include "game.h"
#include "assets.h"
#include "ui.h"
#include "workers.h"
#include <math.h>
#include <stdio.h>

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
  float zoom = s->camera.zoom;
  float cx = s->camera.pos.x + (sw / 2.0f) / zoom;
  float cy = s->camera.pos.y + (sh / 2.0f) / zoom;
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
  float zoom = s->camera.zoom;
  float cx = s->camera.pos.x + (sw / 2.0f) / zoom;
  float cy = s->camera.pos.y + (sh / 2.0f) / zoom;
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

void Renderer_Init(AppState *s) {
  int w, h; SDL_GetRenderOutputSize(s->renderer, &w, &h);
  s->textures.bg_w = w / BG_SCALE_FACTOR; s->textures.bg_h = h / BG_SCALE_FACTOR; s->threads.bg_pixel_buffer = SDL_calloc(s->textures.bg_w * s->textures.bg_h, 4);
  s->textures.bg_texture = SDL_CreateTexture(s->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, s->textures.bg_w, s->textures.bg_h);
  if (s->textures.bg_texture) { SDL_SetTextureScaleMode(s->textures.bg_texture, SDL_SCALEMODE_LINEAR); SDL_SetTextureBlendMode(s->textures.bg_texture, SDL_BLENDMODE_BLEND); }
  float cell_sz = (float)DENSITY_CELL_SIZE / (float)GRID_DENSITY_SUB_RES;
  s->threads.density_w = (int)(MINIMAP_RANGE / cell_sz); s->threads.density_h = (int)(MINIMAP_RANGE / cell_sz); s->threads.density_pixel_buffer = SDL_calloc(s->threads.density_w * s->threads.density_h, 4);
  s->textures.density_texture = SDL_CreateTexture(s->renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, s->threads.density_w, s->threads.density_h);
  if (s->textures.density_texture) { SDL_SetTextureScaleMode(s->textures.density_texture, SDL_SCALEMODE_LINEAR); SDL_SetTextureBlendMode(s->textures.density_texture, SDL_BLENDMODE_BLEND); }
  s->threads.bg_mutex = SDL_CreateMutex(); s->threads.density_mutex = SDL_CreateMutex(); s->threads.radar_mutex = SDL_CreateMutex(); s->threads.unit_fx_mutex = SDL_CreateMutex();
  s->textures.mothership_fx_size = MOTHERSHIP_FX_TEXTURE_SIZE; 
  s->threads.mothership_hull_buffer = SDL_calloc(s->textures.mothership_fx_size * s->textures.mothership_fx_size, 4);
  s->assets_generated = 0;
}

static void StarLayerFn(SDL_Renderer *r, const AppState *s, const LayerCell *cell) {
  if (cell->seed > 0.85f) {
    float jx = DeterministicHash(cell->gx + 7, cell->gy + 3) * (cell->cell_size / 2.0f);
    float jy = DeterministicHash(cell->gx + 1, cell->gy + 9) * (cell->cell_size / 2.0f);
    
    float sx = cell->screen_x + jx * cell->parallax * s->camera.zoom;
    float sy = cell->screen_y + jy * cell->parallax * s->camera.zoom;
    
    float twinkle = sinf(s->current_time * 2.5f + cell->seed * 50.0f);
    if (twinkle < -0.4f && cell->seed < 0.93f) return; 

    float sz_s = DeterministicHash(cell->gx + 55, cell->gy + 66);
    int pattern = (sz_s > 0.98f) ? 3 : (sz_s > 0.85f ? 2 : 1);
    
    if (!IsVisible(sx, sy, 5.0f, cell->win_w, cell->win_h)) return;

    float b_s = DeterministicHash(cell->gx + 77, cell->gy + 88), c_s = DeterministicHash(cell->gx + 99, cell->gy + 11);
    Uint8 val = (Uint8)(180 + b_s * 75);
    if (twinkle < 0.2f) val = (Uint8)(val * 0.6f);
    
    Uint8 rv = val, gv = val, bv = val;
    if (c_s > 0.94f) { rv = (Uint8)(val * 0.6f); gv = (Uint8)(val * 0.7f); bv = 255; }
    else if (c_s > 0.88f) { rv = 255; gv = (Uint8)(val * 0.8f); bv = (Uint8)(val * 0.6f); }

    float rx = floorf(sx), ry = floorf(sy);
    float p_sz = (s->camera.zoom > 0.5f) ? 2.0f : 1.0f;

    if (pattern == 1) {
        SDL_SetRenderDrawColor(r, rv, gv, bv, 160);
        SDL_RenderFillRect(r, &(SDL_FRect){rx, ry, p_sz, p_sz});
    } else if (pattern == 2) {
        SDL_SetRenderDrawColor(r, rv, gv, bv, 255);
        SDL_RenderFillRect(r, &(SDL_FRect){rx, ry, p_sz, p_sz});
        SDL_SetRenderDrawColor(r, rv, gv, bv, 140); 
        SDL_RenderFillRect(r, &(SDL_FRect){rx, ry - p_sz, p_sz, p_sz});
        SDL_RenderFillRect(r, &(SDL_FRect){rx, ry + p_sz, p_sz, p_sz});
        SDL_RenderFillRect(r, &(SDL_FRect){rx - p_sz, ry, p_sz, p_sz});
        SDL_RenderFillRect(r, &(SDL_FRect){rx + p_sz, ry, p_sz, p_sz});
    } else {
        SDL_SetRenderDrawColor(r, rv, gv, bv, 255);
        SDL_RenderFillRect(r, &(SDL_FRect){rx, ry, p_sz, p_sz});
        SDL_SetRenderDrawColor(r, rv, gv, bv, 200);
        SDL_RenderFillRect(r, &(SDL_FRect){rx, ry - p_sz, p_sz, p_sz});
        SDL_RenderFillRect(r, &(SDL_FRect){rx, ry + p_sz, p_sz, p_sz});
        SDL_RenderFillRect(r, &(SDL_FRect){rx - p_sz, ry, p_sz, p_sz});
        SDL_RenderFillRect(r, &(SDL_FRect){rx + p_sz, ry, p_sz, p_sz});
        SDL_SetRenderDrawColor(r, rv, gv, bv, 100);
        SDL_RenderFillRect(r, &(SDL_FRect){rx, ry - p_sz * 2, p_sz, p_sz});
        SDL_RenderFillRect(r, &(SDL_FRect){rx, ry + p_sz * 2, p_sz, p_sz});
        SDL_RenderFillRect(r, &(SDL_FRect){rx - p_sz * 2, ry, p_sz, p_sz});
        SDL_RenderFillRect(r, &(SDL_FRect){rx + p_sz * 2, ry, p_sz, p_sz});
    }
  }
}

static void SystemLayerFn(SDL_Renderer *r, const AppState *s, const LayerCell *cell) {
  Vec2 b_pos; float type_seed, b_radius;
  if (GetCelestialBodyInfo(cell->gx, cell->gy, &b_pos, &type_seed, &b_radius)) {
    Vec2 screen_pos = WorldToScreenParallax(b_pos, cell->parallax, s, cell->win_w, cell->win_h); float sx = screen_pos.x, sy = screen_pos.y, rad = b_radius * s->camera.zoom;
    if (type_seed > 0.95f) { if (IsVisible(sx, sy, rad * GALAXY_VISUAL_SCALE, cell->win_w, cell->win_h)) { float tsz = rad * 2.0f * GALAXY_VISUAL_SCALE; SDL_RenderTexture(r, s->textures.galaxy_textures[(int)(DeterministicHash(cell->gx + 9, cell->gy + 2) * GALAXY_COUNT)], NULL, &(SDL_FRect){sx - tsz / 2, sy - tsz / 2, tsz, tsz}); } }
    else { if (IsVisible(sx, sy, rad * PLANET_VISUAL_SCALE, cell->win_w, cell->win_h)) { float tsz = rad * 2.2f * PLANET_VISUAL_SCALE; SDL_RenderTexture(r, s->textures.planet_textures[(int)(DeterministicHash(cell->gx + 1, cell->gy + 1) * PLANET_COUNT)], NULL, &(SDL_FRect){sx - tsz / 2, sy - tsz / 2, tsz, tsz}); } }
  }
}

static void Renderer_DrawAsteroids(SDL_Renderer *r, const AppState *s, int win_w, int win_h) {
  for (int i = 0; i < MAX_ASTEROIDS; i++) {
    if (!s->world.asteroids[i].active) continue;
    Vec2 sx_y = WorldToScreenParallax(s->world.asteroids[i].pos, 1.0f, s, win_w, win_h); float rad = s->world.asteroids[i].radius * s->camera.zoom, v_rad = rad * ASTEROID_VISUAL_SCALE, c_rad = rad * ASTEROID_CORE_SCALE;
    if (!IsVisible(sx_y.x, sx_y.y, v_rad, win_w, win_h)) continue;
    SDL_RenderTextureRotated(r, s->textures.asteroid_textures[s->world.asteroids[i].tex_idx], NULL, &(SDL_FRect){sx_y.x - v_rad, sx_y.y - v_rad, v_rad * 2.0f, v_rad * 2.0f}, s->world.asteroids[i].rotation, NULL, SDL_FLIP_NONE);
    if (s->world.asteroids[i].targeted) { float hp_pct = s->world.asteroids[i].health / s->world.asteroids[i].max_health, bw = c_rad * 1.5f; SDL_FRect rct = {sx_y.x - bw/2, sx_y.y + c_rad + 2.0f, bw, 4.0f}; SDL_SetRenderDrawColor(r, 50, 0, 0, 200); SDL_RenderFillRect(r, &rct); rct.w *= hp_pct; SDL_SetRenderDrawColor(r, 255, 50, 50, 255); SDL_RenderFillRect(r, &rct); }
  }
}

static void DrawGradientCircle(SDL_Renderer *r, float cx, float cy, float radius, SDL_FColor center_color, SDL_FColor edge_color) {
    const int segments = 32;
    SDL_Vertex vertices[segments + 2];
    int indices[segments * 3];
    vertices[0].position = (SDL_FPoint){cx, cy};
    vertices[0].color = center_color;
    for (int i = 0; i <= segments; i++) {
        float ang = i * (SDL_PI_F * 2.0f) / segments;
        vertices[i + 1].position = (SDL_FPoint){cx + cosf(ang) * radius, cy + sinf(ang) * radius};
        vertices[i + 1].color = edge_color;
    }
    for (int i = 0; i < segments; i++) {
        indices[i * 3] = 0;
        indices[i * 3 + 1] = i + 1;
        indices[i * 3 + 2] = i + 2;
    }
    SDL_RenderGeometry(r, NULL, vertices, segments + 2, indices, segments * 3);
}

static void DrawTargetCrosshair(SDL_Renderer *r, float x, float y, float size, SDL_Color color) {
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
    float h = size / 2.0f;
    float gap = size * 0.3f;
    SDL_RenderLine(r, x, y - h, x, y - gap);
    SDL_RenderLine(r, x, y + h, x, y + gap);
    SDL_RenderLine(r, x - h, y, x - gap, y);
    SDL_RenderLine(r, x + h, y, x + gap, y);
}

static void Renderer_DrawParticles(SDL_Renderer *r, const AppState *s, int win_w, int win_h) {
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (!s->world.particles[i].active) continue;
    Vec2 sx_y = WorldToScreenParallax(s->world.particles[i].pos, 1.0f, s, win_w, win_h); float sz = s->world.particles[i].size * s->camera.zoom;
    if (!IsVisible(sx_y.x, sx_y.y, sz, win_w, win_h)) continue;
    
    if (s->world.particles[i].type == PARTICLE_DEBRIS) {
        SDL_SetTextureColorMod(s->textures.debris_textures[s->world.particles[i].tex_idx], 255, 255, 255);
        float alpha = s->world.particles[i].life * s->world.particles[i].life;
        SDL_SetTextureAlphaMod(s->textures.debris_textures[s->world.particles[i].tex_idx], (Uint8)(alpha * 255));
        SDL_RenderTextureRotated(r, s->textures.debris_textures[s->world.particles[i].tex_idx], NULL, &(SDL_FRect){sx_y.x - sz / 2, sx_y.y - sz / 2, sz, sz}, s->world.particles[i].rotation, NULL, SDL_FLIP_NONE);
    }
    else if (s->world.particles[i].type == PARTICLE_SHOCKWAVE) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_ADD);
        float a_f = s->world.particles[i].life * s->world.particles[i].life;
        SDL_FColor center = { 1.0f, 1.0f, 1.0f, 0.0f }; 
        SDL_FColor edge = { 1.0f, 1.0f, 1.0f, a_f * 0.5f };
        DrawGradientCircle(r, sx_y.x, sx_y.y, sz, center, edge);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    }
    else if (s->world.particles[i].type == PARTICLE_PUFF) {
        float a_f = (s->world.particles[i].life * s->world.particles[i].life) * 0.10f; 
        SDL_FColor center = { s->world.particles[i].color.r/255.0f, s->world.particles[i].color.g/255.0f, s->world.particles[i].color.b/255.0f, a_f };
        SDL_FColor edge = { center.r * 0.1f, center.g * 0.1f, center.b * 0.1f, 0.0f };
        DrawGradientCircle(r, sx_y.x, sx_y.y, sz / 2, center, edge);
    }
    else if (s->world.particles[i].type == PARTICLE_GLOW) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_ADD);
        float a_f = fminf(1.0f, s->world.particles[i].life * 2.0f);
        SDL_FColor center = { s->world.particles[i].color.r/255.0f, s->world.particles[i].color.g/255.0f, s->world.particles[i].color.b/255.0f, a_f };
        SDL_FColor edge = { center.r * 0.5f, center.g * 0.5f, center.b * 0.5f, 0.0f };
        DrawGradientCircle(r, sx_y.x, sx_y.y, sz / 2, center, edge);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    }
    else if (s->world.particles[i].type == PARTICLE_TRACER) {
        Vec2 tsx_y = WorldToScreenParallax(s->world.particles[i].target_pos, 1.0f, s, win_w, win_h); 
        float a_f = fminf(1.0f, s->world.particles[i].life); 
        float th = s->world.particles[i].size * s->camera.zoom * LASER_THICKNESS_MULT;
        float dx = tsx_y.x - sx_y.x, dy = tsx_y.y - sx_y.y;
        float len = sqrtf(dx * dx + dy * dy);
        if (len > 0.1f) {
            float nx = -dy / len, ny = dx / len;
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_ADD);
            float glow_th = th * LASER_GLOW_MULT;
            SDL_Vertex vg[4];
            SDL_FColor glow_col = { s->world.particles[i].color.r / 255.0f * 0.3f, s->world.particles[i].color.g / 255.0f * 0.3f, s->world.particles[i].color.b / 255.0f * 0.3f, a_f * 0.4f };
            vg[0].position = (SDL_FPoint){ sx_y.x + nx * glow_th, sx_y.y + ny * glow_th }; vg[0].color = glow_col;
            vg[1].position = (SDL_FPoint){ sx_y.x - nx * glow_th, sx_y.y - ny * glow_th }; vg[1].color = glow_col;
            vg[2].position = (SDL_FPoint){ tsx_y.x + nx * glow_th, tsx_y.y + ny * glow_th }; vg[2].color = glow_col;
            vg[3].position = (SDL_FPoint){ tsx_y.x - nx * glow_th, tsx_y.y - ny * glow_th }; vg[3].color = glow_col;
            int indices[6] = { 0, 1, 2, 1, 2, 3 };
            SDL_RenderGeometry(r, NULL, vg, 4, indices, 6);
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
            float pulse = 1.0f + 0.1f * sinf(s->current_time * 25.0f);
            float cur_th = th * pulse;
            SDL_Vertex vb[6]; 
            SDL_FColor edge_col = { s->world.particles[i].color.r / 255.0f, s->world.particles[i].color.g / 255.0f, s->world.particles[i].color.b / 255.0f, a_f };
            SDL_FColor core_col = { 1.0f, 1.0f, 1.0f, a_f }; 
            float core_th = cur_th * LASER_CORE_THICKNESS_MULT;
            vb[0].position = (SDL_FPoint){ sx_y.x + nx * cur_th, sx_y.y + ny * cur_th }; vb[0].color = edge_col;
            vb[1].position = (SDL_FPoint){ sx_y.x, sx_y.y };                           vb[1].color = core_col;
            vb[2].position = (SDL_FPoint){ sx_y.x - nx * cur_th, sx_y.y - ny * cur_th }; vb[2].color = edge_col;
            vb[3].position = (SDL_FPoint){ tsx_y.x + nx * cur_th, tsx_y.y + ny * cur_th }; vb[3].color = edge_col;
            vb[4].position = (SDL_FPoint){ tsx_y.x, tsx_y.y };                           vb[4].color = core_col;
            vb[5].position = (SDL_FPoint){ tsx_y.x - nx * cur_th, tsx_y.y - ny * cur_th }; vb[5].color = edge_col;
            int b_indices[12] = { 0, 1, 3, 1, 3, 4, 1, 2, 4, 2, 4, 5 };
            SDL_RenderGeometry(r, NULL, vb, 6, b_indices, 12);
            if (th > 5.0f && a_f > 0.8f) { 
                SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
                float flash_r = th * 2.5f;
                SDL_FRect flash_rect = { tsx_y.x - flash_r/2, tsx_y.y - flash_r/2, flash_r, flash_r };
                SDL_RenderFillRect(r, &flash_rect);
            }
        }
    } else { 
        SDL_SetRenderDrawColor(r, s->world.particles[i].color.r, s->world.particles[i].color.g, s->world.particles[i].color.b, (Uint8)(s->world.particles[i].life * 255)); 
        SDL_RenderFillRect(r, &(SDL_FRect){sx_y.x - sz / 2, sx_y.y - sz / 2, sz, sz}); 
    }
  }
}

static void DrawGrid(SDL_Renderer *renderer, const AppState *s, int win_w, int win_h) {
  if (s->textures.density_texture) {
      SDL_LockMutex(s->threads.density_mutex); Vec2 tc = s->threads.density_texture_cam_pos; SDL_UnlockMutex(s->threads.density_mutex);
      float range = MINIMAP_RANGE, twx = tc.x - range / 2.0f, twy = tc.y - range / 2.0f;
      Vec2 stl = WorldToScreenParallax((Vec2){twx, twy}, 1.0f, s, win_w, win_h); float ss = range * s->camera.zoom;
      SDL_RenderTexture(renderer, s->textures.density_texture, NULL, &(SDL_FRect){stl.x, stl.y, ss, ss});
  }
  SDL_SetRenderDrawColor(renderer, 50, 50, 50, 40); int gs = GRID_SIZE_SMALL, stx = (int)floorf(s->camera.pos.x / gs) * gs, sty = (int)floorf(s->camera.pos.y / gs) * gs;
  for (float x = stx; x < s->camera.pos.x + win_w / s->camera.zoom + gs; x += gs) { Vec2 s1 = WorldToScreenParallax((Vec2){x, 0}, 1.0f, s, win_w, win_h); SDL_RenderLine(renderer, s1.x, 0, s1.x, (float)win_h); }
  for (float y = sty; y < s->camera.pos.y + win_h / s->camera.zoom + gs; y += gs) { Vec2 s1 = WorldToScreenParallax((Vec2){0, y}, 1.0f, s, win_w, win_h); SDL_RenderLine(renderer, 0, s1.y, (float)win_w, s1.y); }
  SDL_SetRenderDrawColor(renderer, 100, 100, 100, 80); int gl = GRID_SIZE_LARGE, slx = (int)floorf(s->camera.pos.x / gl) * gl, sly = (int)floorf(s->camera.pos.y / gl) * gl;
  for (float x = slx; x < s->camera.pos.x + win_w / s->camera.zoom + gl; x += gl) { float sx = (x - s->camera.pos.x) * s->camera.zoom; SDL_RenderLine(renderer, sx, 0, sx, (float)win_h); }
  for (float y = sly; y < s->camera.pos.y + win_h / s->camera.zoom + gl; y += gl) { float sy = (y - s->camera.pos.y) * s->camera.zoom; SDL_RenderLine(renderer, 0, sy, (float)win_w, sy); }
  SDL_SetRenderDrawColor(renderer, 150, 150, 150, 150);
  for (float x = slx; x < s->camera.pos.x + win_w / s->camera.zoom + gl; x += gl) for (float y = sly; y < s->camera.pos.y + win_h / s->camera.zoom + gl; y += gl) {
      float sx = (x - s->camera.pos.x) * s->camera.zoom, sy = (y - s->camera.pos.y) * s->camera.zoom;
      if (sx >= -10 && sx < win_w && sy >= -10 && sy < win_h) { char l[32]; snprintf(l, 32, "(%.0fk,%.0fk)", x / 1000.0f, y / 1000.0f); SDL_RenderDebugText(renderer, sx + 5, sy + 5, l); }
  }
}

static void DrawDebugInfo(SDL_Renderer *renderer, const AppState *s, int win_w) {
  char ct[64]; snprintf(ct, 64, "Cam: %.1f, %.1f (x%.4f)", s->camera.pos.x, s->camera.pos.y, s->camera.zoom);
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderDebugText(renderer, (float)win_w - (SDL_strlen(ct) * 8) - 20, 20, ct);
  char ft[32]; snprintf(ft, 32, "FPS: %.0f", s->current_fps); SDL_RenderDebugText(renderer, 20, 20, ft);
}

static void DrawMinimap(SDL_Renderer *r, const AppState *s, int win_w, int win_h) {
  float mm_x = (float)win_w - MINIMAP_SIZE - MINIMAP_MARGIN, mm_y = (float)win_h - MINIMAP_SIZE - MINIMAP_MARGIN, wmm = MINIMAP_SIZE / MINIMAP_RANGE;
  float cx = s->camera.pos.x + (win_w / 2.0f) / s->camera.zoom, cy = s->camera.pos.y + (win_h / 2.0f) / s->camera.zoom;
  SDL_SetRenderDrawColor(r, 20, 20, 30, 180); SDL_RenderFillRect(r, &(SDL_FRect){mm_x, mm_y, MINIMAP_SIZE, MINIMAP_SIZE}); SDL_SetRenderDrawColor(r, 80, 80, 100, 255); SDL_RenderRect(r, &(SDL_FRect){mm_x, mm_y, MINIMAP_SIZE, MINIMAP_SIZE});
  if (s->textures.density_texture) {
      SDL_LockMutex(s->threads.density_mutex); Vec2 tc = s->threads.density_texture_cam_pos; SDL_UnlockMutex(s->threads.density_mutex);
      float mdx = (tc.x - cx) * wmm, mdy = (tc.y - cy) * wmm, tmx = MINIMAP_SIZE / 2 + mdx, tmy = MINIMAP_SIZE / 2 + mdy, tms = MINIMAP_RANGE * wmm;
      SDL_SetRenderClipRect(r, &(SDL_Rect){(int)mm_x, (int)mm_y, (int)MINIMAP_SIZE, (int)MINIMAP_SIZE}); SDL_RenderTexture(r, s->textures.density_texture, NULL, &(SDL_FRect){mm_x + tmx - tms / 2, mm_y + tmy - tms / 2, tms, tms}); SDL_SetRenderClipRect(r, NULL);
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
  for (int i = 0; i < MAX_UNITS; i++) if (s->world.units[i].active) {
      float dx = (s->world.units[i].pos.x - cx), dy = (s->world.units[i].pos.y - cy);
      if (fabsf(dx) < MINIMAP_RANGE / 2 && fabsf(dy) < MINIMAP_RANGE / 2) {
          float px = mm_x + MINIMAP_SIZE / 2 + dx * wmm, py = mm_y + MINIMAP_SIZE / 2 + dy * wmm;
          if (s->world.units[i].type == UNIT_MOTHERSHIP) { float rpx = MOTHERSHIP_RADAR_RANGE * wmm; SDL_SetRenderDrawColor(r, 0, 255, 0, 40); SDL_RenderRect(r, &(SDL_FRect){px - rpx, py - rpx, rpx * 2, rpx * 2}); SDL_SetRenderDrawColor(r, 100, 255, 100, 255); SDL_RenderFillRect(r, &(SDL_FRect){px - 4, py - 4, 8, 8}); }
      }
  }
  SDL_LockMutex(s->threads.radar_mutex); int bc = s->threads.radar_blip_count; SDL_SetRenderDrawColor(r, 0, 255, 0, 180);
  for (int i = 0; i < bc; i++) {
      float dx = s->threads.radar_blips[i].pos.x - cx, dy = s->threads.radar_blips[i].pos.y - cy;
      if (fabsf(dx) < MINIMAP_RANGE / 2 && fabsf(dy) < MINIMAP_RANGE / 2) SDL_RenderPoint(r, mm_x + MINIMAP_SIZE / 2 + dx * wmm, mm_y + MINIMAP_SIZE / 2 + dy * wmm);
  }
  SDL_UnlockMutex(s->threads.radar_mutex);
  float vw = ((float)win_w / s->camera.zoom) * wmm, vh = ((float)win_h / s->camera.zoom) * wmm;
  SDL_SetRenderDrawColor(r, 255, 255, 255, 255); SDL_RenderRect(r, &(SDL_FRect){mm_x + (MINIMAP_SIZE - vw) / 2, mm_y + (MINIMAP_SIZE - vh) / 2, vw, vh});
}

static void DrawTargetRing(SDL_Renderer *r, float x, float y, float radius, SDL_Color color) {
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
    const int segs = 32;
    for(int i=0; i<segs; i++) {
        if (i % 4 >= 2) continue;
        float a1 = i * (SDL_PI_F * 2.0f) / (float)segs, a2 = (i+1) * (SDL_PI_F * 2.0f) / (float)segs;
        SDL_RenderLine(r, x + cosf(a1)*radius, y + sinf(a1)*radius, x + cosf(a2)*radius, y + sinf(a2)*radius);
    }
}

static void Renderer_DrawUnits(SDL_Renderer *r, const AppState *s, int win_w, int win_h) {
  for (int i = 0; i < MAX_UNITS; i++) {
    if (!s->world.units[i].active) continue;
    Vec2 sx_y = WorldToScreenParallax(s->world.units[i].pos, 1.0f, s, win_w, win_h); float rad = s->world.units[i].stats->radius * s->camera.zoom;
    if (s->world.units[i].type == UNIT_MOTHERSHIP) {
      if (!IsVisible(sx_y.x, sx_y.y, rad * MOTHERSHIP_VISUAL_SCALE, win_w, win_h)) continue;
      if (s->world.units[i].large_target_idx != -1) {
          int ti = s->world.units[i].large_target_idx;
          float dx = s->world.asteroids[ti].pos.x - s->world.units[i].pos.x, dy = s->world.asteroids[ti].pos.y - s->world.units[i].pos.y;
          float dist = sqrtf(dx * dx + dy * dy);
          SDL_Color col = (dist <= s->world.units[i].stats->main_cannon_range + s->world.asteroids[ti].radius) ? (SDL_Color){255, 50, 50, 180} : (SDL_Color){100, 100, 100, 80};
          Vec2 tsx = WorldToScreenParallax(s->world.asteroids[ti].pos, 1.0f, s, win_w, win_h);
          float ring_sz = (s->world.asteroids[ti].radius * 0.45f) * s->camera.zoom;
          DrawTargetRing(r, tsx.x, tsx.y, fmaxf(15.0f, ring_sz), col);
      }
      for (int c = 0; c < 4; c++) if (s->world.units[i].small_target_idx[c] != -1) {
          int ti = s->world.units[i].small_target_idx[c];
          float dx = s->world.asteroids[ti].pos.x - s->world.units[i].pos.x, dy = s->world.asteroids[ti].pos.y - s->world.units[i].pos.y;
          float dist = sqrtf(dx * dx + dy * dy);
          SDL_Color col = (dist <= s->world.units[i].stats->small_cannon_range + s->world.asteroids[ti].radius) ? (SDL_Color){255, 100, 100, 150} : (SDL_Color){100, 100, 100, 80};
          Vec2 tsx = WorldToScreenParallax(s->world.asteroids[ti].pos, 1.0f, s, win_w, win_h);
          float ring_sz = (s->world.asteroids[ti].radius * 0.4f) * s->camera.zoom;
          DrawTargetRing(r, tsx.x, tsx.y, fmaxf(10.0f, ring_sz), col);
      }
      if (s->textures.mothership_hull_texture) {
          float dr = rad * MOTHERSHIP_VISUAL_SCALE;
          SDL_RenderTextureRotated(r, s->textures.mothership_hull_texture, NULL, 
              &(SDL_FRect){sx_y.x - dr, sx_y.y - dr, dr * 2, dr * 2},
              s->world.units[i].rotation, NULL, SDL_FLIP_NONE);
      }
      if (s->selection.primary_unit_idx == i) {
          float bw = rad * 1.5f, bh = 4.0f, by = sx_y.y + rad * MOTHERSHIP_VISUAL_SCALE + 5.0f;
          SDL_SetRenderDrawColor(r, 20, 40, 20, 200); SDL_RenderFillRect(r, &(SDL_FRect){sx_y.x - bw/2, by, bw, bh});
          SDL_SetRenderDrawColor(r, 100, 255, 100, 255); SDL_RenderFillRect(r, &(SDL_FRect){sx_y.x - bw/2, by, bw * (s->world.units[i].health / s->world.units[i].stats->max_health), bh});
          by += bh + 2.0f;
          SDL_SetRenderDrawColor(r, 0, 0, 40, 200); SDL_RenderFillRect(r, &(SDL_FRect){sx_y.x - bw/2, by, bw, bh});
          SDL_SetRenderDrawColor(r, 50, 150, 255, 255); SDL_RenderFillRect(r, &(SDL_FRect){sx_y.x - bw/2, by, bw * (s->world.energy / INITIAL_ENERGY), bh});
          by += bh + 2.0f;
          if (s->world.units[i].stats->main_cannon_damage > 0) {
              SDL_SetRenderDrawColor(r, 40, 0, 40, 200); SDL_RenderFillRect(r, &(SDL_FRect){sx_y.x - bw/2, by, bw, bh});
              float cd_pct = s->world.units[i].large_cannon_cooldown / s->world.units[i].stats->main_cannon_cooldown;
              SDL_SetRenderDrawColor(r, 200, 50, 255, 255); SDL_RenderFillRect(r, &(SDL_FRect){sx_y.x - bw/2, by, bw * (1.0f - cd_pct), bh});
          }
      }
    }
    if (s->selection.primary_unit_idx == i) {
        if (s->world.units[i].has_target) {
            Vec2 lp = sx_y;
            float visual_rad_px = s->world.units[i].stats->radius * MOTHERSHIP_VISUAL_SCALE * s->camera.zoom;
            for (int q = s->world.units[i].command_current_idx; q < s->world.units[i].command_count; q++) {
                Vec2 wt = s->world.units[i].command_queue[q].pos;
                Vec2 tsx = WorldToScreenParallax(wt, 1.0f, s, win_w, win_h);
                if (s->world.units[i].command_queue[q].type == CMD_PATROL) SDL_SetRenderDrawColor(r, 100, 100, 255, 180);
                else if (s->world.units[i].command_queue[q].type == CMD_ATTACK_MOVE) SDL_SetRenderDrawColor(r, 255, 100, 100, 180);
                else SDL_SetRenderDrawColor(r, 100, 255, 100, 180);
                if (q == s->world.units[i].command_current_idx) {
                    float dx = tsx.x - lp.x, dy = tsx.y - lp.y;
                    float dist = sqrtf(dx*dx + dy*dy);
                    if (dist > visual_rad_px) {
                        float start_x = lp.x + (dx/dist) * visual_rad_px;
                        float start_y = lp.y + (dy/dist) * visual_rad_px;
                        SDL_RenderLine(r, start_x, start_y, tsx.x, tsx.y);
                    }
                } else SDL_RenderLine(r, lp.x, lp.y, tsx.x, tsx.y);
                lp = tsx;
                SDL_RenderRect(r, &(SDL_FRect){tsx.x - 3, tsx.y - 3, 6, 6});
            }
            if (s->world.units[i].command_count > 0 && s->world.units[i].command_queue[s->world.units[i].command_count - 1].type == CMD_PATROL) {
                int first_patrol = s->world.units[i].command_count - 1;
                while (first_patrol > 0 && s->world.units[i].command_queue[first_patrol - 1].type == CMD_PATROL) first_patrol--;
                if (first_patrol < s->world.units[i].command_count - 1) {
                    Vec2 p1 = WorldToScreenParallax(s->world.units[i].command_queue[s->world.units[i].command_count - 1].pos, 1.0f, s, win_w, win_h);
                    Vec2 p2 = WorldToScreenParallax(s->world.units[i].command_queue[first_patrol].pos, 1.0f, s, win_w, win_h);
                    SDL_SetRenderDrawColor(r, 100, 100, 255, 80);
                    SDL_RenderLine(r, p1.x, p1.y, p2.x, p2.y);
                }
            }
        }
    }
  }
}

void Renderer_Draw(AppState *s) {
  int ww, wh; SDL_GetRenderLogicalPresentation(s->renderer, &ww, &wh, NULL); if (ww == 0 || wh == 0) SDL_GetRenderOutputSize(s->renderer, &ww, &wh);
  if (s->game_state == STATE_GAMEOVER) {
      SDL_SetRenderDrawColor(s->renderer, 50, 0, 0, 255); SDL_RenderClear(s->renderer); SDL_SetRenderDrawColor(s->renderer, 255, 255, 255, 255); SDL_SetRenderScale(s->renderer, 4.0f, 4.0f); SDL_RenderDebugText(s->renderer, (ww / 8.0f) - 40, (wh / 8.0f) - 10, "GAME OVER");
      SDL_SetRenderScale(s->renderer, 1.0f, 1.0f); SDL_RenderDebugText(s->renderer, (ww / 2.0f) - 80, (wh / 2.0f) + 40, "The Mothership has been destroyed."); SDL_RenderDebugText(s->renderer, (ww / 2.0f) - 60, (wh / 2.0f) + 60, "Press ESC to quit."); SDL_RenderPresent(s->renderer); return;
  }
  SDL_SetRenderDrawColor(s->renderer, 0, 0, 0, 255); SDL_RenderClear(s->renderer);
  SDL_SetRenderDrawBlendMode(s->renderer, SDL_BLENDMODE_BLEND);
  Workers_UpdateBackground(s); Workers_UpdateDensityMap(s); 
  if (s->textures.bg_texture) SDL_RenderTexture(s->renderer, s->textures.bg_texture, NULL, NULL);
  DrawParallaxLayer(s->renderer, s, ww, wh, 512, 0.1f, 0, StarLayerFn); DrawParallaxLayer(s->renderer, s, ww, wh, SYSTEM_LAYER_CELL_SIZE, SYSTEM_LAYER_PARALLAX, 1000, SystemLayerFn);
  if (s->input.show_grid) DrawGrid(s->renderer, s, ww, wh);
  if (s->input.pending_input_type == INPUT_TARGET) {
      float wx = s->camera.pos.x + s->input.mouse_pos.x / s->camera.zoom;
      float wy = s->camera.pos.y + s->input.mouse_pos.y / s->camera.zoom;
      for (int a = 0; a < MAX_ASTEROIDS; a++) {
          if (!s->world.asteroids[a].active) continue;
          float dx = s->world.asteroids[a].pos.x - wx, dy = s->world.asteroids[a].pos.y - wy;
          if (dx*dx + dy*dy < s->world.asteroids[a].radius * s->world.asteroids[a].radius) {
              Vec2 as = WorldToScreenParallax(s->world.asteroids[a].pos, 1.0f, s, ww, wh);
              float ring_r = (s->world.asteroids[a].radius * ASTEROID_VISUAL_SCALE * 1.5f) * s->camera.zoom;
              float pulse = 0.7f + 0.3f * sinf(s->current_time * 15.0f);
              SDL_Color col = {255, 255, 255, (Uint8)(200 * pulse)};
              DrawTargetRing(s->renderer, as.x, as.y, ring_r, col);
              break;
          }
      }
  }
  if (s->input.hover_asteroid_idx != -1 && s->input.pending_input_type == INPUT_NONE) {
      Vec2 as = WorldToScreenParallax(s->world.asteroids[s->input.hover_asteroid_idx].pos, 1.0f, s, ww, wh);
      float cross_sz = (s->world.asteroids[s->input.hover_asteroid_idx].radius * ASTEROID_VISUAL_SCALE * 1.2f) * s->camera.zoom;
      DrawTargetCrosshair(s->renderer, as.x, as.y, cross_sz, (SDL_Color){255, 255, 255, 100});
  }
  Renderer_DrawAsteroids(s->renderer, s, ww, wh);
  Renderer_DrawUnits(s->renderer, s, ww, wh); 
  Renderer_DrawParticles(s->renderer, s, ww, wh);
  if (s->selection.box_active) { float x1 = fminf(s->selection.box_start.x, s->selection.box_current.x), y1 = fminf(s->selection.box_start.y, s->selection.box_current.y), w = fabsf(s->selection.box_start.x - s->selection.box_current.x), h = fabsf(s->selection.box_start.y - s->selection.box_current.y); SDL_SetRenderDrawColor(s->renderer, 0, 255, 0, 50); SDL_RenderFillRect(s->renderer, &(SDL_FRect){x1, y1, w, h}); SDL_SetRenderDrawColor(s->renderer, 0, 255, 0, 200); SDL_RenderRect(s->renderer, &(SDL_FRect){x1, y1, w, h}); }
  DrawDebugInfo(s->renderer, s, ww); DrawMinimap(s->renderer, s, ww, wh); UI_DrawHUD(s);
  if (s->game_state == STATE_PAUSED) {
      SDL_SetRenderDrawColor(s->renderer, 0, 0, 0, 150); SDL_RenderFillRect(s->renderer, &(SDL_FRect){0, 0, (float)ww, (float)wh});
      SDL_SetRenderDrawColor(s->renderer, 255, 255, 255, 255);
      SDL_SetRenderScale(s->renderer, 3.0f, 3.0f);
      SDL_RenderDebugText(s->renderer, (ww / 6.0f) - 30, (wh / 6.0f) - 20, "EXIT?");
      SDL_SetRenderScale(s->renderer, 1.0f, 1.0f);
      SDL_RenderDebugText(s->renderer, (ww / 2.0f) - 100, (wh / 2.0f) + 20, "Press ENTER to Exit");
      SDL_RenderDebugText(s->renderer, (ww / 2.0f) - 100, (wh / 2.0f) + 40, "Press ESC to Continue");
  }
  SDL_RenderPresent(s->renderer);
}
