#ifndef WEAPONS_H
#define WEAPONS_H

#include "structs.h"

void Weapons_Fire(AppState *s, Unit *u, int asteroid_idx, float damage, float energy_cost, bool is_main_cannon);

#endif
