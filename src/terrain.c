#include "terrain.h"
#include "raymath.h"
#include "rlgl.h"

#define CHUNK_GRID_W 32
#define CHUNK_GRID_H 32
#define CHUNK_RES    16
#define CHUNK_SIZE   20.0f
#define WORLD_SEED   1234

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
    BIOME_COUNT
} BiomeType;

typedef struct {
    float frequency;
    float persistence;
    int   octaves;
    float heightScale;
} BiomeParams;

static const BiomeParams biomeTable[BIOME_COUNT] = {
    //freq      persist oct heightScale
    { 0.02f,    0.6f,   2,  3.8f },
    { 0.03f,    0.7f,   3,  4.5f },
    { 0.015f,    0.95f,  6,  8.0f },
};

static const BiomeType biomeLookup[3][3] = {
    // dry              temperate         wet
    { BIOME_MOUNTAINS, BIOME_HILLS,  BIOME_HILLS  }, // cold
    { BIOME_PLAINS,    BIOME_HILLS,  BIOME_HILLS  }, // temperate
    { BIOME_PLAINS,    BIOME_PLAINS, BIOME_HILLS  }, // hot
};

//----------------------------------------------------------------------------------
// Noise
//----------------------------------------------------------------------------------
static float HashGridValue(int x, int z, unsigned int seed)
{
    // Deterministic hash of integer grid coordinates for cheap value-noise generation.

    unsigned int h = (unsigned int)(x * 374761393 + z * 668265263 + seed * 2147483647);
    h = (h ^ (h >> 13)) * 1274126177;
    h = h ^ (h >> 16);
    return (float)(h % 10000) / 10000.0f;
}

static float Smoothstep(float t)
{
    // Smooth interpolation weight for blending grid values.

    return t * t * (3.0f - 2.0f * t);
}

static float SampleValueNoise2D(float x, float z)
{
    // Sample a 2D value-noise field by blending four hashed grid corners.

    int x0 = (int)floorf(x), z0 = (int)floorf(z);
    float tx = Smoothstep(x - x0);
    float tz = Smoothstep(z - z0);

    float h00 = HashGridValue(x0, z0, WORLD_SEED);
    float h10 = HashGridValue(x0 + 1, z0, WORLD_SEED);
    float h01 = HashGridValue(x0, z0 + 1, WORLD_SEED);
    float h11 = HashGridValue(x0 + 1, z0 + 1, WORLD_SEED);

    float a = h00 + tx * (h10 - h00);
    float b = h01 + tx * (h11 - h01);
    return a + tz * (b - a);
}

// 

static float SampleHeightWithParams(float worldX, float worldZ, const BiomeParams *p)
{
    float total = 0.0f, amp = 1.0, freq = p->frequency;
    for (int o = 0; o < p->octaves; o++)
    {
        total += SampleValueNoise2D(worldX * freq, worldZ * freq) * amp;
        amp *= p->persistence;
        freq *= 2.0f;
    }
    return total * p->heightScale;
}

static float SampleTemperatureNoise(float worldX, float worldZ)
{
    const float freq = 0.006f;
    return SampleValueNoise2D(worldX * freq + 1000.0f, worldZ * freq + 1000.0f);
}

static float SampleMoistureNoise(float worldX, float worldZ)
{
    const float freq = 0.006f;
    return SampleValueNoise2D(worldX * freq + 5000.0f, worldZ * freq + 5000.0f);
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

        float h = GetTerrainHeight(worldX, worldZ);
        chunk->heights[i] = h;
        chunk->mesh.vertices[i * 3 + 1] = h;
    }

    RecalculateNormals(&chunk->mesh, chunk->heights, vertsPerSide, CHUNK_SIZE / (float)CHUNK_RES);

    if (chunk->mesh.vboId != NULL)
    {
        UpdateMeshBuffer(chunk->mesh, 0, chunk->mesh.vertices, vertexCount*3*sizeof(float), 0);
        UpdateMeshBuffer(chunk->mesh, 2, chunk->mesh.normals, vertexCount*3*sizeof(float), 0);
    }
}

static void CreateTerrainTexture(void)
{
    Image image = GenImageColor(256, 256, (Color){ 58, 102, 48, 255 });

    for (int y = 0; y < 256; y++)
    {
        for (int x = 0; x < 256; x++)
        {
            int tx = x / 32;
            int ty = y / 32;
            int checker = ((tx + ty) & 1) ? 1 : 0;

            if (checker)
            {
                ImageDrawPixel(&image, x, y, (Color){ 72, 124, 60, 255 });
            }
            else
            {
                ImageDrawPixel(&image, x, y, (Color){ 46, 84, 38, 255 });
            }
        }
    }

    terrainTexture = LoadTextureFromImage(image);
    SetTextureFilter(terrainTexture, TEXTURE_FILTER_BILINEAR);
    UnloadImage(image);
}

//----------------------------------------------------------------------------------
// 
//----------------------------------------------------------------------------------
void InitTerrain(void)
{
    planeMaterial = LoadMaterialDefault();
    CreateTerrainTexture();
    planeMaterial.maps[MATERIAL_MAP_ALBEDO].texture = terrainTexture;
    planeMaterial.maps[MATERIAL_MAP_ALBEDO].color = WHITE;

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