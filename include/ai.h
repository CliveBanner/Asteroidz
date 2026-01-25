#ifndef AI_H
#define AI_H

#include "structs.h"

void AI_StartThreads(AppState *s);
void AI_UpdateUnitMovement(AppState *s, int unit_idx, float dt);

#endif
