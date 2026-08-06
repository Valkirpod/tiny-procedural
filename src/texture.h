#ifndef TEXTURE_H
#define TEXTURE_H

#include "raylib.h"

// Generates a tileable grayscale-ish noise texture, blended between two colors.
Texture2D GenerateNoiseTexture(int size, int cellCount, Color colorA, Color colorB);

#endif