#ifndef TERRAIN_H
#define TERRAIN_H

#include "raylib.h"
#include <stdbool.h>

void InitTerrain(void);
void DrawTerrain(bool wireframe);
void DrawTerrainChunkBoundaries(void);
void UnloadTerrain(void);
float SampleTerrainHeight(float worldX, float worldZ);

#endif