#include "texture.h"
#include <math.h>

static float HashGridValueWrapped(int x, int z, int wrap, unsigned int seed)
{
    x = ((x % wrap) + wrap) % wrap;
    z = ((z % wrap) + wrap) % wrap;

    unsigned int h = (unsigned int)(x * 374761393 + z * 668265263 + seed * 2147483647);
    h = (h ^ (h >> 13)) * 1274126177;
    h = h ^ (h >> 16);
    return (float)(h % 10000) / 10000.0f;
}

static float Smoothstep(float t) { return t * t * (3.0f - 2.0f * t); }

static float SampleTileableNoise(float x, float z, int wrap, unsigned int seed)
{
    int x0 = (int)floorf(x), z0 = (int)floorf(z);
    float tx = Smoothstep(x - x0), tz = Smoothstep(z - z0);

    float h00 = HashGridValueWrapped(x0,   z0,   wrap, seed);
    float h10 = HashGridValueWrapped(x0+1, z0,   wrap, seed);
    float h01 = HashGridValueWrapped(x0,   z0+1, wrap, seed);
    float h11 = HashGridValueWrapped(x0+1, z0+1, wrap, seed);

    float a = h00 + tx * (h10 - h00);
    float b = h01 + tx * (h11 - h01);
    return a + tz * (b - a);
}

Texture2D GenerateNoiseTexture(int size, int cellCount, Color colorA, Color colorB)
{
    Image image = GenImageColor(size, size, colorA);

    for (int y = 0; y < size; y++)
    {
        for (int x = 0; x < size; x++)
        {
            float nx = ((float)x / size) * cellCount;
            float nz = ((float)y / size) * cellCount;

            float n = SampleTileableNoise(nx, nz, cellCount, 42);

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