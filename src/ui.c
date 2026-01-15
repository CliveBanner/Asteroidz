#include "ui.h"
#include "constants.h"
#include "utils.h"
#include <stdio.h>
#include <math.h>

static void DrawTargetRing(SDL_Renderer *r, float x, float y, float radius, SDL_Color color) {
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
    const int segs = 32;
    for(int i=0; i<segs; i++) {
        if (i % 4 >= 2) continue;
        float a1 = i * (SDL_PI_F * 2.0f) / (float)segs, a2 = (i+1) * (SDL_PI_F * 2.0f) / (float)segs;
        SDL_RenderLine(r, x + cosf(a1)*radius, y + sinf(a1)*radius, x + cosf(a2)*radius, y + sinf(a2)*radius);
    }
}

void UI_DrawLauncher(AppState *s) {
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

void UI_DrawHUD(AppState *s) {
    int ww, wh; SDL_GetRenderLogicalPresentation(s->renderer, &ww, &wh, NULL); if (ww == 0 || wh == 0) SDL_GetRenderOutputSize(s->renderer, &ww, &wh);
    float hp = 0, max_hp = 1; bool found = false;
    bool has_mothership = false;
    bool any_selected = false;

    for (int i = 0; i < MAX_UNITS; i++) {
        if (s->world.units[i].active && s->selection.unit_selected[i]) {
            any_selected = true;
            if (s->world.units[i].type == UNIT_MOTHERSHIP) {
                hp = s->world.units[i].health; max_hp = s->world.units[i].stats->max_health;
                found = true;
                has_mothership = true;
            }
        }
    }
    if (!any_selected) return;

    float gx = 20.0f, csz = 55.0f, pad = 5.0f, gh = (3.0f * csz) + (2.0f * pad), gy = wh - gh - 20.0f;
    const char *lb[12] = {"Q", "W", "E", "R", "H", "", "", "", "", "", "", ""};
    const char *ac[12] = {"PATROL", "MOVE", "AUTO ATK", "", "HOLD", "", "", "", "", "", "", ""};
    if (has_mothership) ac[3] = "MAIN CAN"; 
    
    for (int row = 0; row < 3; row++) for (int col = 0; col < 4; col++) {
            int idx = row * 4 + col; 
            if (idx < 5 && ac[idx][0] == '\0') continue;

            SDL_FRect cell = {gx + col * (csz + pad), gy + row * (csz + pad), csz, csz};
            
            bool key_down = false;
            if (idx == 0 && s->input.key_q_down) key_down = true;
            if (idx == 1 && s->input.key_w_down) key_down = true;
            if (idx == 2 && s->input.key_e_down) key_down = true;
            if (idx == 3 && s->input.key_r_down) key_down = true;
            if (idx == 4 && s->input.key_h_down) key_down = true;

            bool auto_atk_on = (idx == 2 && s->input.auto_attack_enabled);

            if (idx == 4 && (s->ui.hold_flash_timer > 0 || s->input.key_h_down)) SDL_SetRenderDrawColor(s->renderer, 200, 200, 40, 255); 
            else if (idx == 2 && s->ui.auto_attack_flash_timer > 0) SDL_SetRenderDrawColor(s->renderer, 200, 200, 40, 255);
            else if (key_down && idx == 3) SDL_SetRenderDrawColor(s->renderer, 200, 50, 255, 200);
            else if (auto_atk_on) SDL_SetRenderDrawColor(s->renderer, 150, 40, 40, 200);
            else if (key_down) SDL_SetRenderDrawColor(s->renderer, 200, 200, 200, 150);
            else SDL_SetRenderDrawColor(s->renderer, 40, 40, 40, 200);

            
            SDL_RenderFillRect(s->renderer, &cell); SDL_SetRenderDrawColor(s->renderer, 80, 80, 80, 255); SDL_RenderRect(s->renderer, &cell);

            if (idx == 3) {
                float cd_pct = 0;
                for (int i = 0; i < MAX_UNITS; i++) {
                    if (s->world.units[i].active && s->world.units[i].type == UNIT_MOTHERSHIP) {
                        cd_pct = s->world.units[i].large_cannon_cooldown / s->world.units[i].stats->main_cannon_cooldown;
                        break;
                    }
                }
                if (cd_pct > 0) {
                    SDL_FRect cd_rect = {cell.x, cell.y + cell.h * (1.0f - cd_pct), cell.w, cell.h * cd_pct};
                    SDL_SetRenderDrawColor(s->renderer, 0, 0, 0, 180);
                    SDL_RenderFillRect(s->renderer, &cd_rect);
                    
                    char cd_str[8];
                    for (int i = 0; i < MAX_UNITS; i++) {
                        if (s->world.units[i].active && s->world.units[i].type == UNIT_MOTHERSHIP) {
                            snprintf(cd_str, 8, "%.1fs", s->world.units[i].large_cannon_cooldown);
                            break;
                        }
                    }
                    SDL_SetRenderDrawColor(s->renderer, 255, 255, 255, 255);
                    SDL_RenderDebugText(s->renderer, cell.x + (cell.w - (SDL_strlen(cd_str) * 8)) / 2.0f, cell.y + (cell.h - 8) / 2.0f, cd_str);
                }
            }

            if (lb[idx][0] != '\0') { 
                SDL_SetRenderDrawColor(s->renderer, 255, 255, 255, 255); 
                SDL_RenderDebugText(s->renderer, cell.x + 5, cell.y + 5, lb[idx]); 
                SDL_SetRenderDrawColor(s->renderer, 150, 150, 150, 255); 
                SDL_RenderDebugText(s->renderer, cell.x + 5, cell.y + 35, ac[idx]); 
            }
    }
    float bw = 400.0f, bh = 15.0f, bx = (ww - bw) / 2.0f, by = 30.0f;
    SDL_SetRenderDrawColor(s->renderer, 20, 40, 20, 180); SDL_RenderFillRect(s->renderer, &(SDL_FRect){bx, by, bw, bh});
    float hpp = hp / max_hp; 
    SDL_SetRenderDrawColor(s->renderer, 100, 255, 100, 255); SDL_RenderFillRect(s->renderer, &(SDL_FRect){bx, by, bw * (hpp < 0 ? 0 : hpp), bh});
    SDL_SetRenderDrawColor(s->renderer, 255, 255, 255, 255); 
    const char *title = "HULL INTEGRITY";
    SDL_RenderDebugText(s->renderer, (ww - (SDL_strlen(title) * 8)) / 2.0f, by - 15, title);

    if (s->ui.ui_error_timer > 0) {
        SDL_SetRenderDrawColor(s->renderer, 255, 50, 50, 255);
        SDL_RenderDebugText(s->renderer, (ww - (SDL_strlen(s->ui.ui_error_msg) * 8)) / 2.0f, wh / 2.0f - 100.0f, s->ui.ui_error_msg);
    }

    float sx = ww / 2.0f, sy = wh - 80.0f, isz = 60.0f; int sc = 0;
    for (int i = 0; i < MAX_UNITS; i++) if (s->selection.unit_selected[i]) sc++;
    if (sc > 0) {
        float tw = sc * (isz + 5.0f), cx = sx - tw / 2.0f;
        for (int i = 0; i < MAX_UNITS; i++) {
            if (!s->selection.unit_selected[i]) continue;
            
            float min_dist = 1e10f;
            for (int j = 0; j < MAX_ASTEROIDS; j++) {
                if (!s->world.asteroids[j].active) continue;
                float dx = s->world.asteroids[j].pos.x - s->world.units[i].pos.x, dy = s->world.asteroids[j].pos.y - s->world.units[i].pos.y;
                float d = sqrtf(dx * dx + dy * dy); 
                if (d < min_dist) min_dist = d;
            }
            if (min_dist < 0) min_dist = 0;

            Uint8 rr = 100, rg = 255, rb = 100;
            if (min_dist < WARNING_RANGE_NEAR) { rr = 200; rg = 60; rb = 60; }
            else if (min_dist < WARNING_RANGE_MID) { rr = 150; rg = 100; rb = 80; }
            else if (min_dist < WARNING_RANGE_FAR) { rr = 80; rg = 80; rb = 150; }

            SDL_FRect ir = {cx, sy, isz, isz}; SDL_SetRenderDrawColor(s->renderer, 40, 40, 40, 200); SDL_RenderFillRect(s->renderer, &ir); 
            SDL_SetRenderDrawColor(s->renderer, rr, rg, rb, 255); SDL_RenderRect(s->renderer, &ir);
            
            float br_h = isz - 10.0f, br_w = 5.0f, br_x = cx + isz - 8.0f, br_y = sy + 5.0f;
            SDL_SetRenderDrawColor(s->renderer, 0, 0, 100, 200); SDL_RenderFillRect(s->renderer, &(SDL_FRect){br_x, br_y, br_w, br_h});
            SDL_SetRenderDrawColor(s->renderer, 50, 150, 255, 255); SDL_RenderFillRect(s->renderer, &(SDL_FRect){br_x, br_y + br_h * (1.0f - s->world.energy/INITIAL_ENERGY), br_w, br_h * (s->world.energy/INITIAL_ENERGY)});
            br_x -= 7.0f;
            if (s->world.units[i].stats->main_cannon_damage > 0) {
                SDL_SetRenderDrawColor(s->renderer, 100, 0, 100, 200); SDL_RenderFillRect(s->renderer, &(SDL_FRect){br_x, br_y, br_w, br_h});
                float cd_pct = s->world.units[i].large_cannon_cooldown / s->world.units[i].stats->main_cannon_cooldown;
                SDL_SetRenderDrawColor(s->renderer, 200, 50, 255, 255); SDL_RenderFillRect(s->renderer, &(SDL_FRect){br_x, br_y + br_h * (1.0f - (1.0f - cd_pct)), br_w, br_h * (1.0f - cd_pct)});
                br_x -= 7.0f;
            }
            SDL_SetRenderDrawColor(s->renderer, 20, 40, 20, 200); SDL_RenderFillRect(s->renderer, &(SDL_FRect){br_x, br_y, br_w, br_h});
            SDL_SetRenderDrawColor(s->renderer, 100, 255, 100, 255); SDL_RenderFillRect(s->renderer, &(SDL_FRect){br_x, br_y + br_h * (1.0f - s->world.units[i].health/s->world.units[i].stats->max_health), br_w, br_h * (s->world.units[i].health/s->world.units[i].stats->max_health)});

            SDL_SetRenderDrawColor(s->renderer, 255, 255, 255, 255);
            SDL_SetRenderScale(s->renderer, 2.0f, 2.0f);
            SDL_RenderDebugText(s->renderer, (cx + 10.0f) / 2.0f, (sy + 10.0f) / 2.0f, s->world.units[i].type == UNIT_MOTHERSHIP ? "M" : "U");
            SDL_SetRenderScale(s->renderer, 1.0f, 1.0f);
            
            char d_str[16]; 
            if (min_dist > 9999.0f) snprintf(d_str, 16, ">9k");
            else snprintf(d_str, 16, "%.0f", min_dist);
            SDL_SetRenderDrawColor(s->renderer, rr, rg, rb, 255);
            SDL_RenderDebugText(s->renderer, cx + 5, sy + isz - 15, d_str);
            
            cx += isz + 5.0f;
        }
    }
}
