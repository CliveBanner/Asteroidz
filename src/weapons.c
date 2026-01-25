#include "weapons.h"
#include "game.h"
#include "constants.h"
#include "particles.h"
#include "utils.h"
#include <math.h>
#include <stdlib.h>

void Weapons_Fire(AppState *s, int u_idx, int asteroid_idx, float damage, float energy_cost, bool is_main_cannon) {
    if (is_main_cannon) {
        // Main cannon is free (uses cooldown)
    } else {
        bool is_mothership = s->world.units.type[u_idx] == UNIT_MOTHERSHIP;
        if (is_mothership) {
            if (s->world.energy < energy_cost) return;
            s->world.energy -= energy_cost;
        } else {
            if (s->world.units.energy[u_idx] < energy_cost) return;
            s->world.units.energy[u_idx] -= energy_cost;
        }
    }
    
    s->world.asteroids.health[asteroid_idx] -= damage;
    s->world.asteroids.radius[asteroid_idx] -= (damage / ASTEROID_HEALTH_MULT) * 0.2f;
    if (s->world.asteroids.health[asteroid_idx] <= 0 || s->world.asteroids.radius[asteroid_idx] < ASTEROID_MIN_RADIUS) {
        Vec2 pos = s->world.asteroids.pos[asteroid_idx];
        float rad = s->world.asteroids.radius[asteroid_idx];
        int tex = s->world.asteroids.tex_idx[asteroid_idx];
        
        s->world.asteroids.active[asteroid_idx] = false;
        s->world.asteroid_count--;
        Particles_SpawnExplosion(s, pos, 40, s->world.asteroids.max_health[asteroid_idx] / 1000.0f, EXPLOSION_COLLISION, tex);

        // Spawn Crystal on destruction
        if (((float)rand() / (float)RAND_MAX) < 0.3f) { // 30% chance
            float c_rad = CRYSTAL_RADIUS_SMALL_MIN + ((float)rand() / (float)RAND_MAX) * CRYSTAL_RADIUS_SMALL_VARIANCE;
            float angle = ((float)rand() / (float)RAND_MAX) * 2.0f * 3.14159f;
            SpawnCrystal(s, pos, (Vec2){cosf(angle), sinf(angle)}, c_rad);

            // Explosion damage area around crystal spawn
            float blast_radius = c_rad * 2.0f;
            float blast_damage = c_rad * 5.0f; // Scale damage with crystal size
            
            // Damage Units
            for (int u = 0; u < MAX_UNITS; u++) {
                if (!s->world.units.active[u]) continue;
                float dsq = Vector_DistanceSq(pos, s->world.units.pos[u]);
                if (dsq < blast_radius * blast_radius) {
                    s->world.units.health[u] -= blast_damage * 0.5f; // Units take half damage
                }
            }
            // Damage other Asteroids
            for (int a = 0; a < MAX_ASTEROIDS; a++) {
                if (!s->world.asteroids.active[a] || a == asteroid_idx) continue;
                float dsq = Vector_DistanceSq(pos, s->world.asteroids.pos[a]);
                if (dsq < blast_radius * blast_radius) {
                    s->world.asteroids.health[a] -= blast_damage;
                }
            }
        }
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
    Particles_SpawnLaserFlash(s, start_pos, s->world.particles.size[p_idx], (SDL_Color)COLOR_LASER_RED, false);

    // Impact Effect
    float impact_scale = 0.5f + (damage / 2000.0f);
    Particles_SpawnExplosion(s, impact_pos, 15, impact_scale, EXPLOSION_IMPACT, s->world.asteroids.tex_idx[asteroid_idx]); 
    
    // Impact flash
    Particles_SpawnLaserFlash(s, impact_pos, s->world.particles.size[p_idx], (SDL_Color)COLOR_LASER_RED, true);
}

void Weapons_MineCrystal(AppState *s, int u_idx, int resource_idx, float amount) {
    s->world.resources.health[resource_idx] -= amount;
    
    // Scale down slower, proportional to health lost
    // Similar to asteroid logic: radius -= (damage / MULT) * 0.2
    s->world.resources.radius[resource_idx] -= (amount / 5.0f) * 0.15f; 
    if (s->world.resources.radius[resource_idx] < 20.0f) s->world.resources.radius[resource_idx] = 20.0f;
    
    if (s->world.resources.health[resource_idx] <= 0) {
        // Resource depleted
        Vec2 pos = s->world.resources.pos[resource_idx];
        s->world.resources.active[resource_idx] = false;
        s->world.resource_count--;
        
        // Final big explosion
        Particles_SpawnExplosion(s, pos, 30, 1.5f, EXPLOSION_COLLISION, 0); 
        // Use a bright cyan/white shockwave for crystals
        int sw_idx = s->world.particle_next_idx;
        s->world.particles.active[sw_idx] = true;
        s->world.particles.type[sw_idx] = PARTICLE_SHOCKWAVE;
        s->world.particles.pos[sw_idx] = pos;
        s->world.particles.velocity[sw_idx] = (Vec2){0,0};
        s->world.particles.life[sw_idx] = 0.6f;
        s->world.particles.size[sw_idx] = 100.0f;
        s->world.particles.color[sw_idx] = (SDL_Color){100, 255, 255, 255};
        s->world.particle_next_idx = (s->world.particle_next_idx + 1) % MAX_PARTICLES;
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

    SDL_Color mining_color = {50, 255, 200, 255};
    Particles_SpawnLaserFlash(s, start_pos, 2.0f, mining_color, false);
    Particles_SpawnLaserFlash(s, impact_pos, 2.0f, mining_color, false); // No white flash
    Particles_SpawnMiningEffect(s, impact_pos, s->world.units.pos[u_idx], amount / 10.0f);
}