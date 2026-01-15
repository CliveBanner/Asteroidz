#include "constants.h"
#include "game.h"
#include "persistence.h"
#include <math.h>
#include <stdio.h>

void Input_ProcessEvent(AppState *s, SDL_Event *event) {
  switch (event->type) {
  case SDL_EVENT_MOUSE_WHEEL: {
    int win_w, win_h;
    SDL_GetRenderOutputSize(s->renderer, &win_w, &win_h);

    // 1. Calculate World Point at the center of the screen BEFORE zoom
    float center_world_x = s->camera.pos.x + (win_w / 2.0f) / s->camera.zoom;
    float center_world_y = s->camera.pos.y + (win_h / 2.0f) / s->camera.zoom;

    // 2. Apply Zoom
    if (event->wheel.y > 0)
      s->camera.zoom *= ZOOM_STEP;
    if (event->wheel.y < 0)
      s->camera.zoom /= ZOOM_STEP;

    // Clamp zoom
    if (s->camera.zoom < MIN_ZOOM)
      s->camera.zoom = MIN_ZOOM;
    if (s->camera.zoom > MAX_ZOOM)
      s->camera.zoom = MAX_ZOOM;

    // 3. Calculate new Camera Position to keep center fixed
    // NewCameraPos = CenterWorld - (ScreenCenter / NewZoom)
    s->camera.pos.x = center_world_x - (win_w / 2.0f) / s->camera.zoom;
    s->camera.pos.y = center_world_y - (win_h / 2.0f) / s->camera.zoom;
    break;
  }

  case SDL_EVENT_MOUSE_MOTION:
    s->input.mouse_pos.x = event->motion.x;
    s->input.mouse_pos.y = event->motion.y;
    if (s->selection.box_active) {
        s->selection.box_current = s->input.mouse_pos;
    }
    break;

  case SDL_EVENT_MOUSE_BUTTON_DOWN: {
    int win_w, win_h;
    SDL_GetRenderOutputSize(s->renderer, &win_w, &win_h);

    float wx = s->camera.pos.x + (event->button.x) / s->camera.zoom;
    float wy = s->camera.pos.y + (event->button.y) / s->camera.zoom;

    if (event->button.button == SDL_BUTTON_LEFT) {
      float mm_x = win_w - MINIMAP_SIZE - MINIMAP_MARGIN;
      float mm_y = win_h - MINIMAP_SIZE - MINIMAP_MARGIN;

      if (event->button.x >= mm_x && event->button.x <= mm_x + MINIMAP_SIZE &&
          event->button.y >= mm_y && event->button.y <= mm_y + MINIMAP_SIZE) {
        // Minimap click
        float rel_x = (event->button.x - mm_x) / MINIMAP_SIZE - 0.5f;
        float rel_y = (event->button.y - mm_y) / MINIMAP_SIZE - 0.5f;
        s->camera.pos.x += rel_x * MINIMAP_RANGE;
        s->camera.pos.y += rel_y * MINIMAP_RANGE;
      } else {
          // Standard Selection (Always Left Click in RTS)
          bool clicked_unit = false;
          for (int i = 0; i < MAX_UNITS; i++) {
              if (!s->world.units[i].active) continue;
              float dx = s->world.units[i].pos.x - wx, dy = s->world.units[i].pos.y - wy;
              float r = s->world.units[i].stats->radius;
              if (dx*dx + dy*dy < r*r) {
                  if (!s->input.shift_down) SDL_memset(s->selection.unit_selected, 0, sizeof(s->selection.unit_selected));
                  s->selection.unit_selected[i] = true; s->selection.primary_unit_idx = i; clicked_unit = true; break;
              }
          }
          if (!clicked_unit) { s->selection.box_active = true; s->selection.box_start = (Vec2){event->button.x, event->button.y}; s->selection.box_current = s->selection.box_start; }
      }
    } else if (event->button.button == SDL_BUTTON_RIGHT) {
        // --- RTS COMMAND MODIFIER LOGIC ---
        int target_a = -1;
        for (int a = 0; a < MAX_ASTEROIDS; a++) {
            if (!s->world.asteroids[a].active) continue;
            float dx = s->world.asteroids[a].pos.x - wx, dy = s->world.asteroids[a].pos.y - wy;
            float r = s->world.asteroids[a].radius * ASTEROID_HITBOX_MULT;
            if (dx*dx + dy*dy < r * r) { target_a = a; break; }
        }

        CommandType type;
        if (s->input.key_q_down) type = CMD_PATROL;
        else if (s->input.key_w_down) type = CMD_MOVE;
        else if (s->input.key_e_down) type = CMD_ATTACK_MOVE;
        else if (s->input.key_z_down) type = CMD_MAIN_CANNON;
        else type = (target_a != -1) ? CMD_ATTACK_MOVE : CMD_MOVE;

        // Validation for target-based
        if (type == CMD_MAIN_CANNON && target_a == -1) {
            snprintf(s->ui.ui_error_msg, 128, "TARGET REQUIRED"); s->ui.ui_error_timer = 1.0f;
            return;
        }

        for (int i = 0; i < MAX_UNITS; i++) {
            if (!s->world.units[i].active || !s->selection.unit_selected[i]) continue;
            Unit *u = &s->world.units[i];
            
            if (type == CMD_MAIN_CANNON) {
                if (u->large_cannon_cooldown > 0) { snprintf(s->ui.ui_error_msg, 128, "MAIN CANNON COOLDOWN"); s->ui.ui_error_timer = 1.5f; continue; }
                if (u->type == UNIT_MOTHERSHIP && target_a != -1) u->large_target_idx = target_a;
                continue;
            }

            Command cmd = { .pos = {wx, wy}, .target_idx = target_a, .type = type };
            if (s->input.shift_down) {
                if (u->command_count < MAX_COMMANDS) { u->command_queue[u->command_count++] = cmd; u->has_target = true; }
            } else {
                u->command_queue[0] = cmd; u->command_count = 1; u->command_current_idx = 0; u->has_target = true;
                if (type == CMD_STOP) { 
                    u->velocity = (Vec2){0,0}; 
                    u->has_target = false; 
                    u->command_count = 0;
                    u->command_current_idx = 0;
                }
            }
        }
    }
    break;
  }

  case SDL_EVENT_MOUSE_BUTTON_UP: {
      if (event->button.button == SDL_BUTTON_LEFT && s->selection.box_active) {
          s->selection.box_active = false;
          
          float x1 = fminf(s->selection.box_start.x, s->selection.box_current.x);
          float y1 = fminf(s->selection.box_start.y, s->selection.box_current.y);
          float x2 = fmaxf(s->selection.box_start.x, s->selection.box_current.x);
          float y2 = fmaxf(s->selection.box_start.y, s->selection.box_current.y);
          
          // Selection logic
          bool any_selected = false;
          for (int i = 0; i < MAX_UNITS; i++) {
              if (!s->world.units[i].active) { s->selection.unit_selected[i] = false; continue; }
              
              Vec2 sp = { (s->world.units[i].pos.x - s->camera.pos.x) * s->camera.zoom, 
                          (s->world.units[i].pos.y - s->camera.pos.y) * s->camera.zoom };
              
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
      break;
  }

  case SDL_EVENT_KEY_DOWN:
    if (event->key.key == SDLK_ESCAPE) {
        if (s->game_state == STATE_GAME) s->game_state = STATE_PAUSED;
        else if (s->game_state == STATE_PAUSED) s->game_state = STATE_GAME;
        break;
    }
    if (s->game_state == STATE_PAUSED && (event->key.key == SDLK_RETURN || event->key.key == SDLK_KP_ENTER)) {
        SDL_Event quit_event; quit_event.type = SDL_EVENT_QUIT; SDL_PushEvent(&quit_event);
        break;
    }
    if (s->game_state == STATE_PAUSED) break;

        if (event->key.key == SDLK_LSHIFT || event->key.key == SDLK_RSHIFT) {
            s->input.shift_down = true;
        }
        
        if (event->key.key == SDLK_Q) s->input.key_q_down = true;
        if (event->key.key == SDLK_W) s->input.key_w_down = true;
        if (event->key.key == SDLK_E) s->input.key_e_down = true;
        if (event->key.key == SDLK_R) {
            s->input.key_r_down = true;
            for (int i = 0; i < MAX_UNITS; i++) {
                if (s->world.units[i].active && s->selection.unit_selected[i]) {
                    Unit *u = &s->world.units[i];
                    u->velocity = (Vec2){0,0};
                    u->has_target = false;
                    u->command_count = 0;
                    u->command_current_idx = 0;
                }
            }
            s->ui.hold_flash_timer = 0.2f;
        }

        if (event->key.key == SDLK_A) {
            s->input.key_a_down = true;
            for (int i = 0; i < MAX_UNITS; i++) if (s->world.units[i].active && s->selection.unit_selected[i]) s->world.units[i].behavior = BEHAVIOR_OFFENSIVE;
            s->ui.tactical_flash_timer = 0.2f;
        }
        if (event->key.key == SDLK_S) {
            s->input.key_s_down = true;
            for (int i = 0; i < MAX_UNITS; i++) if (s->world.units[i].active && s->selection.unit_selected[i]) s->world.units[i].behavior = BEHAVIOR_DEFENSIVE;
            s->ui.tactical_flash_timer = 0.2f;
        }
        if (event->key.key == SDLK_D) {
            s->input.key_d_down = true;
            for (int i = 0; i < MAX_UNITS; i++) if (s->world.units[i].active && s->selection.unit_selected[i]) s->world.units[i].behavior = BEHAVIOR_HOLD_GROUND;
            s->ui.tactical_flash_timer = 0.2f;
        }
        if (event->key.key == SDLK_Z) s->input.key_z_down = true;
    
        if (event->key.key == SDLK_G) {
              s->input.show_grid = !s->input.show_grid;

        }

        if (event->key.key == SDLK_D) {

          s->input.show_density = !s->input.show_density;

        }

        if (event->key.key == SDLK_K) {
            if (Persistence_SaveGame(s, "savegame.dat")) {
                snprintf(s->ui.ui_error_msg, 128, "GAME SAVED"); s->ui.ui_error_timer = 1.5f;
            } else {
                snprintf(s->ui.ui_error_msg, 128, "SAVE FAILED"); s->ui.ui_error_timer = 1.5f;
            }
        }

        if (event->key.key == SDLK_L) {
            if (Persistence_LoadGame(s, "savegame.dat")) {
                snprintf(s->ui.ui_error_msg, 128, "GAME LOADED"); s->ui.ui_error_timer = 1.5f;
            } else {
                snprintf(s->ui.ui_error_msg, 128, "LOAD FAILED"); s->ui.ui_error_timer = 1.5f;
            }
        }

        break;

    

        case SDL_EVENT_KEY_UP:

    

          if (event->key.key == SDLK_LSHIFT || event->key.key == SDLK_RSHIFT) {

    

              s->input.shift_down = false;

    

              // RTS Requirement: Clear armed ability when shift is released

    

              s->input.pending_input_type = INPUT_NONE;

    

          }

    

                    if (event->key.key == SDLK_Q) s->input.key_q_down = false;

    

                    if (event->key.key == SDLK_W) s->input.key_w_down = false;

    

                    if (event->key.key == SDLK_E) s->input.key_e_down = false;

    

                    if (event->key.key == SDLK_R) s->input.key_r_down = false;

    

                    if (event->key.key == SDLK_A) s->input.key_a_down = false;

    

                              if (event->key.key == SDLK_S) s->input.key_s_down = false;

    

                              if (event->key.key == SDLK_D) s->input.key_d_down = false;

    

                              if (event->key.key == SDLK_Z) s->input.key_z_down = false;

    

          break;

    

      

      }

    }

    