cbuffer ParticleFrameCB : register(b0)
{
    float4x4 ViewProjection;
    float4x4 ViewMatrix;
    float4 CameraRight;
    float4 CameraUp;
    float4 ScreenSize;
    float4 Params; // x = soft particle fade distance.
};

Texture2D<float4> SceneSpecularViewDepth : register(t0);

struct VS_IN
{
    float3 position : POSITION;
    float size : TEXCOORD0;
    float4 color : COLOR0;
    float4 params : TEXCOORD1; // x = rotation, y = edge softness, z = normalized age, w = brightness.
};

struct VS_OUT
{
    float3 position : POSITION;
    float size : TEXCOORD0;
    float4 color : COLOR0;
    float4 params : TEXCOORD1;
};

struct GS_OUT
{
    float4 posH : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
    float4 params : TEXCOORD1;
    float viewDepth : TEXCOORD2;
};

VS_OUT VSMain(VS_IN input)
{
    VS_OUT output;
    output.position = input.position;
    output.size = input.size;
    output.color = input.color;
    output.params = input.params;
    return output;
}

[maxvertexcount(4)]
void GSMain(point VS_OUT input[1], inout TriangleStream<GS_OUT> stream)
{
    float halfSize = input[0].size * 0.5f;
    float rotation = input[0].params.x;
    float sinRotation = sin(rotation);
    float cosRotation = cos(rotation);

    float2 corners[4] =
    {
        float2(-1.0f,  1.0f),
        float2( 1.0f,  1.0f),
        float2(-1.0f, -1.0f),
        float2( 1.0f, -1.0f)
    };

    float2 uvs[4] =
    {
        float2(0.0f, 0.0f),
        float2(1.0f, 0.0f),
        float2(0.0f, 1.0f),
        float2(1.0f, 1.0f)
    };

    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        float2 corner = corners[i];
        float2 rotatedCorner = float2(
            corner.x * cosRotation - corner.y * sinRotation,
            corner.x * sinRotation + corner.y * cosRotation);

        float3 worldPos =
            input[0].position +
            CameraRight.xyz * rotatedCorner.x * halfSize +
            CameraUp.xyz * rotatedCorner.y * halfSize;

        float4 viewPos = mul(float4(worldPos, 1.0f), ViewMatrix);

        GS_OUT output;
        output.posH = mul(float4(worldPos, 1.0f), ViewProjection);
        output.uv = uvs[i];
        output.color = input[0].color;
        output.params = input[0].params;
        output.viewDepth = viewPos.z;
        stream.Append(output);
    }
}

float4 PSMain(GS_OUT input) : SV_Target
{
    float2 centeredUv = input.uv * 2.0f - 1.0f;
    float radiusSq = dot(centeredUv, centeredUv);
    if (radiusSq > 1.0f)
    {
        discard;
    }

    float radial = saturate(1.0f - radiusSq);
    float edge = pow(radial, max(input.params.y, 0.05f));
    float alpha = input.color.a * edge;

    int2 pixel = int2(input.posH.xy);
    float sceneViewDepth = SceneSpecularViewDepth.Load(int3(pixel, 0)).a;
    if (sceneViewDepth > 0.0001f)
    {
        float fadeDistance = max(Params.x, 0.001f);
        alpha *= saturate((sceneViewDepth - input.viewDepth) / fadeDistance);
    }

    clip(alpha - 0.002f);

    float age = saturate(input.params.z);
    float brightness = input.params.w;
    float glow = lerp(1.35f, 0.72f, age) * (0.65f + radial * 0.7f);
    float3 color = saturate(input.color.rgb * brightness * glow);

    return float4(color, alpha);
}
