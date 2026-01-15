#ifndef ASSETS_H
#define ASSETS_H

#include "structs.h"

void Asset_GenerateStep(AppState *s);
void Asset_DrawLoading(AppState *s);

// Procedural generation functions (internal to assets.c but exposed for potential testing or specific use)
void DrawPlanetToBuffer(Uint32 *pixels, int size, float seed);
void DrawGalaxyToBuffer(Uint32 *pixels, int size, float seed);
void DrawAsteroidToBuffer(Uint32 *pixels, int size, float seed);
void DrawMothershipToBuffer(Uint32 *pixels, int size, float seed);
void DrawExplosionPuffToBuffer(Uint32 *pixels, int size, float seed);
void DrawDebrisToBuffer(Uint32 *pixels, int size, float seed);

#endif
