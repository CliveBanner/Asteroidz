#include "game.h"
#include "constants.h"

void Input_ProcessEvent(AppState *s, SDL_Event *event) {
    switch (event->type) {
        case SDL_EVENT_MOUSE_WHEEL: {
            int win_w, win_h;
            SDL_GetRenderOutputSize(s->renderer, &win_w, &win_h);

            // 1. Calculate World Point at the center of the screen BEFORE zoom
            float center_world_x = s->camera_pos.x + (win_w / 2.0f) / s->zoom;
            float center_world_y = s->camera_pos.y + (win_h / 2.0f) / s->zoom;

            // 2. Apply Zoom
            if (event->wheel.y > 0) s->zoom *= 1.1f;
            if (event->wheel.y < 0) s->zoom /= 1.1f;
            
            // Clamp zoom
            if (s->zoom < MIN_ZOOM) s->zoom = MIN_ZOOM;
            if (s->zoom > MAX_ZOOM) s->zoom = MAX_ZOOM;

            // 3. Calculate new Camera Position to keep center fixed
            // NewCameraPos = CenterWorld - (ScreenCenter / NewZoom)
            s->camera_pos.x = center_world_x - (win_w / 2.0f) / s->zoom;
            s->camera_pos.y = center_world_y - (win_h / 2.0f) / s->zoom;
            break;
        }

        case SDL_EVENT_MOUSE_MOTION:
            s->mouse_pos.x = event->motion.x;
            s->mouse_pos.y = event->motion.y;
            break;

        case SDL_EVENT_KEY_DOWN:
            if (event->key.key == SDLK_H) {
                s->show_grid = !s->show_grid;
            }
            break;
    }
}