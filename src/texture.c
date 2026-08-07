#include "texture.h"
#include <math.h>
#include "noise.h"

#define SEED 42

Texture2D GenerateNoiseTexture(int size, Color colorA, Color colorB, int cellCount, int detailCellCount, float detailWeight)
{
    Image image = GenImageColor(size, size, colorA);

    for (int y = 0; y < size; y++)
    {
        for (int x = 0; x < size; x++)
        {
            float nx = ((float)x / size) * cellCount;
            float nz = ((float)y / size) * cellCount;

            float n1 = Noise2DTileable(nx, nz, cellCount, SEED);
            float n2 = Noise2DTileable(nx * detailCellCount / cellCount, nz * detailCellCount / cellCount, detailCellCount, SEED + 1);

            float n = n1 * (1.0f - detailWeight) + n2 * detailWeight;

            Color pixel = {
                (unsigned char)(colorA.r + (colorB.r - colorA.r) * n),
                (unsigned char)(colorA.g + (colorB.g - colorA.g) * n),
                (unsigned char)(colorA.b + (colorB.b - colorA.b) * n),
                255
            };
            ImageDrawPixel(&image, x, y, pixel);
        }
    }

    Texture2D texture = LoadTextureFromImage(image);
    SetTextureFilter(texture, TEXTURE_FILTER_POINT);
    SetTextureWrap(texture, TEXTURE_WRAP_REPEAT);
    UnloadImage(image);

    return texture;
}