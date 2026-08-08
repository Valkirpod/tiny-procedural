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

#include "terrain.h"
#include "player.h"

//----------------------------------------------------------------------------------
// Local Variables
//----------------------------------------------------------------------------------
static Camera3D camera = { 0 };
static Player player;

static bool debugShow = false;
static bool wireframeShow = false;

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
// Gameplay Screen Functions
//----------------------------------------------------------------------------------
void InitGameplayScreen(void)
{
    camera.position = (Vector3){ 0.0f, 2.0f, 5.0f };
    camera.target   = (Vector3){ 0.0f, 0.0f, 0.0f };
    camera.up       = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy     = 75.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    InitTerrain();
    InitPlayer(&player, (Vector3){ 0.0f, 10.0f, 0.0f });
}

void UpdateGameplayScreen(void)
{
    UpdatePlayer(&player);
    camera = GetPlayerCamera(&player);

    if (IsKeyPressed(KEY_F1))
    {
        debugShow = !debugShow;
        wireframeShow = false;
        TraceLog(LOG_INFO, "Debug overlay %s", debugShow ? "ON" : "OFF");
    }

    if (IsKeyPressed(KEY_L) && debugShow)
    {
        wireframeShow = !wireframeShow;
        TraceLog(LOG_INFO, "Wireframe mode %s", wireframeShow ? "ON" : "OFF");
    }
}

void DrawGameplayScreen(void)
{
    ClearBackground(RAYWHITE);

    BeginMode3D(camera);
        if (wireframeShow)
        {
            rlEnableWireMode();
        }

        DrawTerrain(wireframeShow);
        
        if (wireframeShow)
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

        DrawRectangle(10, GetScreenHeight() - 90 - 80 - 5, 260, 80, Fade(BLACK, 0.7f));
        DrawText(TextFormat("Debug Controls:"), 20, GetScreenHeight() - 90 - 80 - 5 + 10, 20, GRAY);
        DrawText(TextFormat("   L - Wireframe: %s", wireframeShow ? "ON" : "OFF"), 20, GetScreenHeight() - 90 - 80 - 5 + 35, 20, GRAY);

        DrawRectangle(260 + 20, GetScreenHeight() - 90, 280, 80, Fade(BLACK, 0.7f));
        DrawText(TextFormat("X: %.2f, Y: %.2f, Z: %.2f", player.position.x, player.position.y, player.position.z), 260 + 30, GetScreenHeight() - 90 + 10, 20, LIGHTGRAY);
        DrawText(TextFormat("Grounded: %s", player.grounded ? "YES" : "NO"), 260 + 30, GetScreenHeight() - 90 + 35, 20, LIGHTGRAY);
        DrawText(TextFormat("Air Time: %.2f s", player.air_time), 260 + 30, GetScreenHeight() - 90 + 60, 20, LIGHTGRAY);

        DrawRectangle(260 + 20, GetScreenHeight() - 90 - 80 - 5, 280, 80, Fade(BLACK, 0.7f));
        BiomeType b;
        GetTerrainHeightAndBiome(player.position.x, player.position.z, &b);
        DrawText(TextFormat("Biome: %s", GetBiomeName(b)), 260 + 30, GetScreenHeight() - 90 - 80 - 5 + 10, 20, GRAY);
    };
}

void UnloadGameplayScreen(void)
{
    UnloadTerrain();
}

int FinishGameplayScreen(void)
{
    return 0;
}