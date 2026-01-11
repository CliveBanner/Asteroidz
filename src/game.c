#include "game.h"
#include "constants.h"

void Game_Update(AppState *s, float dt) {
    float move_speed = CAMERA_SPEED / s->zoom;
    
    int win_w, win_h;
    SDL_GetRenderOutputSize(s->renderer, &win_w, &win_h);

    bool scroll_left = s->mouse_pos.x < EDGE_SCROLL_THRESHOLD;
    bool scroll_right = s->mouse_pos.x > (win_w - EDGE_SCROLL_THRESHOLD);
    bool scroll_up = s->mouse_pos.y < EDGE_SCROLL_THRESHOLD;
    bool scroll_down = s->mouse_pos.y > (win_h - EDGE_SCROLL_THRESHOLD);

    if (scroll_up)    s->camera_pos.y -= move_speed * dt;
    if (scroll_down)  s->camera_pos.y += move_speed * dt;
    if (scroll_left)  s->camera_pos.x -= move_speed * dt;
    if (scroll_right) s->camera_pos.x += move_speed * dt;
}