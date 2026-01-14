#ifndef PERSISTENCE_H
#define PERSISTENCE_H

#include "structs.h"

bool Persistence_SaveGame(const AppState *s, const char *filename);
bool Persistence_LoadGame(AppState *s, const char *filename);

#endif
