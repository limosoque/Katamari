struct PickResult
{
    float4 WorldPositionViewDepth; // xyz = world position, w = view-space depth.
    float4 NormalValid;            // xyz = normal, w = valid as 0/1 for quick GPU-side checks.
    uint4 ObjectIdPixelValid;      // x = object id, y = pixel x, z = pixel y, w = valid.
};

cbuffer PickCB : register(b0)
{
    uint2 Pixel;
    uint2 ScreenSize;
};

Texture2D<float4> GBufferNormalShininess : register(t0);
Texture2D<float4> GBufferSpecularViewDepth : register(t1);
Texture2D<float4> GBufferWorldPosition : register(t2);
Texture2D<uint> GBufferObjectId : register(t3);

RWStructuredBuffer<PickResult> Output : register(u0);

[numthreads(1, 1, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    PickResult result;
    result.WorldPositionViewDepth = float4(0.0f, 0.0f, 0.0f, 0.0f);
    result.NormalValid = float4(0.0f, 1.0f, 0.0f, 0.0f);
    result.ObjectIdPixelValid = uint4(0, Pixel.x, Pixel.y, 0);

    if (Pixel.x >= ScreenSize.x || Pixel.y >= ScreenSize.y)
    {
        Output[0] = result;
        return;
    }

    int3 pixel = int3(Pixel, 0);
    float4 normalShininess = GBufferNormalShininess.Load(pixel);
    float4 specularViewDepth = GBufferSpecularViewDepth.Load(pixel);
    float4 worldPosition = GBufferWorldPosition.Load(pixel);
    uint objectId = GBufferObjectId.Load(pixel);

    uint valid = (specularViewDepth.a > 0.0001f && objectId != 0) ? 1 : 0;

    result.WorldPositionViewDepth = float4(worldPosition.xyz, specularViewDepth.a);
    result.NormalValid = float4(normalize(normalShininess.xyz), (float)valid);
    result.ObjectIdPixelValid = uint4(objectId, Pixel.x, Pixel.y, valid);
    Output[0] = result;
}
