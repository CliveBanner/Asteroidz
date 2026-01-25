#ifndef PHYSICS_H
#define PHYSICS_H

#include "structs.h"

// Updates position, velocity, and rotation for all active asteroids
// Applies gravity from celestial bodies
void Physics_UpdateAsteroids(AppState *s, float dt);
void Physics_UpdateResources(AppState *s, float dt);
void Physics_HandleCollisions(AppState *s, float dt);
void Physics_AreaDamage(AppState *s, Vec2 pos, float range, float damage, int exclude_unit_idx);

#endif
