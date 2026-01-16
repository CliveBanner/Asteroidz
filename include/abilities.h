#ifndef ABILITIES_H
#define ABILITIES_H

#include "structs.h"

void Abilities_Update(AppState *s, int unit_idx, float dt);
void Abilities_Mine(AppState *s, int unit_idx, int resource_idx, float dt);

#endif
