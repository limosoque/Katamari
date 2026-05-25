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
    float3 Padding;

    float4 SunlightColor;
    float4 SunlightDirection;
    float4 CameraPosition;

    float4x4 LightViewProj[3];
    float4 CascadeSplits;

    float4 ShotLightColorAndRange;
    float4 ShotLightCountsAndFlags;
    float4 ShotLights[MAX_SHOT_LIGHTS];
    float4 ShotTrailStamps[MAX_SHOT_TRAILS];
};

Texture2D DiffuseMap : register(t0);
SamplerState TextureSampler : register(s0);

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
    float2 uv : TEXCOORD2;
    float viewDepth : TEXCOORD3;
};

struct GBufferOutput
{
    float4 AlbedoGroundMask : SV_Target0;
    float4 NormalShininess : SV_Target1;
    float4 SpecularViewDepth : SV_Target2;
    float4 WorldPosition : SV_Target3;
    float4 Ambient : SV_Target4;
};

PS_IN VSMain(VS_IN input)
{
    PS_IN output;

    float4 worldPos = mul(float4(input.pos, 1.0f), WorldMatrix);
    float4 viewPos = mul(worldPos, ViewMatrix);

    output.posW = worldPos.xyz;
    output.posH = mul(viewPos, ProjectionMatrix);
    output.normalW = normalize(mul(input.normal, (float3x3) WorldMatrix));
    output.uv = input.uv;
    output.viewDepth = viewPos.z;

    return output;
}

[earlydepthstencil]
GBufferOutput PSMain(PS_IN input)
{
    GBufferOutput output;

    float4 texColor = DiffuseMap.Sample(TextureSampler, input.uv);
    float3 albedo = texColor.rgb * MaterialDiffuseColor.rgb;
    float groundTrailMask = ShotLightCountsAndFlags.z;

    output.AlbedoGroundMask = float4(albedo, groundTrailMask);
    output.NormalShininess = float4(normalize(input.normalW), MaterialShininess);
    output.SpecularViewDepth = float4(MaterialSpecularColor.rgb, input.viewDepth);
    output.WorldPosition = float4(input.posW, 1.0f);
    output.Ambient = float4(MaterialAmbientColor.rgb, 1.0f);

    return output;
}
