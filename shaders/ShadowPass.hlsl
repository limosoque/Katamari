//renders scene depth into a Texture2DArray for cascades
//geometry shader is instanced kCascadeCount times
//and instance writes the triangle into one array slice using
//SV_RenderTargetArrayIndex, so all cascades are filled in a single draw call

cbuffer PerObjectShadow : register(b0)
{
    float4x4 WorldMatrix;
};

cbuffer CascadeBuf : register(b1)
{
    float4x4 LightViewProj[3];
    float4 CascadeSplits;
};

struct GS_IN
{
    float4 posW : POSITION;
};

struct GS_OUT
{
    float4 posH : SV_POSITION;
    uint arrIdx : SV_RenderTargetArrayIndex; //in which cascade slice to write
};

GS_IN VShadow(float3 pos : POSITION,
              float3 normal : NORMAL,
              float2 uv : TEXCOORD0)
{
    GS_IN o;
    o.posW = mul(float4(pos, 1.0f), WorldMatrix);
    return o;
}

[instance(3)] //kCascadeCount
[maxvertexcount(3)]
void GShadow(
    triangle GS_IN input[3],
    in uint instanceId : SV_GSInstanceID,
    inout TriangleStream<GS_OUT> stream)
{
    [unroll]
    for (int v = 0; v < 3; ++v)
    {
        GS_OUT o;
        //project worldspace vertex into this cascade's light space
        o.posH = mul(float4(input[v].posW.xyz, 1.0f), LightViewProj[instanceId]);
        o.arrIdx = instanceId;
        stream.Append(o);
    }
    stream.RestartStrip();
}