#ifndef UI_H
#define UI_H

#include "structs.h"

void UI_DrawLauncher(AppState *s);
void UI_DrawHUD(AppState *s);
void UI_SetError(AppState *s, const char *msg);

#endif
