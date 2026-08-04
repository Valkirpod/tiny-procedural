#include "player.h"
#include "terrain.h"
#include "raymath.h"
#include <math.h>

#define MOUSE_SENS      0.0013f
#define PITCH_LIMIT     1.5f
#define FIELD_OF_VIEW   75.0f
#define AIR_FOV_MULT    6.5f
#define FOV_LERP_SPEED  3.0f

#define MOVE_SPEED      6.0f
#define SPRINT_SPEED    13.0f
#define MOVE_ACCEL      10.0f
#define MOVE_ACCEL_AIR  0.5f

#define GRAVITY         -25.0f
#define JUMP_FORCE      9.5f
#define SNAP_DISTANCE   0.25f

#define EYE_HEIGHT      1.7f
#define BOB_FREQ        18.0f
#define SPRINT_BOB_FREQ 21.0f
#define BOB_AMPLITUDE   0.145f
#define LEAN_AMOUNT     0.05f

static Vector3 GetMovementVector(Player *player)
{
    float yaw = player->yaw;

    Vector3 forward = { sinf(yaw), 0.0f, cosf(yaw) };
    Vector3 right   = { -cosf(yaw), 0.0f, sinf(yaw) };
    Vector3 move = { 0.0f, 0.0f, 0.0f };

    if (IsKeyDown(KEY_W)) move = Vector3Add(move, forward);
    if (IsKeyDown(KEY_S)) move = Vector3Subtract(move, forward);
    if (IsKeyDown(KEY_D)) move = Vector3Add(move, right);
    if (IsKeyDown(KEY_A)) move = Vector3Subtract(move, right);

    if (Vector3LengthSqr(move) > 0.0f)
    {
        move = Vector3Normalize(move);
        move = Vector3Scale(move, player->current_speed);
    }

    return move;
}

static void UpdateCameraMotion(Player *player, Vector3 move, float dt)
{
    bool isMoving = Vector3LengthSqr((Vector3){ move.x, 0.0f, move.z }) > 0.001f;

    if (player->grounded && isMoving)
    {
        player->bobTime += dt * player->current_bob_frequency;
    }
    else
    {
        player->bobTime = 0.0f;
    }

    float targetBob = isMoving ? BOB_AMPLITUDE * sinf(player->bobTime) : 0.0f;
    player->bobOffset = Lerp(player->bobOffset, targetBob, 10.0f * dt);

    Vector3 forward = {
        cosf(player->pitch) * sinf(player->yaw),
        sinf(player->pitch),
        cosf(player->pitch) * cosf(player->yaw)
    };
    forward = Vector3Normalize(forward);

    Vector3 worldUp = { 0.0f, 1.0f, 0.0f };
    Vector3 right = Vector3CrossProduct(worldUp, forward);
    if (Vector3LengthSqr(right) < 0.0001f)
    {
        right = (Vector3){ 1.0f, 0.0f, 0.0f };
    }
    else
    {
        right = Vector3Normalize(right);
    }

    float leanTarget = isMoving ? Clamp(Vector3DotProduct(move, right) / MOVE_SPEED, -LEAN_AMOUNT, LEAN_AMOUNT) : 0.0f;
    player->leanOffset = Lerp(player->leanOffset, leanTarget, 10.0f * dt);
}

void InitPlayer(Player *player, Vector3 startPos)
{
    player->position = startPos;
    player->velocity = (Vector3){ 0 };
    player->yaw = 0.0f;
    player->pitch = 0.0f;
    player->grounded = false;
    player->bobTime = 0.0f;
    player->bobOffset = 0.0f;
    player->leanOffset = 0.0f;
    player->air_time = 0.0f;
    player->current_fov = FIELD_OF_VIEW;
    player->current_bob_frequency = BOB_FREQ;
    player->current_speed = SPRINT_SPEED;
}

void UpdatePlayer(Player *player)
{
    float dt = GetFrameTime();

    // handle mouse input
    Vector2 mouseDelta = GetMouseDelta();
    player->yaw   -= mouseDelta.x * MOUSE_SENS;
    player->pitch -= mouseDelta.y * MOUSE_SENS;
    player->pitch = Clamp(player->pitch, -PITCH_LIMIT, PITCH_LIMIT);

    // handle movement input
    Vector3 move = GetMovementVector(player);
    player->velocity.x = Lerp(player->velocity.x, move.x, player->grounded ? MOVE_ACCEL * dt : MOVE_ACCEL_AIR * dt);
    player->velocity.z = Lerp(player->velocity.z, move.z, player->grounded ? MOVE_ACCEL * dt : MOVE_ACCEL_AIR * dt);

    // camera effects
    UpdateCameraMotion(player, move, dt);

    // gravity & move
    player->velocity.y += GRAVITY * dt;
    player->position = Vector3Add(player->position, Vector3Scale(player->velocity, dt));

    // ground colllision and snapping
    float groundY = SampleTerrainHeight(player->position.x, player->position.z) + EYE_HEIGHT;
    float distanceToGround = player->position.y - groundY;

    if (distanceToGround <= 0.0f || distanceToGround <= SNAP_DISTANCE && player->grounded)
    {
        player->position.y = groundY;
        player->velocity.y = 0.0f;
        player->grounded = true;
        player->air_time = 0.0f;
    }
    else
    {
        player->grounded = false;
        player->air_time += dt;
    }

    // jump
    if (player->grounded && IsKeyPressed(KEY_SPACE))
    {
        player->velocity.y = JUMP_FORCE;
        player->grounded = false;
    }

    // sprint
    if (IsKeyDown(KEY_LEFT_SHIFT) && player->grounded)
    {
        player->current_speed = SPRINT_SPEED;
        player->current_bob_frequency = SPRINT_BOB_FREQ;
    }
    else
    {
        player->current_speed = MOVE_SPEED;
        player->current_bob_frequency = BOB_FREQ;   
    }

    // air time fov
    float targetFov = player->grounded ? FIELD_OF_VIEW : FIELD_OF_VIEW + player->air_time * AIR_FOV_MULT;
    player->current_fov = Lerp(player->current_fov, targetFov, FOV_LERP_SPEED * dt);
}

Camera3D GetPlayerCamera(Player *player)
{
    float dt = GetFrameTime();
    Camera3D cam = { 0 };

    Vector3 cameraPos = player->position;
    cameraPos.y += EYE_HEIGHT + player->bobOffset;
    cam.position = cameraPos;

    float leanAngle = player->leanOffset;
    Vector3 forward = {
        cosf(player->pitch) * sinf(player->yaw),
        sinf(player->pitch),
        cosf(player->pitch) * cosf(player->yaw)
    };
    forward = Vector3Normalize(forward);

    Vector3 up = { 0.0f, 1.0f, 0.0f };
    Vector3 right = Vector3CrossProduct(up, forward);
    if (Vector3LengthSqr(right) < 0.0001f)
    {
        right = (Vector3){ 1.0f, 0.0f, 0.0f };
    }
    else
    {
        right = Vector3Normalize(right);
    }

    Vector3 tiltedUp = Vector3Normalize(Vector3Add(
        Vector3Scale(up, cosf(leanAngle)),
        Vector3Scale(right, sinf(leanAngle))));

    cam.target = Vector3Add(cameraPos, forward);
    cam.up = tiltedUp;
    cam.fovy = player->current_fov;
    cam.projection = CAMERA_PERSPECTIVE;
    return cam;
}