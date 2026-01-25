#include "ui.h"
#include "constants.h"
#include "utils.h"
#include <stdio.h>
#include <math.h>

void UI_SetError(AppState *s, const char *msg) {
    SDL_strlcpy(s->ui.ui_error_msg, msg, sizeof(s->ui.ui_error_msg));
    s->ui.ui_error_timer = 2.0f;
    LogTransaction(s, 0.0f, msg);
}

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

    // --- Global Resource Display (Top Right) ---
    SDL_SetRenderScale(s->renderer, 1.25f, 1.25f);
    char energy_str[32], res_str[32];
    snprintf(energy_str, 32, "ENERGY: %.0f", s->world.energy);
    snprintf(res_str, 32, "RESOURCES: %.0f", s->world.stored_resources);
    
    float energy_x = (ww / 1.25f) - 140.0f;
    float energy_y = 20.0f / 1.25f;
    
    SDL_SetRenderDrawColor(s->renderer, 100, 200, 255, 255); // Energy color
    SDL_RenderDebugText(s->renderer, energy_x, energy_y, energy_str);
    
    SDL_SetRenderDrawColor(s->renderer, 255, 255, 255, 255); // Resource color
    SDL_RenderDebugText(s->renderer, energy_x, energy_y + 15.0f, res_str);
    
    if (s->textures.icon_textures[ICON_GATHER]) {
        SDL_RenderTexture(s->renderer, s->textures.icon_textures[ICON_GATHER], NULL, &(SDL_FRect){energy_x - 22, energy_y + 13.0f, 18, 18});
    }
    SDL_SetRenderScale(s->renderer, 1.0f, 1.0f);

    // --- Transaction Log (Below Resources) ---
    float log_y = 75.0f;
    for (int i = 0; i < MAX_LOGS; i++) {
        if (s->ui.transaction_log[i].life > 0) {
            float alpha = fminf(1.0f, s->ui.transaction_log[i].life);
            SDL_SetRenderDrawColor(s->renderer, s->ui.transaction_log[i].val > 0 ? 100 : 255, s->ui.transaction_log[i].val > 0 ? 255 : 100, 100, (Uint8)(alpha * 255));
            char log_line[64];
            snprintf(log_line, 64, "%s %s%.0f", s->ui.transaction_log[i].label, s->ui.transaction_log[i].val > 0 ? "+" : "", s->ui.transaction_log[i].val);
            SDL_RenderDebugText(s->renderer, ww - (SDL_strlen(log_line) * 8) - 20, log_y, log_line);
            log_y += 15.0f;
        }
    }

    float hp = 0, max_hp = 1; bool found = false;
    bool has_mothership = false;
    bool has_miner = false;
    bool has_fighter = false;
    bool any_selected = false;
    TacticalBehavior primary_behavior = BEHAVIOR_OFFENSIVE;

    int mothership_idx = -1;
    for (int i = 0; i < MAX_UNITS; i++) {
        if (s->world.units.active[i] && s->selection.unit_selected[i]) {
            if (!any_selected) {
                primary_behavior = s->world.units.behavior[i];
                any_selected = true;
            }
            if (s->world.units.type[i] == UNIT_MOTHERSHIP) {
                hp = s->world.units.health[i]; max_hp = s->world.units.stats[i]->max_health;
                found = true;
                has_mothership = true;
                mothership_idx = i;
            }
            if (s->world.units.type[i] == UNIT_MINER) has_miner = true;
            if (s->world.units.type[i] == UNIT_FIGHTER) has_fighter = true;
        }
    }
    
    if (s->ui.ui_error_timer > 0) {
        float scale = 2.0f;
        SDL_SetRenderScale(s->renderer, scale, scale);
        float tw = (float)SDL_strlen(s->ui.ui_error_msg) * 8.0f;
        float tx = (ww / scale - tw) / 2.0f;
        float ty = (wh / scale) / 2.0f;
        
        // Background for error
        SDL_SetRenderDrawColor(s->renderer, 0, 0, 0, 180);
        SDL_RenderFillRect(s->renderer, &(SDL_FRect){tx - 4, ty - 4, tw + 8, 16});
        
        SDL_SetRenderDrawColor(s->renderer, 255, 50, 50, 255);
        SDL_RenderDebugText(s->renderer, tx, ty, s->ui.ui_error_msg);
        SDL_SetRenderScale(s->renderer, 1.0f, 1.0f);
    }

    // --- Unit Group Display (Top Center) ---
    struct { const char *hk; SDL_Texture *tex; int count; bool selected; } groups[3];
    int n_miners = 0, n_fighters = 0, n_all = 0;
    bool miners_sel = false, fighters_sel = false, all_sel = false;
    
    for (int i = 0; i < MAX_UNITS; i++) {
        if (!s->world.units.active[i]) continue;
        if (s->world.units.type[i] == UNIT_MINER) { 
            n_miners++; n_all++; 
            if (s->selection.unit_selected[i]) { miners_sel = true; all_sel = true; }
        }
        else if (s->world.units.type[i] == UNIT_FIGHTER) { 
            n_fighters++; n_all++; 
            if (s->selection.unit_selected[i]) { fighters_sel = true; all_sel = true; }
        }
        else {
            n_all++;
            if (s->selection.unit_selected[i]) all_sel = true;
        }
    }
    groups[0] = (typeof(groups[0])){ "F1", s->textures.miner_texture, n_miners, miners_sel };
    groups[1] = (typeof(groups[0])){ "F2", s->textures.fighter_texture, n_fighters, fighters_sel };
    groups[2] = (typeof(groups[0])){ "F3", s->textures.mothership_hull_texture, n_all, all_sel };

    float g_icon_sz = 40.0f, g_pad = 20.0f;
    float g_total_w = (g_icon_sz + g_pad) * 3 - g_pad;
    float gx_top = (ww - g_total_w) / 2.0f;
    float gy_top = 15.0f;

    for (int i = 0; i < 3; i++) {
        float x = gx_top + i * (g_icon_sz + g_pad);
        SDL_FRect r = {x, gy_top, g_icon_sz, g_icon_sz};
        
        SDL_SetRenderDrawColor(s->renderer, 30, 30, 30, 180);
        SDL_RenderFillRect(s->renderer, &r);
        if (groups[i].selected) {
            SDL_SetRenderDrawColor(s->renderer, 255, 255, 255, 255);
            SDL_RenderRect(s->renderer, &(SDL_FRect){x-2, gy_top-2, g_icon_sz+4, g_icon_sz+4});
        }
        if (groups[i].tex) SDL_RenderTexture(s->renderer, groups[i].tex, NULL, &r);
        
        SDL_SetRenderDrawColor(s->renderer, 255, 255, 255, 255);
        SDL_RenderDebugText(s->renderer, x, gy_top - 12, groups[i].hk);
        char c_str[16]; snprintf(c_str, 16, "%d", groups[i].count);
        SDL_RenderDebugText(s->renderer, x + (g_icon_sz - SDL_strlen(c_str)*8)/2, gy_top + g_icon_sz + 4, c_str);
    }

    if (!any_selected) return;

    // --- RTS Command Card (3x5 Grid) ---
    float csz = 60.0f, pad = 4.0f;
    float card_w = (csz * 5) + (pad * 4);
    float card_h = (csz * 3) + (pad * 2);
    float gx = 20.0f;
    float gy = wh - card_h - 20.0f;

    // --- Production Queue / Toggle Display ---
    if (mothership_idx != -1 && s->world.units.production_mode[mothership_idx] != UNIT_TYPE_COUNT) {
        float queue_x = 20.0f;
        float unit_icon_sz_q = 40.0f;
        float queue_y = gy - 30.0f - unit_icon_sz_q; // Above command card
        
        SDL_SetRenderDrawColor(s->renderer, 200, 200, 200, 255);
        SDL_RenderDebugText(s->renderer, queue_x, queue_y - 15.0f, "AUTO PRODUCTION ACTIVE");

        UnitType ut = s->world.units.production_mode[mothership_idx];
        SDL_Texture *tex = (ut == UNIT_MINER) ? s->textures.miner_texture : s->textures.fighter_texture;
        
        SDL_FRect r = {queue_x, queue_y, unit_icon_sz_q, unit_icon_sz_q};
        SDL_SetRenderDrawColor(s->renderer, 40, 40, 40, 200);
        SDL_RenderFillRect(s->renderer, &r);
        if (tex) SDL_RenderTexture(s->renderer, tex, NULL, &r);
        
        float total = s->world.unit_stats[ut].production_time;
        float current = s->world.units.production_timer[mothership_idx];
        float pct = current / total;
        SDL_SetRenderDrawColor(s->renderer, 0, 255, 0, 150);
        SDL_FRect pr = {queue_x, queue_y + unit_icon_sz_q, unit_icon_sz_q * pct, 4};
        SDL_RenderFillRect(s->renderer, &pr);
    }

    struct {
        const char *hotkey;
        const char *label;
        SDL_Texture *texture;
        bool is_active;
        bool key_down;
        int row, col;
        float cd_pct;
        float cd_val;
    } buttons[15];
    SDL_memset(buttons, 0, sizeof(buttons));


    if (s->ui.menu_state == 0) {
        buttons[0] = (typeof(buttons[0])){ "Q", "PATROL", s->textures.icon_textures[ICON_PATROL], false, s->input.key_q_down, 0, 0 };
        buttons[1] = (typeof(buttons[0])){ "W", "MOVE",   s->textures.icon_textures[ICON_MOVE], false, s->input.key_w_down, 0, 1 };
        buttons[2] = (typeof(buttons[0])){ "E", "ATTACK", s->textures.icon_textures[ICON_ATTACK], false, s->input.key_e_down, 0, 2 };
        buttons[3] = (typeof(buttons[0])){ "R", "STOP",   s->textures.icon_textures[ICON_STOP], false, s->input.key_r_down, 0, 3 };
        buttons[5] = (typeof(buttons[0])){ "A", "OFFENS", s->textures.icon_textures[ICON_OFFENSIVE], primary_behavior == BEHAVIOR_OFFENSIVE,   s->input.key_a_down, 1, 0 };
        buttons[6] = (typeof(buttons[0])){ "S", "DEFENS", s->textures.icon_textures[ICON_DEFENSIVE], primary_behavior == BEHAVIOR_DEFENSIVE,   s->input.key_s_down, 1, 1 };
        buttons[7] = (typeof(buttons[0])){ "D", "HOLD G", s->textures.icon_textures[ICON_HOLD], primary_behavior == BEHAVIOR_HOLD_GROUND, s->input.key_d_down, 1, 2 };
        
        if (has_mothership) {
            float m_cd_pct = 0, m_cd_val = 0;
            for (int i = 0; i < MAX_UNITS; i++) if (s->world.units.active[i] && s->world.units.type[i] == UNIT_MOTHERSHIP) { m_cd_pct = s->world.units.large_cannon_cooldown[i] / s->world.units.stats[i]->main_cannon_cooldown; m_cd_val = s->world.units.large_cannon_cooldown[i]; break; }
            buttons[10] = (typeof(buttons[0])){ "Z", "MAIN C", s->textures.icon_textures[ICON_MAIN_CANNON], false, s->input.key_z_down, 2, 0, m_cd_pct, m_cd_val };
            buttons[11] = (typeof(buttons[0])){ "X", "BUILD", s->textures.miner_texture, false, s->input.key_x_down, 2, 1 };
        }
        if (has_miner) {
            // Merge Gather/Return into available slots
            if (!has_mothership) buttons[10] = (typeof(buttons[0])){ "Z", "GATHER", s->textures.icon_textures[ICON_GATHER], false, s->input.key_z_down, 2, 0 };
            buttons[12] = (typeof(buttons[0])){ "V", "RETURN", s->textures.icon_textures[ICON_RETURN], false, false, 2, 2 };
        }
    } else if (s->ui.menu_state == 1) {
        if (has_mothership) {
            UnitType active_mode = UNIT_TYPE_COUNT;
            for (int i = 0; i < MAX_UNITS; i++) if (s->world.units.active[i] && s->world.units.type[i] == UNIT_MOTHERSHIP && s->selection.unit_selected[i]) { active_mode = s->world.units.production_mode[i]; break; }
            buttons[0] = (typeof(buttons[0])){ "Q", "TGL MINR", s->textures.miner_texture, active_mode == UNIT_MINER, s->input.key_q_down, 0, 0 };
            buttons[1] = (typeof(buttons[0])){ "W", "TGL FGHT", s->textures.fighter_texture, active_mode == UNIT_FIGHTER, s->input.key_w_down, 0, 1 };
            buttons[10] = (typeof(buttons[0])){ "Z", "BACK", s->textures.icon_textures[ICON_BACK], false, s->input.key_z_down, 2, 0 };
        }
    }

    for (int i = 0; i < 15; i++) {
        int r = i / 5, c = i % 5;
        SDL_FRect cell = {gx + c * (csz + pad), gy + r * (csz + pad), csz, csz};
        if (buttons[i].hotkey && buttons[i].hotkey[0] != '\0') {
            if (buttons[i].key_down) SDL_SetRenderDrawColor(s->renderer, 200, 200, 200, 150);
            else if (buttons[i].is_active) SDL_SetRenderDrawColor(s->renderer, 100, 100, 255, 180);
            else SDL_SetRenderDrawColor(s->renderer, 40, 40, 40, 200);
        } else SDL_SetRenderDrawColor(s->renderer, 20, 20, 20, 100);
        SDL_RenderFillRect(s->renderer, &cell);
        SDL_SetRenderDrawColor(s->renderer, 60, 60, 60, 255);
        SDL_RenderRect(s->renderer, &cell);
        if (buttons[i].hotkey && buttons[i].hotkey[0] != '\0') {
            if (buttons[i].texture) SDL_RenderTexture(s->renderer, buttons[i].texture, NULL, &cell);
            SDL_SetRenderDrawColor(s->renderer, 255, 255, 255, 255);
            SDL_RenderDebugText(s->renderer, cell.x + 4, cell.y + 4, buttons[i].hotkey);
            if (!buttons[i].texture) { SDL_SetRenderDrawColor(s->renderer, 180, 180, 180, 255); SDL_RenderDebugText(s->renderer, cell.x + 4, cell.y + csz - 14, buttons[i].label); }
            if (buttons[i].cd_pct > 0.0f) { SDL_FRect cd_rect = {cell.x, cell.y + cell.h * (1.0f - buttons[i].cd_pct), cell.w, cell.h * buttons[i].cd_pct}; SDL_SetRenderDrawColor(s->renderer, 0, 0, 0, 180); SDL_RenderFillRect(s->renderer, &cd_rect); }
            if (buttons[i].cd_val > 0.0f) { char cd_str[8]; snprintf(cd_str, 8, "%.1f", buttons[i].cd_val); SDL_SetRenderDrawColor(s->renderer, 255, 255, 255, 255); SDL_RenderDebugText(s->renderer, cell.x + (csz - SDL_strlen(cd_str)*8)/2, cell.y + (csz-8)/2, cd_str); }
        }
    }
}
