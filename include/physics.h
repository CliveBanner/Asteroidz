#ifndef PHYSICS_H
#define PHYSICS_H

#include "structs.h"

// Updates position, velocity, and rotation for all active asteroids
// Applies gravity from celestial bodies
void Physics_UpdateAsteroids(AppState *s, float dt);

// Handles collisions between asteroids (bounce, split)
// and between asteroids and units (damage)
void Physics_HandleCollisions(AppState *s, float dt);

#endif
