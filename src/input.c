#include "game.h"
#include "constants.h"

void Input_ProcessEvent(AppState *s, SDL_Event *event) {
    switch (event->type) {
        case SDL_EVENT_MOUSE_WHEEL:
            if (event->wheel.y > 0) s->zoom *= 1.1f;
            if (event->wheel.y < 0) s->zoom /= 1.1f;
            
            // Clamp zoom
            if (s->zoom < MIN_ZOOM) s->zoom = MIN_ZOOM;
            if (s->zoom > MAX_ZOOM) s->zoom = MAX_ZOOM;
            break;

        case SDL_EVENT_MOUSE_MOTION:
            s->mouse_pos.x = event->motion.x;
            s->mouse_pos.y = event->motion.y;
            break;
    }
}
