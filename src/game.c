#include "game.h"
#include "constants.h"

void Game_Update(AppState *s, float dt) {
    float move_speed = CAMERA_SPEED / s->zoom;
    
    bool scroll_left = s->mouse_pos.x < EDGE_SCROLL_THRESHOLD;
    bool scroll_right = s->mouse_pos.x > (WINDOW_WIDTH - EDGE_SCROLL_THRESHOLD);
    bool scroll_up = s->mouse_pos.y < EDGE_SCROLL_THRESHOLD;
    bool scroll_down = s->mouse_pos.y > (WINDOW_HEIGHT - EDGE_SCROLL_THRESHOLD);

    if (scroll_up)    s->camera_pos.y -= move_speed * dt;
    if (scroll_down)  s->camera_pos.y += move_speed * dt;
    if (scroll_left)  s->camera_pos.x -= move_speed * dt;
    if (scroll_right) s->camera_pos.x += move_speed * dt;
}