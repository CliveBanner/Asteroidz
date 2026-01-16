#include "weapons.h"
#include "constants.h"
#include "particles.h"
#include "utils.h"
#include <math.h>

void Weapons_Fire(AppState *s, int u_idx, int asteroid_idx, float damage, float energy_cost, bool is_main_cannon) {
    if (is_main_cannon) {
        // Main cannon is free (uses cooldown)
    } else {
        if (s->world.energy < energy_cost) return;
        s->world.energy -= energy_cost;
    }
    
    s->world.asteroids.health[asteroid_idx] -= damage;
    s->world.asteroids.radius[asteroid_idx] -= (damage / ASTEROID_HEALTH_MULT) * 0.2f;
    if (s->world.asteroids.health[asteroid_idx] <= 0 || s->world.asteroids.radius[asteroid_idx] < ASTEROID_MIN_RADIUS) {
        s->world.asteroids.active[asteroid_idx] = false;
        s->world.asteroid_count--;
        Particles_SpawnExplosion(s, s->world.asteroids.pos[asteroid_idx], 40, s->world.asteroids.max_health[asteroid_idx] / 1000.0f, EXPLOSION_COLLISION, s->world.asteroids.tex_idx[asteroid_idx]);
    }
    
    float dx = s->world.asteroids.pos[asteroid_idx].x - s->world.units.pos[u_idx].x;
    float dy = s->world.asteroids.pos[asteroid_idx].y - s->world.units.pos[u_idx].y;
    float dist = sqrtf(dx * dx + dy * dy);
    Vec2 start_pos = s->world.units.pos[u_idx];
    Vec2 impact_pos = s->world.asteroids.pos[asteroid_idx];

    if (dist > 0.1f) {
        float unit_r = s->world.units.stats[u_idx]->radius * s->world.units.stats[u_idx]->visual_scale * (s->world.units.stats[u_idx]->laser_start_offset_mult * 0.1f); 
        float ast_r = s->world.asteroids.radius[asteroid_idx] * ASTEROID_CORE_SCALE * 0.5f;
        start_pos.x += (dx / dist) * unit_r;
        start_pos.y += (dy / dist) * unit_r;
        impact_pos.x -= (dx / dist) * ast_r;
        impact_pos.y -= (dy / dist) * ast_r;
    }

    int p_idx = s->world.particle_next_idx;
    s->world.particles.active[p_idx] = true;
    s->world.particles.type[p_idx] = PARTICLE_TRACER;
    s->world.particles.pos[p_idx] = start_pos;
    s->world.particles.target_pos[p_idx] = impact_pos;
    s->world.particles.unit_idx[p_idx] = u_idx;
    s->world.particles.life[p_idx] = 1.0f; 
    s->world.particles.size[p_idx] = 1.0f + (damage / 50.0f);
    s->world.particles.color[p_idx] = (SDL_Color)COLOR_LASER_RED; 
    s->world.particle_next_idx = (s->world.particle_next_idx + 1) % MAX_PARTICLES;

    // Muzzle flash
    Particles_SpawnLaserFlash(s, start_pos, s->world.particles.size[p_idx], false);

    // Impact Effect
    float impact_scale = 0.5f + (damage / 2000.0f);
    Particles_SpawnExplosion(s, impact_pos, 15, impact_scale, EXPLOSION_IMPACT, s->world.asteroids.tex_idx[asteroid_idx]); 
    
    // Impact flash
    Particles_SpawnLaserFlash(s, impact_pos, s->world.particles.size[p_idx], true);
}

void Weapons_MineCrystal(AppState *s, int u_idx, int resource_idx, float amount) {
    s->world.resources.health[resource_idx] -= amount;
    
    if (s->world.resources.health[resource_idx] <= 0) {
        // Resource depleted
        s->world.resources.active[resource_idx] = false;
        s->world.resource_count--;
        Particles_SpawnExplosion(s, s->world.resources.pos[resource_idx], 20, s->world.resources.radius[resource_idx] / 200.0f, EXPLOSION_COLLISION, s->world.resources.tex_idx[resource_idx]);
    }

    float dx = s->world.resources.pos[resource_idx].x - s->world.units.pos[u_idx].x;
    float dy = s->world.resources.pos[resource_idx].y - s->world.units.pos[u_idx].y;
    float dist = sqrtf(dx * dx + dy * dy);
    Vec2 start_pos = s->world.units.pos[u_idx];
    Vec2 impact_pos = s->world.resources.pos[resource_idx];

    if (dist > 0.1f) {
        float unit_r = s->world.units.stats[u_idx]->radius * s->world.units.stats[u_idx]->visual_scale * (s->world.units.stats[u_idx]->laser_start_offset_mult * 0.1f);
        float res_r = s->world.resources.radius[resource_idx] * 0.5f; // Visual radius roughly
        start_pos.x += (dx / dist) * unit_r;
        start_pos.y += (dy / dist) * unit_r;
        impact_pos.x -= (dx / dist) * res_r;
        impact_pos.y -= (dy / dist) * res_r;
    }

    int p_idx = s->world.particle_next_idx;
    s->world.particles.active[p_idx] = true;
    s->world.particles.type[p_idx] = PARTICLE_TRACER;
    s->world.particles.pos[p_idx] = start_pos;
    s->world.particles.target_pos[p_idx] = impact_pos;
    s->world.particles.unit_idx[p_idx] = u_idx;
    s->world.particles.life[p_idx] = 0.5f; // Shorter life for continuous beam look
    s->world.particles.size[p_idx] = 6.0f; // Increased width
    s->world.particles.color[p_idx] = (SDL_Color){50, 255, 200, 255}; // Cyan mining laser
    s->world.particle_next_idx = (s->world.particle_next_idx + 1) % MAX_PARTICLES;

    Particles_SpawnLaserFlash(s, start_pos, 4.0f, false);
    Particles_SpawnLaserFlash(s, impact_pos, 6.0f, true);
}