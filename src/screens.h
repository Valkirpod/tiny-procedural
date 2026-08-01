#ifndef SCREENS_H
#define SCREENS_H

//----------------------------------------------------------------------------------
// Types
//----------------------------------------------------------------------------------
typedef enum GameScreen { UNKNOWN = -1, GAMEPLAY = 0 } GameScreen;

//----------------------------------------------------------------------------------
// Global Variables
//----------------------------------------------------------------------------------
extern GameScreen currentScreen;

#ifdef __cplusplus
extern "C" {
#endif

//----------------------------------------------------------------------------------
// Gameplay Screen Functions
//----------------------------------------------------------------------------------
void InitGameplayScreen(void);
void UpdateGameplayScreen(void);
void DrawGameplayScreen(void);
void UnloadGameplayScreen(void);
int FinishGameplayScreen(void);

#ifdef __cplusplus
}
#endif

#endif // SCREENS_H