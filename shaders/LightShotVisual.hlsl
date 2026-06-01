#define MAX_SHOT_LIGHTS 8
#define MAX_SHOT_TRAILS 64

cbuffer PerObject : register(b0)
{
    float4x4 WorldMatrix;
    float4x4 ViewMatrix;
    float4x4 ProjectionMatrix;

    float4 MaterialAmbientColor;
    float4 MaterialDiffuseColor;
    float4 MaterialSpecularColor;

    float MaterialShininess;
    uint ObjectId;
    float2 Padding;

    float4 SunlightColor;
    float4 SunlightDirection;
    float4 CameraPosition;

    float4x4 LightViewProj[3];
    float4 CascadeSplits;

    float4 ShotLightColorAndRange;      // rgb = visible shot color, a = unused here.
    float4 ShotLightCountsAndFlags;     // w = emissive strength for the visible shot sphere.
    float4 ShotLights[MAX_SHOT_LIGHTS];
    float4 ShotTrailStamps[MAX_SHOT_TRAILS];
};

struct VS_IN
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

struct PS_IN
{
    float4 posH : SV_POSITION;
    float3 posW : TEXCOORD0;
    float3 normalW : TEXCOORD1;
};

PS_IN VSMain(VS_IN input)
{
    PS_IN output;

    float4 worldPos = mul(float4(input.pos, 1.0f), WorldMatrix);
    float4 viewPos = mul(worldPos, ViewMatrix);

    output.posW = worldPos.xyz;
    output.posH = mul(viewPos, ProjectionMatrix);
    output.normalW = normalize(mul(input.normal, (float3x3) WorldMatrix));

    return output;
}

float4 PSMain(PS_IN input) : SV_Target
{
    float emissiveAmount = ShotLightCountsAndFlags.w;
    clip(emissiveAmount - 0.001f);

    float3 normal = normalize(input.normalW);
    float3 viewDirection = normalize(CameraPosition.xyz - input.posW);
    float rim = pow(1.0f - saturate(dot(normal, viewDirection)), 2.0f);
    float3 glow = ShotLightColorAndRange.rgb * emissiveAmount * (0.85f + rim * 0.45f);

    return float4(saturate(glow), 1.0f);
}
