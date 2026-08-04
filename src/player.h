#ifndef PLAYER_H
#define PLAYER_H

#include "raylib.h"

typedef struct {
    Vector3 position;
    Vector3 velocity;
    float yaw;
    float pitch;
    bool grounded;
    float bobTime;
    float bobOffset;
    float leanOffset;
    float air_time;
    float current_fov;
    float current_speed;
    float current_bob_frequency;
} Player;

void InitPlayer(Player *player, Vector3 startPos);
void UpdatePlayer(Player *player);
Camera3D GetPlayerCamera(Player *player);

#endif