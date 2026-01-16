#ifndef WEAPONS_H
#define WEAPONS_H

#include "structs.h"

void Weapons_Fire(AppState *s, int unit_idx, int asteroid_idx, float damage, float energy_cost, bool is_main_cannon);
void Weapons_MineCrystal(AppState *s, int unit_idx, int resource_idx, float damage);

#endif
