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

    // --- RTS Command Card (3x5 Grid) ---
    float csz = 60.0f, pad = 4.0f;
    float card_w = (csz * 5) + (pad * 4);
    float card_h = (csz * 3) + (pad * 2);
    float gx = 20.0f;
    float gy = wh - card_h - 20.0f;

    // Grid Mapping:
    // [Q] [W] [E] [R] [ ]
    // [A] [S] [D] [ ] [ ]
    // [Z] [ ] [ ] [ ] [ ]

    struct {
        const char *hotkey;
        const char *label;
        bool is_active;
        bool key_down;
        int row, col;
        float cd_pct;
        float cd_val;
    } buttons[15];
    SDL_memset(buttons, 0, sizeof(buttons));

    // Row 0: Commands
    buttons[0] = (typeof(buttons[0])){ "Q", "PATROL", false, s->input.key_q_down, 0, 0 };
    buttons[1] = (typeof(buttons[0])){ "W", "MOVE",   false, s->input.key_w_down, 0, 1 };
    buttons[2] = (typeof(buttons[0])){ "E", "ATTACK", false, s->input.key_e_down, 0, 2 };
    buttons[3] = (typeof(buttons[0])){ "R", "STOP",   false, s->input.key_r_down, 0, 3 };

    // Row 1: Tactical Behaviors
    buttons[5] = (typeof(buttons[0])){ "A", "OFFENS", primary_behavior == BEHAVIOR_OFFENSIVE,   s->input.key_a_down, 1, 0 };
    buttons[6] = (typeof(buttons[0])){ "S", "DEFENS", primary_behavior == BEHAVIOR_DEFENSIVE,   s->input.key_s_down, 1, 1 };
    buttons[7] = (typeof(buttons[0])){ "D", "HOLD G", primary_behavior == BEHAVIOR_HOLD_GROUND, s->input.key_d_down, 1, 2 };

    // Row 2: Abilities (Mothership Main Cannon)
    if (has_mothership) {
        float cd_pct = 0, cd_val = 0;
        for (int i = 0; i < MAX_UNITS; i++) {
            if (s->world.units[i].active && s->world.units[i].type == UNIT_MOTHERSHIP) {
                cd_pct = s->world.units[i].large_cannon_cooldown / s->world.units[i].stats->main_cannon_cooldown;
                cd_val = s->world.units[i].large_cannon_cooldown;
                break;
            }
        }
        buttons[10] = (typeof(buttons[0])){ "Z", "MAIN C", false, false, 2, 0, cd_pct, cd_val };
    }

    for (int i = 0; i < 15; i++) {
        int r = i / 5, c = i % 5;
        SDL_FRect cell = {gx + c * (csz + pad), gy + r * (csz + pad), csz, csz};
        
        // Background
        if (buttons[i].hotkey && buttons[i].hotkey[0] != '\0') {
            if (buttons[i].key_down) SDL_SetRenderDrawColor(s->renderer, 200, 200, 200, 150);
            else if (buttons[i].is_active) SDL_SetRenderDrawColor(s->renderer, 100, 100, 255, 180);
            else SDL_SetRenderDrawColor(s->renderer, 40, 40, 40, 200);
        } else {
            SDL_SetRenderDrawColor(s->renderer, 20, 20, 20, 100);
        }
        
        SDL_RenderFillRect(s->renderer, &cell);
        SDL_SetRenderDrawColor(s->renderer, 60, 60, 60, 255);
        SDL_RenderRect(s->renderer, &cell);

        if (buttons[i].hotkey && buttons[i].hotkey[0] != '\0') {
            // Hotkey label
            SDL_SetRenderDrawColor(s->renderer, 255, 255, 255, 255);
            SDL_RenderDebugText(s->renderer, cell.x + 4, cell.y + 4, buttons[i].hotkey);
            // Action label
            SDL_SetRenderDrawColor(s->renderer, 180, 180, 180, 255);
            SDL_RenderDebugText(s->renderer, cell.x + 4, cell.y + csz - 14, buttons[i].label);

            // Cooldown
            if (buttons[i].cd_val > 0) {
                SDL_FRect cd_rect = {cell.x, cell.y + cell.h * (1.0f - buttons[i].cd_pct), cell.w, cell.h * buttons[i].cd_pct};
                SDL_SetRenderDrawColor(s->renderer, 0, 0, 0, 180);
                SDL_RenderFillRect(s->renderer, &cd_rect);
                char cd_str[8]; snprintf(cd_str, 8, "%.1f", buttons[i].cd_val);
                SDL_SetRenderDrawColor(s->renderer, 255, 255, 255, 255);
                SDL_RenderDebugText(s->renderer, cell.x + (csz - SDL_strlen(cd_str)*8)/2, cell.y + (csz-8)/2, cd_str);
            }
        }
    }

    // --- Hull Integrity Bar ---
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
