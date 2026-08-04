/*******************************************************************************************
*
*   tiny-procedural
*
********************************************************************************************/

#include "raylib.h"
#include "screens.h"

//----------------------------------------------------------------------------------
// Shared Variables
//----------------------------------------------------------------------------------
GameScreen currentScreen = GAMEPLAY;

//----------------------------------------------------------------------------------
// Local Variables
//----------------------------------------------------------------------------------
static const int screenWidth = 1280;
static const int screenHeight = 720;

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
static void UpdateDrawFrame(void);

//----------------------------------------------------------------------------------
// Program main entry point
//----------------------------------------------------------------------------------
int main(void)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(screenWidth, screenHeight, "tiny-procedural");
    SetWindowState(FLAG_WINDOW_MAXIMIZED);
    DisableCursor();

    InitAudioDevice();

    currentScreen = GAMEPLAY;
    InitGameplayScreen();

    SetTargetFPS(60);

    while (!WindowShouldClose())
    {
        UpdateDrawFrame();
    }

    UnloadGameplayScreen();

    CloseAudioDevice();
    CloseWindow();

    return 0;
}

static void UpdateDrawFrame(void)
{
    UpdateGameplayScreen();

    BeginDrawing();

        ClearBackground(RAYWHITE);
        DrawGameplayScreen();

    EndDrawing();
}