#ifndef TERRAIN_H
#define TERRAIN_H

#include "raylib.h"
#include <stdbool.h>

typedef enum {
    BIOME_PLAINS,
    BIOME_HILLS,
    BIOME_MOUNTAINS,
    BIOME_PEAKS,
    BIOME_DESERT,
    BIOME_STEPPES,
    BIOME_JUNGLE,
    BIOME_FOREST,
    BIOME_TUNDRA,
    BIOME_TAIGA,
    BIOME_ICY,
    BIOME_COUNT
} BiomeType;

const char *GetBiomeName(BiomeType b);

void InitTerrain(void);
void DrawTerrain(bool wireframe);
void DrawTerrainChunkBoundaries(void);
void UnloadTerrain(void);
float GetTerrainHeightAndBiome(float worldX, float worldZ, BiomeType *biome);
float GetTerrainHeight(float worldX, float worldZ);

#endif