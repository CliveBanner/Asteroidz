#include "weapons.h"
#include "constants.h"
#include "particles.h"
#include "utils.h"
#include <math.h>

void Weapons_Fire(AppState *s, Unit *u, int asteroid_idx, float damage, float energy_cost, bool is_main_cannon) {
    if (is_main_cannon) {
        // Main cannon is free (uses cooldown)
    } else {
        if (s->world.energy < energy_cost) return;
        s->world.energy -= energy_cost;
    }
    Asteroid *a = &s->world.asteroids[asteroid_idx];
    a->health -= damage;
    a->radius -= (damage / ASTEROID_HEALTH_MULT) * 0.2f;
    if (a->health <= 0 || a->radius < ASTEROID_MIN_RADIUS) {
        a->active = false;
        s->world.asteroid_count--;
        Particles_SpawnExplosion(s, a->pos, 40, a->max_health / 1000.0f, EXPLOSION_COLLISION, a->tex_idx);
    }
    
    float dx = a->pos.x - u->pos.x;
    float dy = a->pos.y - u->pos.y;
    float dist = sqrtf(dx * dx + dy * dy);
    Vec2 start_pos = u->pos;
    Vec2 impact_pos = a->pos;

    if (dist > 0.1f) {
        float unit_r = u->stats->radius * MOTHERSHIP_VISUAL_SCALE * (LASER_START_OFFSET_MULT * 0.1f); 
        float ast_r = a->radius * ASTEROID_CORE_SCALE * 0.5f;
        start_pos.x += (dx / dist) * unit_r;
        start_pos.y += (dy / dist) * unit_r;
        impact_pos.x -= (dx / dist) * ast_r;
        impact_pos.y -= (dy / dist) * ast_r;
    }

    int p_idx = s->world.particle_next_idx;
    s->world.particles[p_idx].active = true;
    s->world.particles[p_idx].type = PARTICLE_TRACER;
    s->world.particles[p_idx].pos = start_pos;
    s->world.particles[p_idx].target_pos = impact_pos;
    s->world.particles[p_idx].life = 1.0f; 
    s->world.particles[p_idx].size = 1.0f + (damage / 50.0f);
    s->world.particles[p_idx].color = (SDL_Color)COLOR_LASER_RED; 
    s->world.particle_next_idx = (s->world.particle_next_idx + 1) % MAX_PARTICLES;

    // Muzzle flash
    Particles_SpawnLaserFlash(s, start_pos, s->world.particles[p_idx].size, false);

    // Impact Effect
    float impact_scale = 0.5f + (damage / 2000.0f);
    Particles_SpawnExplosion(s, impact_pos, 15, impact_scale, EXPLOSION_IMPACT, a->tex_idx); 
    
    // Impact flash
    Particles_SpawnLaserFlash(s, impact_pos, s->world.particles[p_idx].size, true);
}
