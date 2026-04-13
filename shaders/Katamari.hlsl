// Katamari.hlsl — Blinn-Phong shader

cbuffer PerObject : register(b0)
{
    float4x4 gModel;
    float4x4 gView;
    float4x4 gProjection;
    float4 gColor;
    float4 gLightDir;
    float4 gCameraPos;
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
    float3 normal : TEXCOORD1;
};

PS_IN VSMain(VS_IN input)
{
    PS_IN o;
    float4 worldPos = mul(float4(input.pos, 1.0f), gModel);
    o.posW = worldPos.xyz;
    o.posH = mul(mul(worldPos, gView), gProjection);
    o.normal = normalize(mul(input.normal, (float3x3) gModel));
    return o;
}

float4 PSMain(PS_IN input) : SV_Target
{
    float3 N = normalize(input.normal);
    float3 L = normalize(gLightDir.xyz);
    float3 V = normalize(gCameraPos.xyz - input.posW);
    float3 H = normalize(L + V);

    float ambient = 0.25f;
    float diffuse = max(dot(N, L), 0.0f);
    float specular = pow(max(dot(N, H), 0.0f), 32.0f) * 0.3f;

    float3 col = gColor.rgb * (ambient + diffuse) + float3(1.0f, 1.0f, 1.0f) * specular;
    return float4(saturate(col), gColor.a);
}