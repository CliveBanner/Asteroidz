#include "input.h"
#include "constants.h"
#include "game.h"
#include "ui.h"
#include "persistence.h"
#include "utils.h"
#include <math.h>
#include <stdio.h>

void Input_ScheduleCommand(AppState *s, Command cmd, bool queue) {
    for (int i = 0; i < MAX_UNITS; i++) {
        if (!s->world.units.active[i] || !s->selection.unit_selected[i]) continue;

        if (cmd.type == CMD_MAIN_CANNON) {
            if (s->world.units.large_cannon_cooldown[i] > 0) {
                UI_SetError(s, "MAIN CANNON COOLDOWN");
                continue;
            }
            if (s->world.units.type[i] == UNIT_MOTHERSHIP && cmd.target_idx != -1) {
                s->world.units.large_target_idx[i] = cmd.target_idx;
            }
            continue;
        }

        if (cmd.type == CMD_GATHER) {
            if (s->world.units.type[i] != UNIT_MOTHERSHIP && s->world.units.current_cargo[i] >= s->world.units.stats[i]->max_cargo) {
                UI_SetError(s, "CARGO FULL");
                continue;
            }
        }

        if (queue) {
            if (s->world.units.command_count[i] < MAX_COMMANDS) {
                s->world.units.command_queue[i][s->world.units.command_count[i]++] = cmd;
                s->world.units.has_target[i] = true;
            }
        } else {
            s->world.units.command_queue[i][0] = cmd;
            s->world.units.command_count[i] = 1;
            s->world.units.command_current_idx[i] = 0;
            s->world.units.has_target[i] = true;
        }
    }
}

void Input_HandleMouseWheel(AppState *s, SDL_MouseWheelEvent *event) {
    int win_w, win_h;
    SDL_GetRenderOutputSize(s->renderer, &win_w, &win_h);
    float center_world_x = s->camera.pos.x + (win_w / 2.0f) / s->camera.zoom;
    float center_world_y = s->camera.pos.y + (win_h / 2.0f) / s->camera.zoom;
    
    if (event->y > 0) s->camera.zoom *= ZOOM_STEP;
    if (event->y < 0) s->camera.zoom /= ZOOM_STEP;
    
    if (s->camera.zoom < MIN_ZOOM) s->camera.zoom = MIN_ZOOM;
    if (s->camera.zoom > MAX_ZOOM) s->camera.zoom = MAX_ZOOM;
    
    s->camera.pos.x = center_world_x - (win_w / 2.0f) / s->camera.zoom;
    s->camera.pos.y = center_world_y - (win_h / 2.0f) / s->camera.zoom;
}

void Input_HandleMouseMove(AppState *s, SDL_MouseMotionEvent *event) {
    s->input.mouse_pos.x = event->x;
    s->input.mouse_pos.y = event->y;
    if (s->selection.box_active) s->selection.box_current = s->input.mouse_pos;
}

static void HandleLeftClick(AppState *s, SDL_MouseButtonEvent *event, int win_w, int win_h) {
    float mm_x = win_w - MINIMAP_SIZE - MINIMAP_MARGIN;
    float mm_y = win_h - MINIMAP_SIZE - MINIMAP_MARGIN;
    float csz = 60.0f, pad = 4.0f;
    float card_h = (csz * 3) + (pad * 2);
    float gx = 20.0f;
    float gy = (float)win_h - card_h - 20.0f;

    // Minimap interaction
    if (event->x >= mm_x && event->x <= mm_x + MINIMAP_SIZE && 
        event->y >= mm_y && event->y <= mm_y + MINIMAP_SIZE) {
        float rel_x = (event->x - mm_x) / MINIMAP_SIZE - 0.5f;
        float rel_y = (event->y - mm_y) / MINIMAP_SIZE - 0.5f;
        s->camera.pos.x += rel_x * MINIMAP_RANGE;
        s->camera.pos.y += rel_y * MINIMAP_RANGE;
        return;
    }

    // Command Grid interaction
    if (event->x >= gx && event->x <= gx + (csz + pad) * 5 && 
        event->y >= gy && event->y <= gy + (csz + pad) * 3) {
        int col = (int)((event->x - gx) / (csz + pad));
        int row = (int)((event->y - gy) / (csz + pad));
        int btn_idx = row * 5 + col;

        if (s->ui.menu_state == 0) {
            if (btn_idx == 0) s->input.pending_cmd_type = CMD_PATROL;
            else if (btn_idx == 1) s->input.pending_cmd_type = CMD_MOVE;
            else if (btn_idx == 2) s->input.pending_cmd_type = CMD_ATTACK_MOVE;
            else if (btn_idx == 3) { // Stop
                for (int i = 0; i < MAX_UNITS; i++) if (s->world.units.active[i] && s->selection.unit_selected[i]) { 
                    s->world.units.velocity[i] = (Vec2){0,0}; s->world.units.has_target[i] = false; 
                    s->world.units.command_count[i] = 0; s->world.units.command_current_idx[i] = 0; 
                }
                s->ui.hold_flash_timer = 0.2f;
            }
            else if (btn_idx == 5) { for (int i = 0; i < MAX_UNITS; i++) if (s->world.units.active[i] && s->selection.unit_selected[i]) s->world.units.behavior[i] = BEHAVIOR_OFFENSIVE; s->ui.tactical_flash_timer = 0.2f; }
            else if (btn_idx == 6) { for (int i = 0; i < MAX_UNITS; i++) if (s->world.units.active[i] && s->selection.unit_selected[i]) s->world.units.behavior[i] = BEHAVIOR_DEFENSIVE; s->ui.tactical_flash_timer = 0.2f; }
            else if (btn_idx == 7) { for (int i = 0; i < MAX_UNITS; i++) if (s->world.units.active[i] && s->selection.unit_selected[i]) s->world.units.behavior[i] = BEHAVIOR_HOLD_GROUND; s->ui.tactical_flash_timer = 0.2f; }
            else if (btn_idx == 10) s->input.pending_cmd_type = CMD_MAIN_CANNON;
            else if (btn_idx == 11) s->ui.menu_state = 1; // Build
            else if (btn_idx == 12) { // Return Cargo
                 for (int i = 0; i < MAX_UNITS; i++) {
                    if (s->world.units.active[i] && s->selection.unit_selected[i] && s->world.units.type[i] == UNIT_MINER) {
                        int m_idx = -1; float min_d = 1e18;
                        for (int j = 0; j < MAX_UNITS; j++) {
                            if (s->world.units.active[j] && s->world.units.type[j] == UNIT_MOTHERSHIP) {
                                float d = Vector_DistanceSq(s->world.units.pos[i], s->world.units.pos[j]);
                                if (d < min_d) { min_d = d; m_idx = j; }
                            }
                        }
                        if (m_idx != -1) {
                            Command cmd = { .pos = s->world.units.pos[m_idx], .target_idx = m_idx, .type = CMD_RETURN_CARGO };
                            Input_ScheduleCommand(s, cmd, s->input.shift_down);
                        }
                    }
                }
            }
        } else if (s->ui.menu_state == 1) {
            if (btn_idx == 0) { // Toggle Miner
                for (int i = 0; i < MAX_UNITS; i++) if (s->world.units.active[i] && s->selection.unit_selected[i] && s->world.units.type[i] == UNIT_MOTHERSHIP) {
                    if (s->world.units.production_count[i] < MAX_PRODUCTION_QUEUE) {
                        s->world.units.production_queue[i][s->world.units.production_count[i]++] = UNIT_MINER;
                    }
                }
            } else if (btn_idx == 1) { // Toggle Fighter
                for (int i = 0; i < MAX_UNITS; i++) if (s->world.units.active[i] && s->selection.unit_selected[i] && s->world.units.type[i] == UNIT_MOTHERSHIP) {
                    if (s->world.units.production_count[i] < MAX_PRODUCTION_QUEUE) {
                        s->world.units.production_queue[i][s->world.units.production_count[i]++] = UNIT_FIGHTER;
                    }
                }
            } else if (btn_idx == 10) s->ui.menu_state = 0; // Back
        }
        return;
    }

    // World interaction (Selection)
    float wx = s->camera.pos.x + event->x / s->camera.zoom;
    float wy = s->camera.pos.y + event->y / s->camera.zoom;
    bool clicked_unit = false;
    for (int i = 0; i < MAX_UNITS; i++) {
        if (!s->world.units.active[i]) continue;
        float dx = s->world.units.pos[i].x - wx, dy = s->world.units.pos[i].y - wy;
        float r = s->world.units.stats[i]->radius;
        if (dx * dx + dy * dy < r * r) {
            if (!s->input.shift_down) SDL_memset(s->selection.unit_selected, 0, sizeof(s->selection.unit_selected));
            s->selection.unit_selected[i] = true;
            s->selection.primary_unit_idx = i;
            clicked_unit = true;
            break;
        }
    }
    if (!clicked_unit) {
        s->selection.box_active = true;
        s->selection.box_start = (Vec2){event->x, event->y};
        s->selection.box_current = s->selection.box_start;
    }
}

static void HandleRightClick(AppState *s, SDL_MouseButtonEvent *event) {
    float wx = s->camera.pos.x + event->x / s->camera.zoom;
    float wy = s->camera.pos.y + event->y / s->camera.zoom;

    int target_a = -1;
    for (int a = 0; a < MAX_ASTEROIDS; a++) {
        if (!s->world.asteroids.active[a]) continue;
        float dx = s->world.asteroids.pos[a].x - wx, dy = s->world.asteroids.pos[a].y - wy;
        float r = s->world.asteroids.radius[a] * ASTEROID_HITBOX_MULT;
        if (dx * dx + dy * dy < r * r) {
            target_a = a;
            break;
        }
    }

    CommandType type = CMD_IDLE;
    
    // 1. Check pending/sticky command
    if (s->input.pending_cmd_type != CMD_IDLE) {
        type = s->input.pending_cmd_type;
    } 
    // 2. Check held keys (legacy/power user mode)
    else if (s->input.key_q_down) type = CMD_PATROL;
    else if (s->input.key_w_down) type = CMD_MOVE;
    else if (s->input.key_e_down) type = CMD_ATTACK_MOVE;
    else if (s->input.key_y_down) type = CMD_MAIN_CANNON;
    // 3. Contextual defaults
    else if (s->input.hover_resource_idx != -1) {
        type = CMD_GATHER;
        target_a = s->input.hover_resource_idx;
    } else {
        type = (target_a != -1) ? CMD_ATTACK_MOVE : CMD_MOVE;
    }

    if (type == CMD_MAIN_CANNON && target_a == -1) {
        UI_SetError(s, "TARGET REQUIRED");
        return;
    }

    Command cmd = { .pos = {wx, wy}, .target_idx = target_a, .type = type };
    Input_ScheduleCommand(s, cmd, s->input.shift_down);

    // Reset sticky command if not shifting
    if (!s->input.shift_down) {
        s->input.pending_cmd_type = CMD_IDLE;
    }
}

void Input_HandleMouseButtonDown(AppState *s, SDL_MouseButtonEvent *event) {
    int win_w, win_h;
    SDL_GetRenderOutputSize(s->renderer, &win_w, &win_h);

    if (event->button == SDL_BUTTON_LEFT) {
        HandleLeftClick(s, event, win_w, win_h);
    } else if (event->button == SDL_BUTTON_RIGHT) {
        HandleRightClick(s, event);
    }
}

void Input_HandleMouseButtonUp(AppState *s, SDL_MouseButtonEvent *event) {
    if (event->button == SDL_BUTTON_LEFT && s->selection.box_active) {
        s->selection.box_active = false;
        float x1 = fminf(s->selection.box_start.x, s->selection.box_current.x);
        float y1 = fminf(s->selection.box_start.y, s->selection.box_current.y);
        float x2 = fmaxf(s->selection.box_start.x, s->selection.box_current.x);
        float y2 = fmaxf(s->selection.box_start.y, s->selection.box_current.y);
        
        bool any_selected = false;
        for (int i = 0; i < MAX_UNITS; i++) {
            if (!s->world.units.active[i]) {
                if (!s->input.shift_down) s->selection.unit_selected[i] = false;
                continue;
            }
            Vec2 sp = { 
                (s->world.units.pos[i].x - s->camera.pos.x) * s->camera.zoom, 
                (s->world.units.pos[i].y - s->camera.pos.y) * s->camera.zoom 
            };
            if (sp.x >= x1 && sp.x <= x2 && sp.y >= y1 && sp.y <= y2) {
                s->selection.unit_selected[i] = true;
                s->selection.primary_unit_idx = i;
                any_selected = true;
            } else if (!s->input.shift_down) {
                s->selection.unit_selected[i] = false;
            }
        }
        if (!any_selected && !s->input.shift_down) s->selection.primary_unit_idx = -1;
    }
}

void Input_HandleKeyDown(AppState *s, SDL_KeyboardEvent *event) {
    SDL_Keycode key = event->key;
    if (key == SDLK_ESCAPE) {
        if (s->game_state == STATE_GAME) s->game_state = STATE_PAUSED;
        else if (s->game_state == STATE_PAUSED) s->game_state = STATE_GAME;
        return;
    }
    if (s->game_state == STATE_PAUSED) {
        if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
            SDL_Event qe; qe.type = SDL_EVENT_QUIT; SDL_PushEvent(&qe);
        }
        return;
    }

    if (key == SDLK_LSHIFT || key == SDLK_RSHIFT) s->input.shift_down = true;
    if (key == SDLK_LCTRL || key == SDLK_RCTRL) s->input.ctrl_down = true;

    // Control Groups
    if (key >= SDLK_1 && key <= SDLK_9) {
        int g = key - SDLK_0;
        if (s->input.ctrl_down) {
            SDL_memset(s->selection.group_members[g], 0, sizeof(s->selection.group_members[g]));
            for (int i = 0; i < MAX_UNITS; i++) if (s->world.units.active[i] && s->selection.unit_selected[i]) s->selection.group_members[g][i] = true;
        } else {
            bool found = false;
            if (!s->input.shift_down) SDL_memset(s->selection.unit_selected, 0, sizeof(s->selection.unit_selected));
            for (int i = 0; i < MAX_UNITS; i++) if (s->world.units.active[i] && s->selection.group_members[g][i]) { s->selection.unit_selected[i] = true; s->selection.primary_unit_idx = i; found = true; }
            if (found) s->ui.menu_state = 0;
        }
    }

    if (key == SDLK_Q) {
        s->input.key_q_down = true;
        if (s->ui.menu_state == 1) {
            for (int i = 0; i < MAX_UNITS; i++) if (s->world.units.active[i] && s->selection.unit_selected[i] && s->world.units.type[i] == UNIT_MOTHERSHIP) {
                if (s->world.units.production_mode[i] == UNIT_MINER) {
                    s->world.units.production_mode[i] = UNIT_TYPE_COUNT;
                    UI_SetError(s, "MINER PRODUCTION OFF");
                } else {
                    s->world.units.production_mode[i] = UNIT_MINER;
                    UI_SetError(s, "MINER PRODUCTION ON");
                }
                s->world.units.production_timer[i] = 0.0f;
            }
        } else {
            s->input.pending_cmd_type = CMD_PATROL;
        }
    }
    if (key == SDLK_W) {
        s->input.key_w_down = true;
        if (s->ui.menu_state == 1) {
            for (int i = 0; i < MAX_UNITS; i++) if (s->world.units.active[i] && s->selection.unit_selected[i] && s->world.units.type[i] == UNIT_MOTHERSHIP) {
                if (s->world.units.production_mode[i] == UNIT_FIGHTER) {
                    s->world.units.production_mode[i] = UNIT_TYPE_COUNT;
                    UI_SetError(s, "FIGHTER PRODUCTION OFF");
                } else {
                    s->world.units.production_mode[i] = UNIT_FIGHTER;
                    UI_SetError(s, "FIGHTER PRODUCTION ON");
                }
                s->world.units.production_timer[i] = 0.0f;
            }
        } else {
            s->input.pending_cmd_type = CMD_MOVE;
        }
    }
    if (key == SDLK_E) { s->input.key_e_down = true; s->input.pending_cmd_type = CMD_ATTACK_MOVE; }
    if (key == SDLK_Y) { s->input.key_y_down = true; s->input.pending_cmd_type = CMD_MAIN_CANNON; }
    
    if (key == SDLK_R) {
        s->input.key_r_down = true;
        for (int i = 0; i < MAX_UNITS; i++) if (s->world.units.active[i] && s->selection.unit_selected[i]) { 
            s->world.units.velocity[i] = (Vec2){0,0}; s->world.units.has_target[i] = false; 
            s->world.units.command_count[i] = 0; s->world.units.command_current_idx[i] = 0; 
        }
        s->ui.hold_flash_timer = 0.2f;
    }
    if (key == SDLK_A) { s->input.key_a_down = true; for (int i = 0; i < MAX_UNITS; i++) if (s->world.units.active[i] && s->selection.unit_selected[i]) s->world.units.behavior[i] = BEHAVIOR_OFFENSIVE; s->ui.tactical_flash_timer = 0.2f; }
    if (key == SDLK_S) { s->input.key_s_down = true; for (int i = 0; i < MAX_UNITS; i++) if (s->world.units.active[i] && s->selection.unit_selected[i]) s->world.units.behavior[i] = BEHAVIOR_DEFENSIVE; s->ui.tactical_flash_timer = 0.2f; }
    if (key == SDLK_D) { s->input.key_d_down = true; for (int i = 0; i < MAX_UNITS; i++) if (s->world.units.active[i] && s->selection.unit_selected[i]) s->world.units.behavior[i] = BEHAVIOR_HOLD_GROUND; s->ui.tactical_flash_timer = 0.2f; }
    
    // Quick Selection
    if (key == SDLK_F1) { // Miners
        if (!s->input.shift_down) SDL_memset(s->selection.unit_selected, 0, sizeof(s->selection.unit_selected));
        for (int i = 0; i < MAX_UNITS; i++) if (s->world.units.active[i] && s->world.units.type[i] == UNIT_MINER) { s->selection.unit_selected[i] = true; s->selection.primary_unit_idx = i; }
        s->ui.menu_state = 0;
    }
    if (key == SDLK_F2) { // Fighters
        if (!s->input.shift_down) SDL_memset(s->selection.unit_selected, 0, sizeof(s->selection.unit_selected));
        for (int i = 0; i < MAX_UNITS; i++) if (s->world.units.active[i] && s->world.units.type[i] == UNIT_FIGHTER) { s->selection.unit_selected[i] = true; s->selection.primary_unit_idx = i; }
        s->ui.menu_state = 0;
    }
    if (key == SDLK_F3) { // All Units
        if (!s->input.shift_down) SDL_memset(s->selection.unit_selected, 0, sizeof(s->selection.unit_selected));
        int m_idx = -1;
        for (int i = 0; i < MAX_UNITS; i++) {
            if (s->world.units.active[i]) {
                s->selection.unit_selected[i] = true;
                if (s->world.units.type[i] == UNIT_MOTHERSHIP) m_idx = i;
                else if (m_idx == -1) s->selection.primary_unit_idx = i;
            }
        }
        if (m_idx != -1) s->selection.primary_unit_idx = m_idx;
        s->ui.menu_state = 0;
    }

    if (key == SDLK_F) {
        int m_idx = -1;
        for (int i = 0; i < MAX_UNITS; i++) if (s->world.units.active[i] && s->world.units.type[i] == UNIT_MOTHERSHIP) { m_idx = i; break; }
        if (m_idx != -1) {
            if (!s->input.shift_down) SDL_memset(s->selection.unit_selected, 0, sizeof(s->selection.unit_selected));
            s->selection.unit_selected[m_idx] = true;
            s->selection.primary_unit_idx = m_idx;
            s->ui.menu_state = 0;
        }
    }

    if (key == SDLK_X) {
        s->input.key_x_down = true;
        if (s->ui.menu_state == 0) {
            bool has_mothership = false;
            for (int i = 0; i < MAX_UNITS; i++) if (s->world.units.active[i] && s->selection.unit_selected[i] && s->world.units.type[i] == UNIT_MOTHERSHIP) { has_mothership = true; break; }
            if (has_mothership) s->ui.menu_state = 1;
        } else if (s->ui.menu_state == 1) {
            s->ui.menu_state = 0;
        }
    }
    
    if (key == SDLK_V) {
        for (int i = 0; i < MAX_UNITS; i++) {
            if (s->world.units.active[i] && s->selection.unit_selected[i] && s->world.units.type[i] == UNIT_MINER) {
                int m_idx = -1; float min_d = 1e18;
                for (int j = 0; j < MAX_UNITS; j++) {
                    if (s->world.units.active[j] && s->world.units.type[j] == UNIT_MOTHERSHIP) {
                        float d = Vector_DistanceSq(s->world.units.pos[i], s->world.units.pos[j]);
                        if (d < min_d) { min_d = d; m_idx = j; }
                    }
                }
                if (m_idx != -1) {
                    Command cmd = { .pos = s->world.units.pos[m_idx], .target_idx = m_idx, .type = CMD_RETURN_CARGO };
                    Input_ScheduleCommand(s, cmd, s->input.shift_down);
                }
            }
        }
    }
    
    if (key == SDLK_C) s->input.key_c_down = true;
    if (key == SDLK_G) s->input.show_grid = !s->input.show_grid;
    if (key == SDLK_D) s->input.show_density = !s->input.show_density;
    if (key == SDLK_K) { if (Persistence_SaveGame(s, "savegame.dat")) { UI_SetError(s, "GAME SAVED"); } else { UI_SetError(s, "SAVE FAILED"); } }
    if (key == SDLK_L) { if (Persistence_LoadGame(s, "savegame.dat")) { UI_SetError(s, "GAME LOADED"); } else { UI_SetError(s, "LOAD FAILED"); } }
}

void Input_HandleKeyUp(AppState *s, SDL_KeyboardEvent *event) {
    SDL_Keycode key = event->key;
    if (key == SDLK_LSHIFT || key == SDLK_RSHIFT) s->input.shift_down = false;
    if (key == SDLK_LCTRL || key == SDLK_RCTRL) s->input.ctrl_down = false;
    if (key == SDLK_Q) s->input.key_q_down = false;
    if (key == SDLK_W) s->input.key_w_down = false;
    if (key == SDLK_E) s->input.key_e_down = false;
    if (key == SDLK_R) s->input.key_r_down = false;
    if (key == SDLK_A) s->input.key_a_down = false;
    if (key == SDLK_S) s->input.key_s_down = false;
    if (key == SDLK_D) s->input.key_d_down = false;
    if (key == SDLK_Y) s->input.key_y_down = false;
    if (key == SDLK_X) s->input.key_x_down = false;
    if (key == SDLK_C) s->input.key_c_down = false;
}

void Input_ProcessEvent(AppState *s, SDL_Event *event) {
    switch (event->type) {
    case SDL_EVENT_MOUSE_WHEEL:
        Input_HandleMouseWheel(s, &event->wheel);
        break;
    case SDL_EVENT_MOUSE_MOTION:
        Input_HandleMouseMove(s, &event->motion);
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        Input_HandleMouseButtonDown(s, &event->button);
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        Input_HandleMouseButtonUp(s, &event->button);
        break;
    case SDL_EVENT_KEY_DOWN:
        Input_HandleKeyDown(s, &event->key);
        break;
    case SDL_EVENT_KEY_UP:
        Input_HandleKeyUp(s, &event->key);
        break;
    }
}