#include "terrain.h"
#include "raymath.h"
#include "rlgl.h"
#include "texture.h"
#include "noise.h"

#define CHUNK_GRID_W 32
#define CHUNK_GRID_H 32
#define CHUNK_RES    16
#define CHUNK_SIZE   20.0f
#define WORLD_SEED   1254

typedef struct {
    Mesh mesh;
    float *heights;
    Vector3 worldOffset;
} TerrainChunk;

static TerrainChunk chunks[CHUNK_GRID_W][CHUNK_GRID_H];
static Material planeMaterial = { 0 };
static Texture2D terrainTexture = { 0 };

// biomes

typedef enum {
    BIOME_PLAINS,
    BIOME_HILLS,
    BIOME_MOUNTAINS,
    BIOME_PEAKS,
    BIOME_DESERT,
    BIOME_COUNT
} BiomeType;

typedef struct {
    float frequency;
    float persistence;
    int   octaves;
    float heightScale;
    Color color;
} BiomeParams;

static const BiomeParams biomeTable[BIOME_COUNT] = {
    //freq      persist oct heightscale color
    { 0.02f,    0.6f,   2,  3.8f,       {75.0f, 235.0f, 75.0f}},    // plains
    { 0.03f,    0.7f,   3,  4.5f,       {25.0f, 250.0f, 0.0f}},      // hills
    { 0.005f,    0.95f,  6,  6.0f,      {125.0f, 125.0f, 155.0f}},      // mountains
    { 0.015f,    0.95f,  6,  8.0f,      {255.0f, 255.0f, 255.0f}},      // peaks
    { 0.02f,    0.6f,   2,  3.8f,       {222.0f, 222.0f, 0.0f}},      // desert
};

static const BiomeType biomeLookup[3][3] = {
    // dry              temperate         wet
    { BIOME_PEAKS,     BIOME_MOUNTAINS,  BIOME_HILLS  }, // cold
    { BIOME_PLAINS,    BIOME_HILLS,      BIOME_HILLS  }, // temperate
    { BIOME_DESERT,    BIOME_PLAINS,     BIOME_HILLS  }, // hot
};

//----------------------------------------------------------------------------------
// Noise
//----------------------------------------------------------------------------------

static float SampleHeightWithParams(float worldX, float worldZ, const BiomeParams *p)
{
    float total = 0.0f, amp = 1.0, freq = p->frequency;
    for (int o = 0; o < p->octaves; o++)
    {
        total += Noise2D(worldX * freq, worldZ * freq, WORLD_SEED) * amp;
        amp *= p->persistence;
        freq *= 2.0f;
    }
    return total * p->heightScale;
}

static float SampleTemperatureNoise(float worldX, float worldZ)
{
    const float freq = 0.006f;
    return Noise2D(worldX * freq + 1000.0f, worldZ * freq + 1000.0f, WORLD_SEED);
}

static float SampleMoistureNoise(float worldX, float worldZ)
{
    const float freq = 0.006f;
    return Noise2D(worldX * freq + 5000.0f, worldZ * freq + 5000.0f, WORLD_SEED);
}

// 

static int GetBucket(float value, int bucketCount)
{
    int b = (int)(value * bucketCount);
    if (b >= bucketCount) b = bucketCount - 1;
    if (b < 0) b = 0;
    return b;
}

float GetTerrainHeight(float worldX, float worldZ)
{
    float temp = SampleTemperatureNoise(worldX, worldZ);
    float moist = SampleMoistureNoise(worldX, worldZ);

    BiomeType biome = biomeLookup[GetBucket(temp, 3)][GetBucket(moist, 3)];
    return SampleHeightWithParams(worldX, worldZ, &biomeTable[biome]);
}

static BiomeType GetBiomeAt(float n)
{
    int i = (int)(n * BIOME_COUNT);
    if (i >= BIOME_COUNT) i = BIOME_COUNT - 1;
    if (i < 0) i = 0;
    return (BiomeType)i;
}

//----------------------------------------------------------------------------------
// Mesh generation
//----------------------------------------------------------------------------------
static void RecalculateNormals(Mesh *mesh, float *heights, int vertsPerSide, float cellSize)
{
    for (int z = 0; z < vertsPerSide; z++)
    {
        for (int x = 0; x < vertsPerSide; x++)
        {
            int i = z * vertsPerSide + x;

            float hL = heights[z * vertsPerSide + (x > 0 ? x - 1 : x)];
            float hR = heights[z * vertsPerSide + (x < vertsPerSide - 1 ? x + 1 : x)];
            float hD = heights[(z > 0 ? z - 1 : z) * vertsPerSide + x];
            float hU = heights[(z < vertsPerSide - 1 ? z + 1 : z) * vertsPerSide + x];

            Vector3 normal = { (hL - hR) / (2.0f * cellSize), 1.0f, (hD - hU) / (2.0f * cellSize) };
            normal = Vector3Normalize(normal);

            mesh->normals[i * 3 + 0] = normal.x;
            mesh->normals[i * 3 + 1] = normal.y;
            mesh->normals[i * 3 + 2] = normal.z;
        }
    }
}

static void GenerateChunk(int cx, int cz)
{
    TerrainChunk *chunk = &chunks[cx][cz];

    chunk->mesh = GenMeshPlane(CHUNK_SIZE, CHUNK_SIZE, CHUNK_RES, CHUNK_RES);

    int vertsPerSide = CHUNK_RES + 1;
    int vertexCount = vertsPerSide * vertsPerSide;
    chunk->heights = (float *)MemAlloc(vertexCount * sizeof(float));
    chunk->mesh.colors = (unsigned char *)MemAlloc(vertexCount * 4 * sizeof(unsigned char));

    chunk->worldOffset = (Vector3){
        cx * CHUNK_SIZE - (CHUNK_GRID_W * CHUNK_SIZE) / 2.0f + CHUNK_SIZE / 2.0f,
        0.0f,
        cz * CHUNK_SIZE - (CHUNK_GRID_H * CHUNK_SIZE) / 2.0f + CHUNK_SIZE / 2.0f
    };

    for (int i = 0; i < vertexCount; i++)
    {
        float localX = chunk->mesh.vertices[i * 3 + 0];
        float localZ = chunk->mesh.vertices[i * 3 + 2];

        float worldX = localX + chunk->worldOffset.x;
        float worldZ = localZ + chunk->worldOffset.z;

        float temp  = SampleTemperatureNoise(worldX, worldZ);
        float moist = SampleMoistureNoise(worldX, worldZ);
        BiomeType b = biomeLookup[GetBucket(temp, 3)][GetBucket(moist, 3)];

        Color c = biomeTable[b].color;
        chunk->mesh.colors[i*4+0] = c.r;
        chunk->mesh.colors[i*4+1] = c.g;
        chunk->mesh.colors[i*4+2] = c.b;
        chunk->mesh.colors[i*4+3] = 255;

        float h = GetTerrainHeight(worldX, worldZ);
        chunk->heights[i] = h;
        chunk->mesh.vertices[i * 3 + 1] = h;
    }

    RecalculateNormals(&chunk->mesh, chunk->heights, vertsPerSide, CHUNK_SIZE / (float)CHUNK_RES);

    if (chunk->mesh.vboId != NULL)
    {
        UpdateMeshBuffer(chunk->mesh, 0, chunk->mesh.vertices, vertexCount*3*sizeof(float), 0);
        UpdateMeshBuffer(chunk->mesh, 2, chunk->mesh.normals, vertexCount*3*sizeof(float), 0);

        chunk->mesh.vboId[3] = rlLoadVertexBuffer(chunk->mesh.colors, vertexCount * 4 * sizeof(unsigned char), false);
        rlEnableVertexArray(chunk->mesh.vaoId);
        rlSetVertexAttribute(3, 4, RL_UNSIGNED_BYTE, 1, 0, 0);
        rlEnableVertexAttribute(3);
        rlDisableVertexArray();
    }
}

//----------------------------------------------------------------------------------
// 
//----------------------------------------------------------------------------------
void InitTerrain(void)
{
    terrainTexture = GenerateNoiseTexture(256, 64, (Color){190,190,190,255}, (Color){215,215,215,255});

    planeMaterial = LoadMaterialDefault();
    planeMaterial.maps[MATERIAL_MAP_ALBEDO].texture = terrainTexture;

    for (int cx = 0; cx < CHUNK_GRID_W; cx++)
        for (int cz = 0; cz < CHUNK_GRID_H; cz++)
            GenerateChunk(cx, cz);
}

void DrawTerrain(bool wireframe)
{
    if (wireframe) rlEnableWireMode();

    for (int cx = 0; cx < CHUNK_GRID_W; cx++)
        for (int cz = 0; cz < CHUNK_GRID_H; cz++)
        {
            Matrix t = MatrixTranslate(chunks[cx][cz].worldOffset.x, 0.0f, chunks[cx][cz].worldOffset.z);
            DrawMesh(chunks[cx][cz].mesh, planeMaterial, t);
        }

    if (wireframe) rlDisableWireMode();
}

void DrawChunkBoundaries(void)
{
    Color boundaryColor = Fade(GOLD, 0.85f);
    float height = 50.0f;

    for (int cx = 0; cx < CHUNK_GRID_W; cx++)
    {
        for (int cz = 0; cz < CHUNK_GRID_H; cz++)
        {
            TerrainChunk *chunk = &chunks[cx][cz];
            Vector3 center = { chunk->worldOffset.x, height/2.0f, chunk->worldOffset.z };
            Vector3 size = { CHUNK_SIZE, height, CHUNK_SIZE };

            DrawCubeWires(center, size.x, size.y, size.z, boundaryColor);
        }
    }
}

void UnloadTerrain(void)
{
    for (int cx = 0; cx < CHUNK_GRID_W; cx++)
        for (int cz = 0; cz < CHUNK_GRID_H; cz++)
        {
            UnloadMesh(chunks[cx][cz].mesh);
            MemFree(chunks[cx][cz].heights);
        }
    UnloadMaterial(planeMaterial);
    if (terrainTexture.id > 0) UnloadTexture(terrainTexture);
}