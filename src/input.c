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
    float center_world_x = s->camera.pos.x + (win_w / 2.0f) / s->camera.zoom;
    float center_world_y = s->camera.pos.y + (win_h / 2.0f) / s->camera.zoom;
    if (event->wheel.y > 0) s->camera.zoom *= ZOOM_STEP;
    if (event->wheel.y < 0) s->camera.zoom /= ZOOM_STEP;
    if (s->camera.zoom < MIN_ZOOM) s->camera.zoom = MIN_ZOOM;
    if (s->camera.zoom > MAX_ZOOM) s->camera.zoom = MAX_ZOOM;
    s->camera.pos.x = center_world_x - (win_w / 2.0f) / s->camera.zoom;
    s->camera.pos.y = center_world_y - (win_h / 2.0f) / s->camera.zoom;
    break;
  }
  case SDL_EVENT_MOUSE_MOTION:
    s->input.mouse_pos.x = event->motion.x;
    s->input.mouse_pos.y = event->motion.y;
    if (s->selection.box_active) s->selection.box_current = s->input.mouse_pos;
    break;
  case SDL_EVENT_MOUSE_BUTTON_DOWN: {
    int win_w, win_h; SDL_GetRenderOutputSize(s->renderer, &win_w, &win_h);
    float wx = s->camera.pos.x + (event->button.x) / s->camera.zoom;
    float wy = s->camera.pos.y + (event->button.y) / s->camera.zoom;
    if (event->button.button == SDL_BUTTON_LEFT) {
      float mm_x = win_w - MINIMAP_SIZE - MINIMAP_MARGIN, mm_y = win_h - MINIMAP_SIZE - MINIMAP_MARGIN;
      if (event->button.x >= mm_x && event->button.x <= mm_x + MINIMAP_SIZE && event->button.y >= mm_y && event->button.y <= mm_y + MINIMAP_SIZE) {
        float rel_x = (event->button.x - mm_x) / MINIMAP_SIZE - 0.5f, rel_y = (event->button.y - mm_y) / MINIMAP_SIZE - 0.5f;
        s->camera.pos.x += rel_x * MINIMAP_RANGE; s->camera.pos.y += rel_y * MINIMAP_RANGE;
      } else {
          bool clicked_unit = false;
          for (int i = 0; i < MAX_UNITS; i++) {
              if (!s->world.units.active[i]) continue;
              float dx = s->world.units.pos[i].x - wx, dy = s->world.units.pos[i].y - wy, r = s->world.units.stats[i]->radius;
              if (dx*dx + dy*dy < r*r) {
                  if (!s->input.shift_down) SDL_memset(s->selection.unit_selected, 0, sizeof(s->selection.unit_selected));
                  s->selection.unit_selected[i] = true; s->selection.primary_unit_idx = i; clicked_unit = true; break;
              }
          }
          if (!clicked_unit) { s->selection.box_active = true; s->selection.box_start = (Vec2){event->button.x, event->button.y}; s->selection.box_current = s->selection.box_start; }
      }
    } else if (event->button.button == SDL_BUTTON_RIGHT) {
        int target_a = -1;
        for (int a = 0; a < MAX_ASTEROIDS; a++) {
            if (!s->world.asteroids.active[a]) continue;
            float dx = s->world.asteroids.pos[a].x - wx, dy = s->world.asteroids.pos[a].y - wy, r = s->world.asteroids.radius[a] * ASTEROID_HITBOX_MULT;
            if (dx*dx + dy*dy < r * r) { target_a = a; break; }
        }
        CommandType type;
        if (s->input.key_q_down) type = CMD_PATROL;
        else if (s->input.key_w_down) type = CMD_MOVE;
        else if (s->input.key_e_down) type = CMD_ATTACK_MOVE;
        else if (s->input.key_z_down) type = CMD_MAIN_CANNON;
        else if (s->input.hover_resource_idx != -1) { type = CMD_GATHER; target_a = s->input.hover_resource_idx; }
        else type = (target_a != -1) ? CMD_ATTACK_MOVE : CMD_MOVE;
        
        if (type == CMD_MAIN_CANNON && target_a == -1) { snprintf(s->ui.ui_error_msg, 128, "TARGET REQUIRED"); s->ui.ui_error_timer = 1.0f; return; }
        for (int i = 0; i < MAX_UNITS; i++) {
            if (!s->world.units.active[i] || !s->selection.unit_selected[i]) continue;
            if (type == CMD_MAIN_CANNON) {
                if (s->world.units.large_cannon_cooldown[i] > 0) { snprintf(s->ui.ui_error_msg, 128, "MAIN CANNON COOLDOWN"); s->ui.ui_error_timer = 1.5f; continue; }
                if (s->world.units.type[i] == UNIT_MOTHERSHIP && target_a != -1) s->world.units.large_target_idx[i] = target_a;
                continue;
            }
            if (type == CMD_GATHER) {
                if (s->world.units.current_cargo[i] >= s->world.units.stats[i]->max_cargo) {
                    snprintf(s->ui.ui_error_msg, 128, "CARGO FULL"); s->ui.ui_error_timer = 1.0f;
                    continue;
                }
            }
            Command cmd = { .pos = {wx, wy}, .target_idx = target_a, .type = type };
            if (s->input.shift_down) { if (s->world.units.command_count[i] < MAX_COMMANDS) { s->world.units.command_queue[i][s->world.units.command_count[i]++] = cmd; s->world.units.has_target[i] = true; } }
            else { s->world.units.command_queue[i][0] = cmd; s->world.units.command_count[i] = 1; s->world.units.command_current_idx[i] = 0; s->world.units.has_target[i] = true; }
        }
    }
    break;
  }
  case SDL_EVENT_MOUSE_BUTTON_UP: {
      if (event->button.button == SDL_BUTTON_LEFT && s->selection.box_active) {
          s->selection.box_active = false;
          float x1 = fminf(s->selection.box_start.x, s->selection.box_current.x), y1 = fminf(s->selection.box_start.y, s->selection.box_current.y), x2 = fmaxf(s->selection.box_start.x, s->selection.box_current.x), y2 = fmaxf(s->selection.box_start.y, s->selection.box_current.y);
          bool any_selected = false;
          for (int i = 0; i < MAX_UNITS; i++) {
              if (!s->world.units.active[i]) { s->selection.unit_selected[i] = false; continue; }
              Vec2 sp = { (s->world.units.pos[i].x - s->camera.pos.x) * s->camera.zoom, (s->world.units.pos[i].y - s->camera.pos.y) * s->camera.zoom };
              if (sp.x >= x1 && sp.x <= x2 && sp.y >= y1 && sp.y <= y2) { s->selection.unit_selected[i] = true; s->selection.primary_unit_idx = i; any_selected = true; }
              else if (!s->input.shift_down) s->selection.unit_selected[i] = false;
          }
          if (!any_selected && !s->input.shift_down) s->selection.primary_unit_idx = -1;
      }
      break;
  }
  case SDL_EVENT_KEY_DOWN:
    if (event->key.key == SDLK_ESCAPE) { if (s->game_state == STATE_GAME) s->game_state = STATE_PAUSED; else if (s->game_state == STATE_PAUSED) s->game_state = STATE_GAME; break; }
    if (s->game_state == STATE_PAUSED && (event->key.key == SDLK_RETURN || event->key.key == SDLK_KP_ENTER)) { SDL_Event qe; qe.type = SDL_EVENT_QUIT; SDL_PushEvent(&qe); break; }
    if (s->game_state == STATE_PAUSED) break;
    if (event->key.key == SDLK_LSHIFT || event->key.key == SDLK_RSHIFT) s->input.shift_down = true;
    if (event->key.key == SDLK_LCTRL || event->key.key == SDLK_RCTRL) s->input.ctrl_down = true;
    if (event->key.key >= SDLK_1 && event->key.key <= SDLK_9) {
        int g = event->key.key - SDLK_0;
        if (s->input.ctrl_down) {
            SDL_memset(s->selection.group_members[g], 0, sizeof(s->selection.group_members[g]));
            for (int i = 0; i < MAX_UNITS; i++) if (s->world.units.active[i] && s->selection.unit_selected[i]) s->selection.group_members[g][i] = true;
        } else {
            bool found = false;
            if (!s->input.shift_down) SDL_memset(s->selection.unit_selected, 0, sizeof(s->selection.unit_selected));
            for (int i = 0; i < MAX_UNITS; i++) if (s->world.units.active[i] && s->selection.group_members[g][i]) { s->selection.unit_selected[i] = true; s->selection.primary_unit_idx = i; found = true; }
        }
    }
    if (event->key.key == SDLK_Q) s->input.key_q_down = true;
    if (event->key.key == SDLK_W) s->input.key_w_down = true;
    if (event->key.key == SDLK_E) s->input.key_e_down = true;
    if (event->key.key == SDLK_R) {
        s->input.key_r_down = true;
        for (int i = 0; i < MAX_UNITS; i++) if (s->world.units.active[i] && s->selection.unit_selected[i]) { s->world.units.velocity[i] = (Vec2){0,0}; s->world.units.has_target[i] = false; s->world.units.command_count[i] = 0; s->world.units.command_current_idx[i] = 0; }
        s->ui.hold_flash_timer = 0.2f;
    }
    if (event->key.key == SDLK_A) { s->input.key_a_down = true; for (int i = 0; i < MAX_UNITS; i++) if (s->world.units.active[i] && s->selection.unit_selected[i]) s->world.units.behavior[i] = BEHAVIOR_OFFENSIVE; s->ui.tactical_flash_timer = 0.2f; }
    if (event->key.key == SDLK_S) { s->input.key_s_down = true; for (int i = 0; i < MAX_UNITS; i++) if (s->world.units.active[i] && s->selection.unit_selected[i]) s->world.units.behavior[i] = BEHAVIOR_DEFENSIVE; s->ui.tactical_flash_timer = 0.2f; }
    if (event->key.key == SDLK_D) { s->input.key_d_down = true; for (int i = 0; i < MAX_UNITS; i++) if (s->world.units.active[i] && s->selection.unit_selected[i]) s->world.units.behavior[i] = BEHAVIOR_HOLD_GROUND; s->ui.tactical_flash_timer = 0.2f; }
    if (event->key.key == SDLK_Z) s->input.key_z_down = true;
    if (event->key.key == SDLK_X) s->input.key_x_down = true;
    if (event->key.key == SDLK_C) s->input.key_c_down = true;
    if (event->key.key == SDLK_G) s->input.show_grid = !s->input.show_grid;
    if (event->key.key == SDLK_D) s->input.show_density = !s->input.show_density;
    if (event->key.key == SDLK_K) { if (Persistence_SaveGame(s, "savegame.dat")) { snprintf(s->ui.ui_error_msg, 128, "GAME SAVED"); s->ui.ui_error_timer = 1.5f; } else { snprintf(s->ui.ui_error_msg, 128, "SAVE FAILED"); s->ui.ui_error_timer = 1.5f; } }
    if (event->key.key == SDLK_L) { if (Persistence_LoadGame(s, "savegame.dat")) { snprintf(s->ui.ui_error_msg, 128, "GAME LOADED"); s->ui.ui_error_timer = 1.5f; } else { snprintf(s->ui.ui_error_msg, 128, "LOAD FAILED"); s->ui.ui_error_timer = 1.5f; } }
    break;
  case SDL_EVENT_KEY_UP:
    if (event->key.key == SDLK_LSHIFT || event->key.key == SDLK_RSHIFT) { s->input.shift_down = false; s->input.pending_input_type = INPUT_NONE; }
    if (event->key.key == SDLK_LCTRL || event->key.key == SDLK_RCTRL) s->input.ctrl_down = false;
    if (event->key.key == SDLK_Q) s->input.key_q_down = false;
    if (event->key.key == SDLK_W) s->input.key_w_down = false;
    if (event->key.key == SDLK_E) s->input.key_e_down = false;
    if (event->key.key == SDLK_R) s->input.key_r_down = false;
    if (event->key.key == SDLK_A) s->input.key_a_down = false;
    if (event->key.key == SDLK_S) s->input.key_s_down = false;
    if (event->key.key == SDLK_D) s->input.key_d_down = false;
        if (event->key.key == SDLK_Z) s->input.key_z_down = false;
        if (event->key.key == SDLK_X) s->input.key_x_down = false;
        if (event->key.key == SDLK_C) s->input.key_c_down = false;
        break;
      }
    }
    