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
    fwrite(&s->camera.pos, sizeof(Vec2), 1, f);
    fwrite(&s->camera.zoom, sizeof(float), 1, f);
    fwrite(&s->world.energy, sizeof(float), 1, f);
    fwrite(&s->current_time, sizeof(float), 1, f);

    // 2. Entities
    fwrite(&s->world.units, sizeof(UnitPool), 1, f);
    fwrite(&s->world.asteroids, sizeof(AsteroidPool), 1, f);

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
    fread(&s->camera.pos, sizeof(Vec2), 1, f);
    fread(&s->camera.zoom, sizeof(float), 1, f);
    fread(&s->world.energy, sizeof(float), 1, f);
    fread(&s->current_time, sizeof(float), 1, f);

    // 2. Entities
    fread(&s->world.units, sizeof(UnitPool), 1, f);
    fread(&s->world.asteroids, sizeof(AsteroidPool), 1, f);

    // Re-link pointers
    for (int i = 0; i < MAX_UNITS; i++) {
        if (s->world.units.active[i]) {
            s->world.units.stats[i] = &s->world.unit_stats[s->world.units.type[i]];
        }
    }

    s->selection.primary_unit_idx = -1;
    for (int i = 0; i < MAX_UNITS; i++) s->selection.unit_selected[i] = false;
    s->selection.box_active = false;
    s->input.pending_input_type = INPUT_NONE;

    fclose(f);
    return true;
}
