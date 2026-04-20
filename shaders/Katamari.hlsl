cbuffer PerObject : register(b0)
{
    float4x4 gModel;
    float4x4 gView;
    float4x4 gProjection;
    float4 gColor;
    float4 gLightDir;
    float4 gCameraPos;
};

Texture2D gDiffuseMap : register(t0);
SamplerState gSampler : register(s0);

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
    float2 uv : TEXCOORD2;
};

PS_IN VSMain(VS_IN input)
{
    PS_IN o;
    float4 worldPos = mul(float4(input.pos, 1.0f), gModel);
    o.posW = worldPos.xyz;
    o.posH = mul(mul(worldPos, gView), gProjection);
    o.normal = normalize(mul(input.normal, (float3x3) gModel));
    o.uv = input.uv;
    return o;
}

float4 PSMain(PS_IN input) : SV_Target
{
    float3 N = normalize(input.normal);
    float3 L = normalize(gLightDir.xyz);
    float3 V = normalize(gCameraPos.xyz - input.posW);
    float3 H = normalize(L + V);
    
    float4 texColor = gDiffuseMap.Sample(gSampler, input.uv);

    float ambient = 0.25f;
    float diffuse = max(dot(N, L), 0.0f);
    float specular = pow(max(dot(N, H), 0.0f), 32.0f) * 0.3f;
    
    float3 finalColor = texColor.rgb * gColor.rgb * (ambient + diffuse) + specular;

    return float4(saturate(finalColor), gColor.a * texColor.a);
}