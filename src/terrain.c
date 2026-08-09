#include "terrain.h"
#include "raymath.h"
#include "rlgl.h"
#include "texture.h"
#include "noise.h"

#define MAX_CHUNKS   2048*100
#define RENDER_RANGE 6
#define CHUNK_RES    16
#define CHUNK_SIZE   64.0f
#define WORLD_SEED   1254

#define ELEVATION_WEIGHT 55.0f
#define ELEVATION_FREQ  0.008f
#define CLIMATE_FREQ    0.002f

typedef struct {
    int x, z;
    Mesh mesh;
    float *heights;
    Vector3 worldOffset;
} TerrainChunk;

static int activeChunks = 0;
static TerrainChunk chunks[MAX_CHUNKS];
static Material planeMaterial = { 0 };
static Texture2D terrainTexture = { 0 };

// biomes

typedef struct {
    float frequency;
    float persistence;
    int   octaves;
    float heightScale;
    Color color;
} BiomeParams;

static const BiomeParams biomeTable[BIOME_COUNT] = {
    //freq      persist oct heightscale color
    { 0.02f,    0.6f,   2,  1.8f,       {100.0f, 222.0f, 100.0f}},    // plains
    { 0.1f,    0.85f,   4,  2.4f,       {100.0f, 100.0f, 125.0f}},      // hills
    { 0.01f,    0.95f,  6,  4.8f,      {125.0f, 125.0f, 155.0f}},      // mountains
    { 0.05f,    0.95f,  6,  8.8f,      {255.0f, 255.0f, 255.0f}},      // peaks
    { 0.01f,    0.6f,   1,  1.0f,       {255.0f, 244.0f, 200.0f}},      // desert
    { 0.04f,    0.6f,   2,  2.2f,       {200.0f, 222.0f, 100.0f}},      // steppes
    { 0.01f,    0.95f,   4,  2.4f,       {0.0f, 200.0f, 0.0f}},      // jungle
    { 0.01f,    0.95f,   4,  2.4f,       {100.0f, 200.0f, 100.0f}},      // forest
    { 0.02f,    0.6f,   2,  1.8f,       {255.0f, 255.0f, 255.0f}},      // tundra
    { 0.02f,    0.6f,   2,  1.8f,       {255.0f, 255.0f, 255.0f}},      // taiga
    { 0.03f,    0.9f,   8,  3.0f,       {200.0f, 255.0f, 255.0f}},      // icy
};

static const BiomeType biomeLookup[3][3] = {
    // dry              temperate         wet
    { BIOME_ICY,     BIOME_TUNDRA,  BIOME_TAIGA  }, // cold
    { BIOME_STEPPES,    BIOME_PLAINS,      BIOME_FOREST  }, // temperate
    { BIOME_DESERT,    BIOME_STEPPES,     BIOME_JUNGLE  }, // hot
};

#define PEAKS_ELEVATION     40.0f
#define MOUNTAIN_ELEVATION  28.0f
#define HILLS_ELEVATION     15.0f

const char *GetBiomeName(BiomeType b)
{
    switch (b) {
        case BIOME_PLAINS:    return "PLAINS";
        case BIOME_HILLS:     return "HILLS";
        case BIOME_MOUNTAINS: return "MOUNTAINS";
        case BIOME_PEAKS:     return "PEAKS";
        case BIOME_DESERT:    return "DESERT";
        case BIOME_STEPPES:   return "STEPPES";
        case BIOME_JUNGLE:    return "JUNGLE";
        case BIOME_FOREST:     return "FOREST";
        case BIOME_TUNDRA:      return "TUNDRA";
        case BIOME_TAIGA:       return "TAIGA";
        case BIOME_ICY:         return "ICY";
        default:              return "UNKNOWN";
    }
}

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
    return Noise2D(worldX * CLIMATE_FREQ + 1000.0f, worldZ * CLIMATE_FREQ + 1000.0f, WORLD_SEED);
}

static float SampleMoistureNoise(float worldX, float worldZ)
{
    const float freq = 0.006f;
    return Noise2D(worldX * CLIMATE_FREQ + 5000.0f, worldZ * CLIMATE_FREQ + 5000.0f, WORLD_SEED);
}

static float SampleElevationNoise(float worldX, float worldZ)
{
    float e = Noise2D(worldX * ELEVATION_FREQ + 9000.0f, worldZ * ELEVATION_FREQ + 9000.0f, WORLD_SEED);

    const float redistributionPower = 4.0f; // higher = flatter lowlands, sharper rare peaks
    e = powf(e, redistributionPower);

    return e;
}

// 

static int GetBucket(float value, int bucketCount)
{
    int b = (int)(value * bucketCount);
    if (b >= bucketCount) b = bucketCount - 1;
    if (b < 0) b = 0;
    return b;
}

float GetTerrainHeightAndBiome(float worldX, float worldZ, BiomeType *outBiome)
{
    float e = SampleElevationNoise(worldX, worldZ) * ELEVATION_WEIGHT;

    float temp  = SampleTemperatureNoise(worldX, worldZ);
    float moist = SampleMoistureNoise(worldX, worldZ);
    BiomeType biome = biomeLookup[GetBucket(temp, 3)][GetBucket(moist, 3)];

    if (e > PEAKS_ELEVATION) biome = BIOME_PEAKS;
    else if (e > MOUNTAIN_ELEVATION)      biome = BIOME_MOUNTAINS;
    else if (e > HILLS_ELEVATION) biome = BIOME_HILLS;

    if (outBiome) *outBiome = biome;

    return e + SampleHeightWithParams(worldX, worldZ, &biomeTable[biome]);
}

float GetTerrainHeight(float worldX, float worldZ)
{
    return GetTerrainHeightAndBiome(worldX, worldZ, NULL);
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

static TerrainChunk *FindChunk(int cx, int cz)
{
    for (int i = 0; i < activeChunks; i++)
    {
        if (chunks[i].x == cx && chunks[i].z == cz)
            return &chunks[i];
    }
    return NULL;
}

static void GenerateChunk(int cx, int cz)
{
    if (FindChunk(cx, cz)) return;
    if (activeChunks >= MAX_CHUNKS) return;

    TerrainChunk *chunk = &chunks[activeChunks++];
    chunk->x = cx;
    chunk->z = cz;

    chunk->mesh = GenMeshPlane(CHUNK_SIZE, CHUNK_SIZE, CHUNK_RES, CHUNK_RES);

    int vertsPerSide = CHUNK_RES + 1;
    int vertexCount = vertsPerSide * vertsPerSide;
    chunk->heights = (float *)MemAlloc(vertexCount * sizeof(float));
    chunk->mesh.colors = (unsigned char *)MemAlloc(vertexCount * 4 * sizeof(unsigned char));

    chunk->worldOffset = (Vector3){
        cx * CHUNK_SIZE + CHUNK_SIZE * 0.5f,
        0.0f,
        cz * CHUNK_SIZE + CHUNK_SIZE * 0.5f
    };

    for (int i = 0; i < vertexCount; i++)
    {
        float localX = chunk->mesh.vertices[i * 3 + 0];
        float localZ = chunk->mesh.vertices[i * 3 + 2];

        float worldX = localX + chunk->worldOffset.x;
        float worldZ = localZ + chunk->worldOffset.z;

        BiomeType b;
        float h = GetTerrainHeightAndBiome(worldX, worldZ, &b);
        chunk->heights[i] = h;
        chunk->mesh.vertices[i * 3 + 1] = h;

        Color c = biomeTable[b].color;
        chunk->mesh.colors[i*4+0] = c.r;
        chunk->mesh.colors[i*4+1] = c.g;
        chunk->mesh.colors[i*4+2] = c.b;
        chunk->mesh.colors[i*4+3] = 255;
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

static void UnloadChunk(int i)
{
    if (i < 0 || i >= activeChunks) return;

    UnloadMesh(chunks[i].mesh);
    if (chunks[i].heights) MemFree(chunks[i].heights);
    
    chunks[i] = chunks[activeChunks - 1];
    activeChunks--;
}

//----------------------------------------------------------------------------------
// 
//----------------------------------------------------------------------------------
void InitTerrain(void)
{
    terrainTexture = GenerateNoiseTexture(512, (Color){175,175,175,255}, (Color){245,245,245,255}, 128, 6, 0.35f);

    planeMaterial = LoadMaterialDefault();
    planeMaterial.maps[MATERIAL_MAP_ALBEDO].texture = terrainTexture;
}

void UpdateTerrainStreaming(float playerX, float playerZ)
{
    int pcx = (int)floorf(playerX / CHUNK_SIZE);
    int pcz = (int)floorf(playerZ / CHUNK_SIZE);

    int radius = RENDER_RANGE;

    for (int dz = -radius; dz <= radius; dz++)
        for (int dx = -radius; dx <= radius; dx++)
            GenerateChunk(pcx + dx, pcz + dz);

    for (int i = activeChunks - 1; i >= 0; i--)
    {
        int dx = chunks[i].x - pcx;
        int dz = chunks[i].z - pcz;

        int dist = (abs(dx) > abs(dz)) ? abs(dx) : abs(dz);

        if (dist > RENDER_RANGE + 1)
        {
            UnloadChunk(i);
        }
    }
}

void DrawTerrain(bool wireframe)
{
    if (wireframe) rlEnableWireMode();

    for (int i = 0; i < activeChunks; i++)
    {
        Matrix t = MatrixTranslate(chunks[i].worldOffset.x, 0.0f, chunks[i].worldOffset.z);
        DrawMesh(chunks[i].mesh, planeMaterial, t);
    }

    if (wireframe) rlDisableWireMode();
}

void DrawChunkBoundaries(void)
{
    Color boundaryColor = Fade(GOLD, 0.85f);
    float height = 50.0f;

    for (int i = 0; i < activeChunks; i++)
    {
        TerrainChunk *chunk = &chunks[i];
        Vector3 center = { chunk->worldOffset.x, height/2.0f, chunk->worldOffset.z };
        Vector3 size = { CHUNK_SIZE, height, CHUNK_SIZE };

        DrawCubeWires(center, size.x, size.y, size.z, boundaryColor);
    }
}

void UnloadTerrain(void)
{
    for (int i = 0; i < activeChunks; i++)
    {
        UnloadMesh(chunks[i].mesh);
        MemFree(chunks[i].heights);
    }
    UnloadMaterial(planeMaterial);
    if (terrainTexture.id > 0) UnloadTexture(terrainTexture);
}