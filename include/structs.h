#ifndef STRUCTS_H
#define STRUCTS_H

#include <SDL3/SDL.h>
#include <stdbool.h>
#include "constants.h"

typedef struct {
    float x, y;
} Vec2;

typedef enum {
    STATE_LAUNCHER,
    STATE_GAME,
    STATE_PAUSED,
    STATE_GAMEOVER
} GameState;

typedef enum {
    UNIT_MOTHERSHIP,
    UNIT_SCOUT,
    UNIT_TYPE_COUNT
} UnitType;

typedef struct {
    float max_health;
    float max_energy;
    float speed;
    float friction;
    float radius;
    float radar_range;
    
    // Weapon stats
    float main_cannon_damage;
    float main_cannon_range;
    float main_cannon_cooldown;
    
    float small_cannon_damage;
    float small_cannon_range;
    float small_cannon_cooldown;
    float small_cannon_energy_cost;
} UnitStats;

typedef struct {
    Vec2 pos;
} RadarBlip;

typedef enum {
    INPUT_NONE,
    INPUT_POSITION,
    INPUT_TARGET
} AbilityInputType;

typedef enum {
    CMD_MOVE,
    CMD_ATTACK_MOVE,
    CMD_PATROL,
    CMD_HOLD,
    CMD_MAIN_CANNON,
    CMD_TOGGLE_ATTACK // Passive toggle
} CommandType;

typedef struct {
    Vec2 pos;
    int target_idx;
    CommandType type;
} Command;

typedef struct {
    Vec2 pos;
    Vec2 velocity;
    float rotation;
    float health;
    float energy;
    
    UnitType type;
    const UnitStats *stats;
    bool active;

    // Weapons state
    float large_cannon_cooldown;
    float small_cannon_cooldown[4];
    int large_target_idx;
    int small_target_idx[4];

    // Movement/Commands
    Command command_queue[MAX_COMMANDS];
    int command_count;
    int command_current_idx;
    bool has_target;
    
    // AI State
    Vec2 patrol_start;
    bool patrolling_back;
} Unit;

typedef struct {
    int selected_res_index; // 0: 1280x720, 1: 1920x1080
    bool fullscreen;
    bool start_hovered;
    bool res_hovered;
    bool fs_hovered;
} LauncherState;

typedef enum {
    PARTICLE_SPARK,
    PARTICLE_PUFF,
    PARTICLE_GLOW,
    PARTICLE_TRACER,
    PARTICLE_DEBRIS,
    PARTICLE_SHOCKWAVE
} ParticleType;

typedef enum {
    EXPLOSION_IMPACT,
    EXPLOSION_COLLISION
} ExplosionType;

typedef struct {
    Vec2 pos;
    Vec2 velocity;
    float life;       // 1.0 down to 0.0
    float size;
    float rotation;
    int tex_idx;
    int asteroid_tex_idx; // New: to match debris to asteroid color
    SDL_Color color;
    ParticleType type;
    bool active;
    Vec2 target_pos; // For tracers
} Particle;

typedef struct {
    Vec2 pos;
    Vec2 velocity;
    float radius;
    float rotation;
    float rot_speed;
    float health;
    float max_health;
    int tex_idx;
    bool active;
    bool targeted;
} Asteroid;

typedef struct {
    Vec2 pos;
} SimAnchor;

typedef struct {
    // State
    GameState state;
    LauncherState launcher;

    // Camera
    Vec2 camera_pos;      // Top-left position of the camera in the world
    float zoom;
    
    // Simulation Anchors
    SimAnchor sim_anchors[MAX_SIM_ANCHORS];
    int sim_anchor_count;

    // Selection
    int selected_unit_idx; // Primary selected (last clicked)
    bool unit_selected[MAX_UNITS]; // Multiple selection
    
    // Box Selection
    bool box_active;
    Vec2 box_start; // Screen coordinates
    Vec2 box_current;

    // Command Mode
    bool auto_attack_enabled;
    bool patrol_mode;
    CommandType pending_cmd_type;
    AbilityInputType pending_input_type;

    // Game Entities
    Asteroid asteroids[MAX_ASTEROIDS];
    int asteroid_count;

    Unit units[MAX_UNITS];
    UnitStats unit_stats[UNIT_TYPE_COUNT];
    int unit_count;

    Particle particles[MAX_PARTICLES];
    int particle_next_idx;

    // Input State
    Vec2 mouse_pos;       // Screen coordinates
    bool show_grid;
    bool show_density;
    bool shift_down;
    bool key_q_down, key_w_down, key_e_down, key_r_down, key_h_down;
    
    // Resources
    float energy; // Global energy pool

    // Rendering
    SDL_Texture *bg_texture;
    SDL_Texture *explosion_puff_texture;
    SDL_Texture *mothership_texture;
    int bg_w, bg_h;
    float current_fps;
    float current_time;

    SDL_Texture *planet_textures[PLANET_COUNT];
    SDL_Texture *galaxy_textures[GALAXY_COUNT];
    SDL_Texture *asteroid_textures[ASTEROID_TYPE_COUNT];
    SDL_Texture *debris_textures[DEBRIS_COUNT];
    
    bool is_loading;
    int assets_generated;

    // UI
    float respawn_timer;
    Vec2 respawn_pos;
    float hold_flash_timer;
    float auto_attack_flash_timer;
    char ui_error_msg[128];
    float ui_error_timer;

    // Background Threading (Nebula)
    SDL_Thread *bg_thread;
    SDL_Mutex *bg_mutex;
    SDL_AtomicInt bg_should_quit;
    SDL_AtomicInt bg_request_update;
    SDL_AtomicInt bg_data_ready;
    
    Uint32 *bg_pixel_buffer;
    Vec2 bg_target_cam_pos;
    float bg_target_zoom;
    float bg_target_time;

    // Density Threading
    SDL_Thread *density_thread;
    SDL_Mutex *density_mutex;
    SDL_AtomicInt density_should_quit;
    SDL_AtomicInt density_request_update;
    SDL_AtomicInt density_data_ready;

    Uint32 *density_pixel_buffer;
    int density_w, density_h;
    Vec2 density_target_cam_pos;
    Vec2 density_texture_cam_pos; // Position of the currently uploaded texture
    SDL_Texture *density_texture;

    // Radar Threading
    SDL_Thread *radar_thread;
    SDL_Mutex *radar_mutex;
    SDL_AtomicInt radar_should_quit;
    SDL_AtomicInt radar_request_update;
    SDL_AtomicInt radar_data_ready;
    
    RadarBlip radar_blips[MAX_RADAR_BLIPS];
    int radar_blip_count;
    Vec2 radar_mothership_pos;

    // UnitFX & Targeting Threading
    SDL_Thread *unit_fx_thread;
    SDL_Mutex *unit_fx_mutex;
    SDL_AtomicInt unit_fx_should_quit;
    
    SDL_Texture *mothership_fx_texture; // Legacy/unused soon
    SDL_Texture *mothership_hull_texture;
    SDL_Texture *mothership_arm_texture;
    
    Uint32 *mothership_hull_buffer;
    Uint32 *mothership_arm_buffer;
    SDL_AtomicInt mothership_data_ready;
    
    int mothership_fx_size;
    Vec2 unit_fx_cam_pos; // Camera position at time of last successful FX generation

    SDL_Renderer *renderer;
    SDL_Window *window;
} AppState;

#endif
