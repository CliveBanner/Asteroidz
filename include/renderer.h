#ifndef RENDERER_H
#define RENDERER_H

#include "structs.h"

void Renderer_Init(AppState *s);
void Renderer_Draw(AppState *s);
void Renderer_DrawLauncher(AppState *s);
void Renderer_StartBackgroundThreads(AppState *s);

#endif
