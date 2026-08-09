#ifndef NOISE_H
#define NOISE_H

float Noise2D(float x, float z, unsigned int seed);
 
float Noise2DTileable(float x, float z, int wrap, unsigned int seed);

#endif