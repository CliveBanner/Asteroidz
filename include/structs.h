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
    STATE_LOADING,
    STATE_GAME,
    STATE_PAUSED,
    STATE_GAMEOVER
} GameState;

typedef enum {
    UNIT_MOTHERSHIP,
    UNIT_SCOUT,
    UNIT_MINER,
    UNIT_FIGHTER,
    UNIT_TYPE_COUNT
} UnitType;

typedef struct {
    float max_health;
    float max_energy;
    float max_cargo;
    float speed;
    float friction;
    float radius;
    float radar_range;
    float visual_scale;
    float production_cost;
    float production_time;
    
    // Weapon stats
    float main_cannon_damage;
    float main_cannon_range;
    float main_cannon_cooldown;
    
    float small_cannon_damage;
    float small_cannon_range;
    float small_cannon_cooldown;
    float small_cannon_energy_cost;
    float laser_thickness;
    float laser_glow_mult;
    float laser_core_thickness_mult;
    float laser_start_offset_mult;
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
    BEHAVIOR_OFFENSIVE,
    BEHAVIOR_DEFENSIVE,
    BEHAVIOR_HOLD_GROUND,
    BEHAVIOR_PASSIVE
} TacticalBehavior;

typedef enum {
    CMD_MOVE,
    CMD_ATTACK_MOVE,
    CMD_PATROL,
    CMD_STOP,
    CMD_MAIN_CANNON,
    CMD_GATHER,
    CMD_RETURN_CARGO
} CommandType;

typedef struct {
    Vec2 pos;
    int target_idx;
    CommandType type;
} Command;

typedef struct {
    Vec2 pos[MAX_UNITS];
    Vec2 velocity[MAX_UNITS];
    float rotation[MAX_UNITS];
    float health[MAX_UNITS];
    float energy[MAX_UNITS];
    float current_cargo[MAX_UNITS];
    UnitType type[MAX_UNITS];
    const UnitStats *stats[MAX_UNITS];
    bool active[MAX_UNITS];
    float large_cannon_cooldown[MAX_UNITS];
    float small_cannon_cooldown[MAX_UNITS][4];
    float mining_cooldown[MAX_UNITS];
    int large_target_idx[MAX_UNITS];
    int small_target_idx[MAX_UNITS][4];
    Command command_queue[MAX_UNITS][MAX_COMMANDS];
    int command_count[MAX_UNITS];
    int command_current_idx[MAX_UNITS];
    bool has_target[MAX_UNITS];
    Vec2 patrol_start[MAX_UNITS];
    bool patrolling_back[MAX_UNITS];
    TacticalBehavior behavior[MAX_UNITS];
    
    // Production
    UnitType production_queue[MAX_UNITS][MAX_PRODUCTION_QUEUE];
    int production_count[MAX_UNITS];
    float production_timer[MAX_UNITS];
} UnitPool;

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
    Vec2 pos[MAX_PARTICLES];
    Vec2 velocity[MAX_PARTICLES];
    float life[MAX_PARTICLES];
    float size[MAX_PARTICLES];
    float rotation[MAX_PARTICLES];
    int tex_idx[MAX_PARTICLES];
    int asteroid_tex_idx[MAX_PARTICLES];
    SDL_Color color[MAX_PARTICLES];
    ParticleType type[MAX_PARTICLES];
    bool active[MAX_PARTICLES];
    Vec2 target_pos[MAX_PARTICLES];
    int unit_idx[MAX_PARTICLES];
} ParticlePool;

typedef struct {
    Vec2 pos[MAX_ASTEROIDS];
    Vec2 velocity[MAX_ASTEROIDS];
    float radius[MAX_ASTEROIDS];
    float rotation[MAX_ASTEROIDS];
    float rot_speed[MAX_ASTEROIDS];
    float health[MAX_ASTEROIDS];
    float max_health[MAX_ASTEROIDS];
    int tex_idx[MAX_ASTEROIDS];
    bool active[MAX_ASTEROIDS];
    bool targeted[MAX_ASTEROIDS];
} AsteroidPool;

typedef struct {
    Vec2 pos;
} SimAnchor;

// --- Sub-structs for AppState ---

typedef struct {
    Vec2 pos;
    float zoom;
} CameraState;

typedef struct {
    int primary_unit_idx;
    bool unit_selected[MAX_UNITS];
    bool box_active;
    Vec2 box_start;
    Vec2 box_current;

    // Control Groups
    bool group_members[10][MAX_UNITS]; // 1-9 are groups.
} SelectionState;

typedef struct {
    CommandType pending_cmd_type;
    AbilityInputType pending_input_type;
    Vec2 mouse_pos;
    int hover_asteroid_idx;
    int hover_resource_idx;
    bool show_grid;
    bool show_density;
    bool shift_down;
    bool ctrl_down;
    bool key_q_down, key_w_down, key_e_down, key_r_down, key_a_down, key_s_down, key_d_down, key_z_down, key_x_down, key_c_down;
} InputControlState;

typedef struct {
    Vec2 pos[MAX_RESOURCES];
    Vec2 velocity[MAX_RESOURCES];
    float radius[MAX_RESOURCES];
    float rotation[MAX_RESOURCES];
    float rot_speed[MAX_RESOURCES];
    float amount[MAX_RESOURCES];
    float health[MAX_RESOURCES];
    float max_health[MAX_RESOURCES];
    int tex_idx[MAX_RESOURCES];
    bool active[MAX_RESOURCES];
} ResourcePool;

typedef struct {
    AsteroidPool asteroids;
    int asteroid_count;
    UnitPool units;
    UnitStats unit_stats[UNIT_TYPE_COUNT];
    int unit_count;
    ParticlePool particles;
    int particle_next_idx;
    ResourcePool resources;
    int resource_count;
    SimAnchor sim_anchors[MAX_SIM_ANCHORS];
    int sim_anchor_count;
    float energy;
    float stored_resources;
} WorldState;

typedef struct {
    SDL_Texture *bg_texture;
    SDL_Texture *explosion_puff_texture;
    SDL_Texture *mothership_hull_texture;
    SDL_Texture *mothership_arm_texture;
    SDL_Texture *miner_texture;
    SDL_Texture *fighter_texture;
    SDL_Texture *icon_textures[ICON_COUNT];
    SDL_Texture *planet_textures[PLANET_COUNT];
    SDL_Texture *galaxy_textures[GALAXY_COUNT];
    SDL_Texture *asteroid_textures[ASTEROID_TYPE_COUNT];
    SDL_Texture *crystal_textures[8]; // 8 variations of crystals
    SDL_Texture *debris_textures[DEBRIS_COUNT];
    SDL_Texture *density_texture;
    int bg_w, bg_h;
    int mothership_fx_size;
} TextureState;

typedef struct {
    // Nebula
    SDL_Thread *bg_thread;
    SDL_Mutex *bg_mutex;
    SDL_AtomicInt bg_should_quit;
    SDL_AtomicInt bg_request_update;
    SDL_AtomicInt bg_data_ready;
    Uint32 *bg_pixel_buffer;
    Vec2 bg_target_cam_pos;
    float bg_target_zoom;
    float bg_target_time;

    // Density
    SDL_Thread *density_thread;
    SDL_Mutex *density_mutex;
    SDL_AtomicInt density_should_quit;
    SDL_AtomicInt density_request_update;
    SDL_AtomicInt density_data_ready;
    Uint32 *density_pixel_buffer;
    int density_w, density_h;
    Vec2 density_target_cam_pos;
    Vec2 density_texture_cam_pos;

    // Radar
    SDL_Thread *radar_thread;
    SDL_Mutex *radar_mutex;
    SDL_AtomicInt radar_should_quit;
    SDL_AtomicInt radar_request_update;
    SDL_AtomicInt radar_data_ready;
    RadarBlip radar_blips[MAX_RADAR_BLIPS];
    int radar_blip_count;
    Vec2 radar_mothership_pos;

    // UnitFX
    SDL_Thread *unit_fx_thread;
    SDL_Mutex *unit_fx_mutex;
    SDL_AtomicInt unit_fx_should_quit;
    Uint32 *mothership_hull_buffer;
    Uint32 *mothership_arm_buffer;
    SDL_AtomicInt mothership_data_ready;
    Vec2 unit_fx_cam_pos;
} ThreadState;

typedef struct {
    float val;
    char label[32];
    float life;
} Transaction;

typedef struct {
    float respawn_timer;
    Vec2 respawn_pos;
    float hold_flash_timer;
    float tactical_flash_timer;
    char ui_error_msg[128];
    float ui_error_timer;
    int menu_state; // 0: Main, 1: Build
    
    Transaction transaction_log[MAX_LOGS];
    float resource_accumulator;
    float resource_log_timer;
} UIState;

typedef struct {
    GameState game_state;
    LauncherState launcher;
    CameraState camera;
    SelectionState selection;
    InputControlState input;
    WorldState world;
    TextureState textures;
    ThreadState threads;
    UIState ui;

    int assets_generated;
    float current_fps;
    float current_time;

    SDL_Renderer *renderer;
    SDL_Window *window;
} AppState;

#endif
