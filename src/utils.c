#include "game.h"
#include <math.h>

Vec2 Vector_Sub(Vec2 a, Vec2 b) {
    return (Vec2){ a.x - b.x, a.y - b.y };
}

float Vector_Length(Vec2 v) {
    return sqrtf(v.x * v.x + v.y * v.y);
}

Vec2 Vector_Normalize(Vec2 v) {
    float len = Vector_Length(v);
    if (len == 0) return (Vec2){ 0, 0 };
    return (Vec2){ v.x / len, v.y / len };
}
