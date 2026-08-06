#include "texture.h"
#include <math.h>
#include "noise.h"

Texture2D GenerateNoiseTexture(int size, int cellCount, Color colorA, Color colorB)
{
    Image image = GenImageColor(size, size, colorA);

    for (int y = 0; y < size; y++)
    {
        for (int x = 0; x < size; x++)
        {
            float nx = ((float)x / size) * cellCount;
            float nz = ((float)y / size) * cellCount;

            float n = Noise2DTileable(nx, nz, cellCount, 42);

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
    SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(texture, TEXTURE_WRAP_REPEAT);
    UnloadImage(image);

    return texture;
}