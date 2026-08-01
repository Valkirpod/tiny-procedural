// Prevent Windows headers from conflicting with raylib
#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #define NOGDI
    #define NOUSER
    #include <windows.h>
    #include <psapi.h>
#endif

#include "raylib.h"
#include "screens.h"
#include "raymath.h"

//----------------------------------------------------------------------------------
// Terrain Config
//----------------------------------------------------------------------------------
#define CHUNK_GRID_W 32
#define CHUNK_GRID_H 32
#define CHUNK_RES    16
#define CHUNK_SIZE   20.0f
#define WORLD_SEED   1234

typedef struct {
    Mesh mesh;
    float *heights;
    Vector3 worldOffset; // translation to place this chunk in the world
} TerrainChunk;

//----------------------------------------------------------------------------------
// Local Variables
//----------------------------------------------------------------------------------
static Camera3D camera = { 0 };
static TerrainChunk chunks[CHUNK_GRID_W][CHUNK_GRID_H];

static Material planeMaterial = { 0 };
static bool debugShow = false;

//----------------------------------------------------------------------------------
// Helper Functions
//----------------------------------------------------------------------------------
static float GetRAMUsageMB(void)
{
    #if defined(_WIN32)
        PROCESS_MEMORY_COUNTERS_EX pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc)))
        {
            return (float)pmc.PrivateUsage / (1024.0f * 1024.0f);
        }
    #endif
    return 0.0f;
}

//----------------------------------------------------------------------------------
// Terrain generation (value noise, no external deps)
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

static float SampleTerrainHeight(float worldX, float worldZ)
{
    // Combine several value-noise octaves to create a richer terrain height field.

    const float frequency = 0.03f;  // how often the noise pattern repeats in world space
    const float amplitude = 1.0f;   // how strong the noise is (before scaling to height)
    const float persistence = 0.5f; // how quickly the amplitude decreases for each octave
    const int octaves = 1;          // how many layers of noise to combine
    const float heightScale = 6.0f; // how much to scale the final height value

    float total = 0.0f;
    float layerAmplitude = amplitude;
    float layerFrequency = frequency;

    for (int octave = 0; octave < octaves; octave++)
    {
        total += SampleValueNoise2D(worldX * layerFrequency, worldZ * layerFrequency) * layerAmplitude;

        layerAmplitude *= persistence;
        layerFrequency *= 2.0f;
    }

    return total * heightScale;
}

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

        float h = SampleTerrainHeight(worldX, worldZ);
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

static void DrawChunkBoundaries(void)
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

//----------------------------------------------------------------------------------
// Gameplay Screen Functions
//----------------------------------------------------------------------------------
void InitGameplayScreen(void)
{
    camera.position = (Vector3){ 0.0f, 2.0f, 5.0f };
    camera.target   = (Vector3){ 0.0f, 0.0f, 0.0f };
    camera.up       = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy     = 75.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    planeMaterial = LoadMaterialDefault();
    planeMaterial.maps[MATERIAL_MAP_ALBEDO].color = LIME;

    for (int cx = 0; cx < CHUNK_GRID_W; cx++)
        for (int cz = 0; cz < CHUNK_GRID_H; cz++)
            GenerateChunk(cx, cz);
}

void UpdateGameplayScreen(void)
{
    UpdateCamera(&camera, CAMERA_FREE);

    if (IsKeyPressed(KEY_R))
    {
        camera.position = (Vector3){ 0.0f, 2.0f, 5.0f };
        camera.target   = (Vector3){ 0.0f, 0.0f, 0.0f };
    }

    if (IsKeyPressed(KEY_F1))
    {
        debugShow = !debugShow;
        TraceLog(LOG_INFO, "Debug overlay %s", debugShow ? "ON" : "OFF");
    }
}

void DrawGameplayScreen(void)
{
    ClearBackground(RAYWHITE);

    BeginMode3D(camera);
        if (debugShow)
        {
            rlEnableWireMode();
        }

        for (int cx = 0; cx < CHUNK_GRID_W; cx++)
        {
            for (int cz = 0; cz < CHUNK_GRID_H; cz++)
            {
                Matrix transform = MatrixTranslate(chunks[cx][cz].worldOffset.x, 0.0f, chunks[cx][cz].worldOffset.z);
                DrawMesh(chunks[cx][cz].mesh, planeMaterial, transform);
            }
        }
        
        if (debugShow)
        {
            rlDisableWireMode();
            DrawChunkBoundaries();
        }
    EndMode3D();

    if (debugShow) {
        int fps = GetFPS();
        float frameTime = GetFrameTime() * 1000.0f;
        float ramMB = GetRAMUsageMB();

        DrawRectangle(10, GetScreenHeight() - 90, 260, 80, Fade(BLACK, 0.7f));

        DrawText(TextFormat("FPS: %d", fps), 20, GetScreenHeight() - 80, 20, LIME);
        DrawText(TextFormat("Frame: %.2f ms", frameTime), 20, GetScreenHeight() - 55, 20, RAYWHITE);
        DrawText(TextFormat("RAM: %.1f MB", ramMB), 20, GetScreenHeight() - 30, 20, SKYBLUE);
    };
}

void UnloadGameplayScreen(void)
{
    for (int cx = 0; cx < CHUNK_GRID_W; cx++)
    {
        for (int cz = 0; cz < CHUNK_GRID_H; cz++)
        {
            UnloadMesh(chunks[cx][cz].mesh);
            MemFree(chunks[cx][cz].heights);
        }
    }
    UnloadMaterial(planeMaterial);
}

int FinishGameplayScreen(void)
{
    return 0;
}