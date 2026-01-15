#include "ui.h"
#include "constants.h"
#include "utils.h"
#include <stdio.h>
#include <math.h>

void UI_DrawLauncher(AppState *s) {
    int w, h; SDL_GetRenderOutputSize(s->renderer, &w, &h);
    SDL_SetRenderDrawColor(s->renderer, 10, 10, 20, 255); SDL_RenderClear(s->renderer);
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
    TacticalBehavior primary_behavior = BEHAVIOR_OFFENSIVE;

    for (int i = 0; i < MAX_UNITS; i++) {
        if (s->world.units[i].active && s->selection.unit_selected[i]) {
            if (!any_selected) {
                primary_behavior = s->world.units[i].behavior;
                any_selected = true;
            }
            if (s->world.units[i].type == UNIT_MOTHERSHIP) {
                hp = s->world.units[i].health; max_hp = s->world.units[i].stats->max_health;
                found = true;
                has_mothership = true;
            }
        }
    }
    if (!any_selected) return;

    float gx = 20.0f, csz = 55.0f, pad = 5.0f, gy = wh - (csz * 2 + pad) - 20.0f;
    
    // Command Bar (Top row)
    const char *lb1[4] = {"Q", "W", "E", "R"};
    const char *ac1[4] = {"PATROL", "MOVE", "ATTACK", "STOP"};
    for (int col = 0; col < 4; col++) {
        SDL_FRect cell = {gx + col * (csz + pad), gy, csz, csz};
        bool key_down = false;
        if (col == 0 && s->input.key_q_down) key_down = true;
        if (col == 1 && s->input.key_w_down) key_down = true;
        if (col == 2 && s->input.key_e_down) key_down = true;
        if (col == 3 && s->input.key_r_down) key_down = true;

        if (key_down) SDL_SetRenderDrawColor(s->renderer, 200, 200, 200, 150);
        else SDL_SetRenderDrawColor(s->renderer, 40, 40, 40, 200);
        
        SDL_RenderFillRect(s->renderer, &cell); SDL_SetRenderDrawColor(s->renderer, 80, 80, 80, 255); SDL_RenderRect(s->renderer, &cell);
        
        SDL_SetRenderDrawColor(s->renderer, 255, 255, 255, 255); 
        SDL_RenderDebugText(s->renderer, cell.x + 5, cell.y + 5, lb1[col]); 
        SDL_SetRenderDrawColor(s->renderer, 150, 150, 150, 255); 
        SDL_RenderDebugText(s->renderer, cell.x + 5, cell.y + 35, ac1[col]);

        if (col == 2) { // ATTACK bar (for main cannon)
            float cd_pct = 0;
            float cd_val = 0;
            for (int i = 0; i < MAX_UNITS; i++) {
                if (s->world.units[i].active && s->world.units[i].type == UNIT_MOTHERSHIP) {
                    cd_pct = s->world.units[i].large_cannon_cooldown / s->world.units[i].stats->main_cannon_cooldown;
                    cd_val = s->world.units[i].large_cannon_cooldown;
                    break;
                }
            }
            if (cd_val > 0) {
                SDL_FRect cd_rect = {cell.x, cell.y + cell.h * (1.0f - cd_pct), cell.w, cell.h * cd_pct};
                SDL_SetRenderDrawColor(s->renderer, 0, 0, 0, 180);
                SDL_RenderFillRect(s->renderer, &cd_rect);
                char cd_str[8]; snprintf(cd_str, 8, "%.1fs", cd_val);
                SDL_SetRenderDrawColor(s->renderer, 255, 255, 255, 255);
                SDL_RenderDebugText(s->renderer, cell.x + (cell.w - (SDL_strlen(cd_str) * 8)) / 2.0f, cell.y + (cell.h - 8) / 2.0f, cd_str);
            }
        }
    }

    // Tactical Bar (Bottom row)
    gy += csz + pad;
    const char *lb2[4] = {"A", "S", "D", ""};
    const char *ac2[4] = {"OFFENS", "DEFENS", "HOLD G", ""};
    for (int col = 0; col < 3; col++) {
        SDL_FRect cell = {gx + col * (csz + pad), gy, csz, csz};
        bool key_down = false;
        if (col == 0 && s->input.key_a_down) key_down = true;
        if (col == 1 && s->input.key_s_down) key_down = true;
        if (col == 2 && s->input.key_d_down) key_down = true;

        bool is_active = false;
        if (col == 0 && primary_behavior == BEHAVIOR_OFFENSIVE) is_active = true;
        if (col == 1 && primary_behavior == BEHAVIOR_DEFENSIVE) is_active = true;
        if (col == 2 && primary_behavior == BEHAVIOR_HOLD_GROUND) is_active = true;

        if (s->ui.tactical_flash_timer > 0 && key_down) SDL_SetRenderDrawColor(s->renderer, 255, 255, 100, 255);
        else if (is_active) SDL_SetRenderDrawColor(s->renderer, 100, 100, 255, 180);
        else if (key_down) SDL_SetRenderDrawColor(s->renderer, 200, 200, 200, 150);
        else SDL_SetRenderDrawColor(s->renderer, 40, 40, 40, 200);

        SDL_RenderFillRect(s->renderer, &cell); SDL_SetRenderDrawColor(s->renderer, 80, 80, 80, 255); SDL_RenderRect(s->renderer, &cell);
        
        SDL_SetRenderDrawColor(s->renderer, 255, 255, 255, 255); 
        SDL_RenderDebugText(s->renderer, cell.x + 5, cell.y + 5, lb2[col]); 
        SDL_SetRenderDrawColor(s->renderer, 150, 150, 150, 255); 
        SDL_RenderDebugText(s->renderer, cell.x + 5, cell.y + 35, ac2[col]); 
    }

    if (found) {
        float bw = 400.0f, bh = 15.0f, bx = (ww - bw) / 2.0f, by = 30.0f;
        SDL_SetRenderDrawColor(s->renderer, 20, 40, 20, 180); SDL_RenderFillRect(s->renderer, &(SDL_FRect){bx, by, bw, bh});
        float hpp = hp / max_hp; 
        SDL_SetRenderDrawColor(s->renderer, 100, 255, 100, 255); SDL_RenderFillRect(s->renderer, &(SDL_FRect){bx, by, bw * (hpp < 0 ? 0 : hpp), bh});
        SDL_SetRenderDrawColor(s->renderer, 255, 255, 255, 255); 
        const char *title = "HULL INTEGRITY";
        SDL_RenderDebugText(s->renderer, (ww - (SDL_strlen(title) * 8)) / 2.0f, by - 15, title);
    }

    if (s->ui.ui_error_timer > 0) {
        SDL_SetRenderDrawColor(s->renderer, 255, 50, 50, 255);
        SDL_RenderDebugText(s->renderer, (ww - (SDL_strlen(s->ui.ui_error_msg) * 8)) / 2.0f, wh / 2.0f, s->ui.ui_error_msg);
    }
}