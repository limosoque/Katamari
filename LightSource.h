#ifndef LIGHT_SOURCE_H
#define LIGHT_SOURCE_H

#define MAX_LIGHTS 8

//if added in c++ use directxmath
#ifdef __cplusplus
#include <DirectXMath.h>
typedef DirectX::XMFLOAT4 float4;
typedef DirectX::XMFLOAT4X4 float4x4;
typedef int int1;
#else
    //do nothing cause shader already have these types
#endif

struct LightSource
{
    float4 Color;
    float4 Position;
    int    Type;
    float  Range;
    float  Padding[2];
};

#endif