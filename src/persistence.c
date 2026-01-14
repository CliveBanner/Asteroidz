#include "persistence.h"
#include <stdio.h>
#include <stdlib.h>

#define SAVE_MAGIC 0x41535452 // "ASTR"
#define SAVE_VERSION 1

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t app_state_size;
    uint32_t unit_count;
    uint32_t asteroid_count;
} SaveHeader;

bool Persistence_SaveGame(const AppState *s, const char *filename) {
    FILE *f = fopen(filename, "wb");
    if (!f) return false;

    SaveHeader header = {
        .magic = SAVE_MAGIC,
        .version = SAVE_VERSION,
        .app_state_size = sizeof(AppState),
        .unit_count = MAX_UNITS,
        .asteroid_count = MAX_ASTEROIDS
    };

    if (fwrite(&header, sizeof(SaveHeader), 1, f) != 1) { fclose(f); return false; }

    // 1. Core State
    fwrite(&s->camera_pos, sizeof(Vec2), 1, f);
    fwrite(&s->zoom, sizeof(float), 1, f);
    fwrite(&s->energy, sizeof(float), 1, f);
    fwrite(&s->current_time, sizeof(float), 1, f);
    fwrite(&s->auto_attack_enabled, sizeof(bool), 1, f);

    // 2. Entities
    fwrite(s->units, sizeof(Unit), MAX_UNITS, f);
    fwrite(s->asteroids, sizeof(Asteroid), MAX_ASTEROIDS, f);

    fclose(f);
    return true;
}

bool Persistence_LoadGame(AppState *s, const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return false;

    SaveHeader header;
    if (fread(&header, sizeof(SaveHeader), 1, f) != 1) { fclose(f); return false; }

    if (header.magic != SAVE_MAGIC || header.version != SAVE_VERSION) { fclose(f); return false; }

    // 1. Core State
    fread(&s->camera_pos, sizeof(Vec2), 1, f);
    fread(&s->zoom, sizeof(float), 1, f);
    fread(&s->energy, sizeof(float), 1, f);
    fread(&s->current_time, sizeof(float), 1, f);
    fread(&s->auto_attack_enabled, sizeof(bool), 1, f);

    // 2. Entities
    fread(s->units, sizeof(Unit), MAX_UNITS, f);
    fread(s->asteroids, sizeof(Asteroid), MAX_ASTEROIDS, f);

    // Re-link pointers
    for (int i = 0; i < MAX_UNITS; i++) {
        if (s->units[i].active) {
            s->units[i].stats = &s->unit_stats[s->units[i].type];
        }
    }

    s->selected_unit_idx = -1;
    for (int i = 0; i < MAX_UNITS; i++) s->unit_selected[i] = false;
    s->box_active = false;
    s->pending_input_type = INPUT_NONE;

    fclose(f);
    return true;
}
