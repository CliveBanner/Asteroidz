#include "constants.h"
#include "game.h"
#include <math.h>

void Input_ProcessEvent(AppState *s, SDL_Event *event) {
  switch (event->type) {
  case SDL_EVENT_MOUSE_WHEEL: {
    int win_w, win_h;
    SDL_GetRenderOutputSize(s->renderer, &win_w, &win_h);

    // 1. Calculate World Point at the center of the screen BEFORE zoom
    float center_world_x = s->camera_pos.x + (win_w / 2.0f) / s->zoom;
    float center_world_y = s->camera_pos.y + (win_h / 2.0f) / s->zoom;

    // 2. Apply Zoom
    if (event->wheel.y > 0)
      s->zoom *= ZOOM_STEP;
    if (event->wheel.y < 0)
      s->zoom /= ZOOM_STEP;

    // Clamp zoom
    if (s->zoom < MIN_ZOOM)
      s->zoom = MIN_ZOOM;
    if (s->zoom > MAX_ZOOM)
      s->zoom = MAX_ZOOM;

    // 3. Calculate new Camera Position to keep center fixed
    // NewCameraPos = CenterWorld - (ScreenCenter / NewZoom)
    s->camera_pos.x = center_world_x - (win_w / 2.0f) / s->zoom;
    s->camera_pos.y = center_world_y - (win_h / 2.0f) / s->zoom;
    break;
  }

  case SDL_EVENT_MOUSE_MOTION:
    s->mouse_pos.x = event->motion.x;
    s->mouse_pos.y = event->motion.y;
    if (s->box_active) {
        s->box_current = s->mouse_pos;
    }
    break;

  case SDL_EVENT_MOUSE_BUTTON_DOWN: {
    int win_w, win_h;
    SDL_GetRenderOutputSize(s->renderer, &win_w, &win_h);

    if (event->button.button == SDL_BUTTON_LEFT) {
      float mm_x = win_w - MINIMAP_SIZE - MINIMAP_MARGIN;
      float mm_y = win_h - MINIMAP_SIZE - MINIMAP_MARGIN;

      if (event->button.x >= mm_x && event->button.x <= mm_x + MINIMAP_SIZE &&
          event->button.y >= mm_y && event->button.y <= mm_y + MINIMAP_SIZE) {
        // Minimap click logic
        float rel_x = (event->button.x - mm_x) / MINIMAP_SIZE - 0.5f;
        float rel_y = (event->button.y - mm_y) / MINIMAP_SIZE - 0.5f;
        s->camera_pos.x += rel_x * MINIMAP_RANGE;
        s->camera_pos.y += rel_y * MINIMAP_RANGE;
      } else {
          // Start box selection
          s->box_active = true;
          s->box_start = (Vec2){event->button.x, event->button.y};
          s->box_current = s->box_start;
      }
    } else if (event->button.button == SDL_BUTTON_RIGHT) {
        float wx = s->camera_pos.x + (event->button.x) / s->zoom;
        float wy = s->camera_pos.y + (event->button.y) / s->zoom;
        
        for (int i = 0; i < MAX_UNITS; i++) {
            if (!s->units[i].active || !s->unit_selected[i]) continue;
            
            Unit *u = &s->units[i];
            
            // If active mode is Patrol, right click becomes a Patrol order
            CommandType click_type = s->patrol_mode ? CMD_PATROL : CMD_MOVE;

            Command cmd = { (Vec2){wx, wy}, click_type };
            
            if (s->shift_down) {
                if (u->command_count < MAX_COMMANDS) {
                    u->command_queue[u->command_count++] = cmd;
                    u->has_target = true;
                }
            } else {
                u->command_queue[0] = cmd;
                u->command_count = 1;
                u->command_current_idx = 0;
                u->has_target = true;
            }
        }
    }
    break;
  }

  case SDL_EVENT_MOUSE_BUTTON_UP: {
      if (event->button.button == SDL_BUTTON_LEFT && s->box_active) {
          s->box_active = false;
          
          float x1 = fminf(s->box_start.x, s->box_current.x);
          float y1 = fminf(s->box_start.y, s->box_current.y);
          float x2 = fmaxf(s->box_start.x, s->box_current.x);
          float y2 = fmaxf(s->box_start.y, s->box_current.y);
          
          // Selection logic
          bool any_selected = false;
          for (int i = 0; i < MAX_UNITS; i++) {
              if (!s->units[i].active) { s->unit_selected[i] = false; continue; }
              
              Vec2 sp = { (s->units[i].pos.x - s->camera_pos.x) * s->zoom, 
                          (s->units[i].pos.y - s->camera_pos.y) * s->zoom };
              
              if (sp.x >= x1 && sp.x <= x2 && sp.y >= y1 && sp.y <= y2) {
                  s->unit_selected[i] = true;
                  s->selected_unit_idx = i;
                  any_selected = true;
              } else if (!s->shift_down) {
                  s->unit_selected[i] = false;
              }
          }
          if (!any_selected && !s->shift_down) s->selected_unit_idx = -1;
      }
      break;
  }

  case SDL_EVENT_KEY_DOWN:
    if (event->key.key == SDLK_ESCAPE) {
        if (s->state == STATE_GAME) s->state = STATE_PAUSED;
        else if (s->state == STATE_PAUSED) s->state = STATE_GAME;
        break;
    }
    if (s->state == STATE_PAUSED && (event->key.key == SDLK_RETURN || event->key.key == SDLK_KP_ENTER)) {
        SDL_Event quit_event; quit_event.type = SDL_EVENT_QUIT; SDL_PushEvent(&quit_event);
        break;
    }
    if (s->state == STATE_PAUSED) break;

    if (event->key.key == SDLK_LSHIFT || event->key.key == SDLK_RSHIFT) {
        s->shift_down = true;
    }
    
    CommandType cmd_type = -1;
    if (event->key.key == SDLK_Q) {
        s->patrol_mode = !s->patrol_mode;
        if (s->patrol_mode) {
            // Convert existing waypoints to patrol
            for (int i = 0; i < MAX_UNITS; i++) {
                if (!s->units[i].active || !s->unit_selected[i]) continue;
                for (int q = 0; q < s->units[i].command_count; q++) s->units[i].command_queue[q].type = CMD_PATROL;
            }
        } else {
            // Convert back to move
            for (int i = 0; i < MAX_UNITS; i++) {
                if (!s->units[i].active || !s->unit_selected[i]) continue;
                for (int q = 0; q < s->units[i].command_count; q++) s->units[i].command_queue[q].type = CMD_MOVE;
            }
        }
    }
    if (event->key.key == SDLK_W) {
        s->patrol_mode = false;
        cmd_type = CMD_MOVE;
    }
    if (event->key.key == SDLK_E) {
        s->attack_mode = !s->attack_mode;
    }
    if (event->key.key == SDLK_R) {
        s->patrol_mode = false;
        cmd_type = CMD_HOLD;
        s->hold_flash_timer = 0.2f;
    }

    if (cmd_type != -1) {
        float wx = s->camera_pos.x + (s->mouse_pos.x) / s->zoom;
        float wy = s->camera_pos.y + (s->mouse_pos.y) / s->zoom;
        
        for (int i = 0; i < MAX_UNITS; i++) {
            if (!s->units[i].active || !s->unit_selected[i]) continue;
            Unit *u = &s->units[i];
            Command cmd = { (Vec2){wx, wy}, cmd_type };
            
            if (s->shift_down) {
                if (u->command_count < MAX_COMMANDS) {
                    u->command_queue[u->command_count++] = cmd;
                    u->has_target = true;
                }
            } else {
                u->command_queue[0] = cmd;
                u->command_count = 1;
                u->command_current_idx = 0;
                u->has_target = true;
            }
        }
    }

    if (event->key.key == SDLK_G) {
      s->show_grid = !s->show_grid;
    }
    if (event->key.key == SDLK_D) {
      s->show_density = !s->show_density;
    }
    break;

  case SDL_EVENT_KEY_UP:
    if (event->key.key == SDLK_LSHIFT || event->key.key == SDLK_RSHIFT) {
        s->shift_down = false;
    }
    break;
  }
}