#ifndef PARTICLES_H
#define PARTICLES_H

#include "structs.h"

// Spawns a visual explosion effect at the given position
void Particles_SpawnExplosion(AppState *s, Vec2 pos, int count, float size_mult, ExplosionType type, int asteroid_tex_idx);

// Spawns a bright laser flash (muzzle or impact)
void Particles_SpawnLaserFlash(AppState *s, Vec2 pos, float size, SDL_Color color, bool is_impact);

// Spawns enhanced mining effects (sparks and resource stream)
void Particles_SpawnMiningEffect(AppState *s, Vec2 crystal_pos, Vec2 unit_pos, float intensity);

// Updates all active particles
void Particles_Update(AppState *s, float dt);

#endif