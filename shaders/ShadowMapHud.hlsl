cbuffer ShadowMapHudCB : register(b0)
{
    float4 PanelRect;      // x, y, width, height in screen pixels.
    float4 ScreenSize;     // width, height, 1 / width, 1 / height.
    float4 Params;         // x = cascade index, y = exposure, z = contour mode, w = border px.
    float4 BorderColor;    // rgba debug border color.
};

Texture2DArray ShadowMapArray : register(t0);
SamplerState HudSampler : register(s0);

struct VS_OUT
{
    float4 posH : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VS_OUT VSMain(uint vertexId : SV_VertexID)
{
    float2 corners[6] =
    {
        float2(0.0f, 0.0f),
        float2(1.0f, 0.0f),
        float2(0.0f, 1.0f),
        float2(0.0f, 1.0f),
        float2(1.0f, 0.0f),
        float2(1.0f, 1.0f)
    };

    float2 uv = corners[vertexId];
    float2 pixelPos = PanelRect.xy + uv * PanelRect.zw;
    float2 ndc = float2(
        pixelPos.x * ScreenSize.z * 2.0f - 1.0f,
        1.0f - pixelPos.y * ScreenSize.w * 2.0f);

    VS_OUT output;
    output.posH = float4(ndc, 0.0f, 1.0f);
    output.uv = uv;
    return output;
}

float4 PSMain(VS_OUT input) : SV_Target
{
    float cascadeIndex = Params.x;
    float exposure = max(Params.y, 0.001f);
    bool contourMode = Params.z > 0.5f;
    float2 borderUv = Params.w / max(PanelRect.zw, float2(1.0f, 1.0f));

    float depth = ShadowMapArray.SampleLevel(HudSampler, float3(input.uv, cascadeIndex), 0.0f).r;
    float validDepth = depth < 0.9999f ? 1.0f : 0.0f;
    float value = saturate((1.0f - depth) * exposure) * validDepth;
    float2 texel = float2(1.0f / 2048.0f, 1.0f / 2048.0f);
    float depthRight = ShadowMapArray.SampleLevel(HudSampler, float3(input.uv + float2(texel.x, 0.0f), cascadeIndex), 0.0f).r;
    float depthDown = ShadowMapArray.SampleLevel(HudSampler, float3(input.uv + float2(0.0f, texel.y), cascadeIndex), 0.0f).r;
    float depthEdge = max(abs(depth - depthRight), abs(depth - depthDown));
    float edgeLine = smoothstep(0.00004f, 0.0015f, depthEdge) * validDepth;

    if (contourMode)
    {
        float bandScale = max(exposure * 8.0f, 1.0f);
        float band = abs(frac(depth * bandScale) * 2.0f - 1.0f);
        float contour = (1.0f - smoothstep(0.0f, 0.12f, band)) * validDepth;
        value = saturate(value * 0.65f + contour * 0.85f);
    }

    bool border =
        input.uv.x < borderUv.x || input.uv.x > 1.0f - borderUv.x ||
        input.uv.y < borderUv.y || input.uv.y > 1.0f - borderUv.y;

    float3 depthColor = lerp(float3(value, value, value), float3(0.0f, 0.0f, 0.0f), edgeLine);
    float3 color = border ? BorderColor.rgb : depthColor;
    return float4(color, 1.0f);
}
