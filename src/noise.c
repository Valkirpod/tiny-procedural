#include "noise.h"
#include <math.h>

static float Smoothstep(float t) { return t * t * (3.0f - 2.0f * t); }

static float HashGridValue(int x, int z, int wrap, unsigned int seed)
{
    if (wrap > 0)
    {
        x = ((x % wrap) + wrap) % wrap;
        z = ((z % wrap) + wrap) % wrap;
    }

    unsigned int h = (unsigned int)(x * 374761393 + z * 668265263 + seed * 2147483647);
    h = (h ^ (h >> 13)) * 1274126177;
    h = h ^ (h >> 16);
    return (float)(h % 10000) / 10000.0f;
}

static float SampleNoise(float x, float z, int wrap, unsigned int seed)
{
    int x0 = (int)floorf(x), z0 = (int)floorf(z);
    float tx = Smoothstep(x - x0), tz = Smoothstep(z - z0);
 
    float h00 = HashGridValue(x0,   z0,   wrap, seed);
    float h10 = HashGridValue(x0+1, z0,   wrap, seed);
    float h01 = HashGridValue(x0,   z0+1, wrap, seed);
    float h11 = HashGridValue(x0+1, z0+1, wrap, seed);
 
    float a = h00 + tx * (h10 - h00);
    float b = h01 + tx * (h11 - h01);
    return a + tz * (b - a);
}
 
float Noise2D(float x, float z, unsigned int seed)
{
    return SampleNoise(x, z, 0, seed);
}
 
float Noise2DTileable(float x, float z, int wrap, unsigned int seed)
{
    return SampleNoise(x, z, wrap, seed);
}
 