// ShadowPass.hlsl
// Renders scene depth into a Texture2DArray for CSM.
//
// Pipeline: VS → GS → (no PS) → DSV[array]
//
// The Geometry Shader is instanced kCascadeCount times.
// Each instance writes the triangle into one array slice using
// SV_RenderTargetArrayIndex, so all cascades are filled in a single draw call.

// ─── Constant buffers ─────────────────────────────────────────────────────────

// b0 — per-object world matrix (updated per draw call, same as main pass).
cbuffer PerObjectShadow : register(b0)
{
    float4x4 WorldMatrix;
};

// b1 — cascade data: all light ViewProj matrices + split distances.
// Matches CascadeData layout described by the teacher.
cbuffer CascadeBuf : register(b1)
{
    float4x4 LightViewProj[3]; // one per cascade (already transposed on CPU)
    float4 CascadeSplits; // xyz = split depths, w unused
};

// ─── Structs ──────────────────────────────────────────────────────────────────

// VS output: only world-space position is needed.
// GS will project it per-cascade.
struct GS_IN
{
    float4 posW : POSITION; // world-space position (not yet projected)
};

// GS output: clip-space position + target array slice index.
struct GS_OUT
{
    float4 posH : SV_POSITION;
    uint arrIdx : SV_RenderTargetArrayIndex; // which cascade slice to write
};

// ─── Vertex Shader ────────────────────────────────────────────────────────────
// Only transforms to world space. Projection happens in the GS per-cascade.

GS_IN VShadow(float3 pos : POSITION,
              float3 normal : NORMAL, // unused — matches main pass layout
              float2 uv : TEXCOORD0)  // unused
{
    GS_IN o;
    o.posW = mul(float4(pos, 1.0f), WorldMatrix);
    return o;
}

// ─── Geometry Shader ──────────────────────────────────────────────────────────
//
// [instance(N)] — the GS runs N times per input primitive, each time with a
//   different SV_GSInstanceID (0..N-1). This is the key instruction from the
//   teacher: one draw call fills all cascade slices simultaneously.
//
// [maxvertexcount(3)] — each GS invocation emits at most 3 vertices (one triangle).
//
// For each instance (= cascade index), we project all 3 input vertices with that
// cascade's LightViewProj and write SV_RenderTargetArrayIndex = instance id.
// The rasterizer then routes the triangle to that DSV array slice.

[instance(3)] // 3 = kCascadeCount; must be a literal here
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
        // Project world-space vertex into this cascade's light clip space.
        o.posH = mul(float4(input[v].posW.xyz, 1.0f), LightViewProj[instanceId]);
        o.arrIdx = instanceId;
        stream.Append(o);
    }
    stream.RestartStrip();
}

// No pixel shader — depth is written automatically by the rasterizer.
