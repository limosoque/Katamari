#define MAX_DEFERRED_POINT_LIGHTS 16
#define MAX_DEFERRED_SPOT_LIGHTS 8
#define MAX_DEFERRED_TRAILS 64

cbuffer DeferredLightingCB : register(b0)
{
    float4 DirectionalDirection;    // xyz = direction from surface to light.
    float4 DirectionalColor;        // rgb = color, a = intensity.
    float4 CameraPosition;          // xyz = camera world position.
    float4 CascadeSplits;           // xyz = cascade far distances in view space.
    float4 SkyColor;                // rgb = background for pixels not written to the GBuffer.
    float4 TrailColor;              // rgb = emissive ground trail color.
    float4 LightCounts;             // x = point count, y = spot count, z = trail count.
    float4 Padding;

    float4x4 LightViewProj[3];

    float4 PointPositionRange[MAX_DEFERRED_POINT_LIGHTS];
    float4 PointColorIntensity[MAX_DEFERRED_POINT_LIGHTS];

    float4 SpotPositionRange[MAX_DEFERRED_SPOT_LIGHTS];
    float4 SpotDirectionOuterCos[MAX_DEFERRED_SPOT_LIGHTS];
    float4 SpotColorIntensity[MAX_DEFERRED_SPOT_LIGHTS];
    float4 SpotConeCos[MAX_DEFERRED_SPOT_LIGHTS];

    float4 TrailStamps[MAX_DEFERRED_TRAILS];
};

Texture2D<float4> GBufferAlbedoGroundMask : register(t0);
Texture2D<float4> GBufferNormalShininess : register(t1);
Texture2D<float4> GBufferSpecularViewDepth : register(t2);
Texture2D<float4> GBufferWorldPosition : register(t3);
Texture2D<float4> GBufferAmbient : register(t4);
Texture2DArray ShadowMapArray : register(t5);

SamplerState ShadowSampler : register(s1);

struct PS_IN
{
    float4 posH : SV_POSITION;
    float2 uv : TEXCOORD0;
};

PS_IN VSMain(uint vertexId : SV_VertexID)
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

    PS_IN output;
    output.posH = float4(uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    output.uv = uv;
    return output;
}

float SmoothRadialFalloff(float normalizedDistance)
{
    float x = saturate(1.0f - normalizedDistance);
    return x * x * (3.0f - 2.0f * x);
}

float SampleShadowPCF(int cascadeIdx, float2 shadowUV, float fragmentDepth)
{
    float shadow = 0.0f;
    float texelSize = 1.0f / 2048.0f;
    float bias = 0.0002f;

    [unroll]
    for (int dy = -1; dy <= 1; ++dy)
    {
        [unroll]
        for (int dx = -1; dx <= 1; ++dx)
        {
            float2 offset = float2(dx, dy) * texelSize;
            float storedDepth = ShadowMapArray.Sample(ShadowSampler, float3(shadowUV + offset, (float)cascadeIdx)).r;
            shadow += (fragmentDepth - bias < storedDepth) ? 1.0f : 0.0f;
        }
    }

    return shadow / 9.0f;
}

float ComputeShadowFactor(float3 worldPos, float3 normal, float viewDepth)
{
    int cascadeIndex = 2;
    if (viewDepth < CascadeSplits.x)
    {
        cascadeIndex = 0;
    }
    else if (viewDepth < CascadeSplits.y)
    {
        cascadeIndex = 1;
    }

    float offsetScale = lerp(0.005f, 0.05f, (float)cascadeIndex / 2.0f);
    float3 offsetPos = worldPos + normal * offsetScale;

    float4 lightClip = mul(float4(offsetPos, 1.0f), LightViewProj[cascadeIndex]);
    float3 lightNDC = lightClip.xyz / lightClip.w;

    float2 shadowUV;
    shadowUV.x = lightNDC.x * 0.5f + 0.5f;
    shadowUV.y = -lightNDC.y * 0.5f + 0.5f;

    if (shadowUV.x < 0.0f || shadowUV.x > 1.0f ||
        shadowUV.y < 0.0f || shadowUV.y > 1.0f ||
        lightNDC.z < 0.0f || lightNDC.z > 1.0f)
    {
        return 1.0f;
    }

    return SampleShadowPCF(cascadeIndex, shadowUV, lightNDC.z);
}

float3 ComputePhongContribution(
    float3 lightDir,
    float3 lightColor,
    float3 albedo,
    float3 normal,
    float3 viewDir,
    float3 specularColor,
    float shininess)
{
    float diffuseAmount = max(dot(normal, lightDir), 0.0f);
    float3 reflectionDirection = reflect(-lightDir, normal);
    float specularAmount = pow(max(dot(reflectionDirection, viewDir), 0.0f), shininess);

    return lightColor * (albedo * diffuseAmount + specularColor * specularAmount);
}

float3 AccumulatePointLights(
    float3 worldPos,
    float3 normal,
    float3 viewDir,
    float3 albedo,
    float3 specularColor,
    float shininess)
{
    float3 result = float3(0.0f, 0.0f, 0.0f);
    int pointCount = (int)LightCounts.x;

    [loop]
    for (int i = 0; i < MAX_DEFERRED_POINT_LIGHTS; ++i)
    {
        if (i >= pointCount)
        {
            break;
        }

        float3 toLight = PointPositionRange[i].xyz - worldPos;
        float distanceToLight = max(length(toLight), 0.001f);
        float range = max(PointPositionRange[i].w, 0.001f);
        float3 lightDir = toLight / distanceToLight;

        float radial = SmoothRadialFalloff(distanceToLight / range);
        float physicalFalloff = 1.0f / (1.0f + distanceToLight * distanceToLight * 0.12f);
        float attenuation = radial * physicalFalloff * PointColorIntensity[i].w;

        result += attenuation * ComputePhongContribution(
            lightDir,
            PointColorIntensity[i].rgb,
            albedo,
            normal,
            viewDir,
            specularColor,
            shininess);
    }

    return result;
}

float3 AccumulateSpotLights(
    float3 worldPos,
    float3 normal,
    float3 viewDir,
    float3 albedo,
    float3 specularColor,
    float shininess)
{
    float3 result = float3(0.0f, 0.0f, 0.0f);
    int spotCount = (int)LightCounts.y;

    [loop]
    for (int i = 0; i < MAX_DEFERRED_SPOT_LIGHTS; ++i)
    {
        if (i >= spotCount)
        {
            break;
        }

        float3 toLight = SpotPositionRange[i].xyz - worldPos;
        float distanceToLight = max(length(toLight), 0.001f);
        float range = max(SpotPositionRange[i].w, 0.001f);
        float3 lightDir = toLight / distanceToLight;

        float3 lightToPixel = -lightDir;
        float3 spotDirection = normalize(SpotDirectionOuterCos[i].xyz);
        float innerCos = SpotConeCos[i].x;
        float outerCos = SpotConeCos[i].y;
        float cone = saturate((dot(lightToPixel, spotDirection) - outerCos) / max(innerCos - outerCos, 0.001f));
        cone = cone * cone * (3.0f - 2.0f * cone);

        float radial = SmoothRadialFalloff(distanceToLight / range);
        float physicalFalloff = 1.0f / (1.0f + distanceToLight * distanceToLight * 0.08f);
        float attenuation = cone * radial * physicalFalloff * SpotColorIntensity[i].w;

        result += attenuation * ComputePhongContribution(
            lightDir,
            SpotColorIntensity[i].rgb,
            albedo,
            normal,
            viewDir,
            specularColor,
            shininess);
    }

    return result;
}

float ComputeTrailGlow(float3 worldPos, float3 normal, float groundMask)
{
    if (groundMask < 0.5f)
    {
        return 0.0f;
    }

    float glow = 0.0f;
    int trailCount = (int)LightCounts.z;

    [loop]
    for (int i = 0; i < MAX_DEFERRED_TRAILS; ++i)
    {
        if (i >= trailCount)
        {
            break;
        }

        float4 stamp = TrailStamps[i];
        float radius = max(stamp.z, 0.001f);
        float distanceOnGround = length(worldPos.xz - stamp.xy);
        float radial = SmoothRadialFalloff(distanceOnGround / radius);
        float hotCore = radial * radial;
        glow += (radial * 0.65f + hotCore * 0.35f) * stamp.w;
    }

    float slopeMask = saturate(dot(normal, float3(0.0f, 1.0f, 0.0f)) * 1.25f);
    return min(glow * slopeMask, 3.0f);
}

float4 PSMain(PS_IN input) : SV_Target
{
    int2 pixel = int2(input.posH.xy);

    float4 specularViewDepth = GBufferSpecularViewDepth.Load(int3(pixel, 0));
    float viewDepth = specularViewDepth.a;
    if (viewDepth <= 0.0001f)
    {
        return float4(SkyColor.rgb, 1.0f);
    }

    float4 albedoGroundMask = GBufferAlbedoGroundMask.Load(int3(pixel, 0));
    float4 normalShininess = GBufferNormalShininess.Load(int3(pixel, 0));
    float4 worldPosition = GBufferWorldPosition.Load(int3(pixel, 0));
    float4 ambient = GBufferAmbient.Load(int3(pixel, 0));

    float3 albedo = albedoGroundMask.rgb;
    float groundMask = albedoGroundMask.a;
    float3 normal = normalize(normalShininess.xyz);
    float shininess = max(normalShininess.a, 1.0f);
    float3 worldPos = worldPosition.xyz;
    float3 specularColor = specularViewDepth.rgb;
    float3 viewDir = normalize(CameraPosition.xyz - worldPos);

    float3 directionalDir = normalize(DirectionalDirection.xyz);
    float3 directionalColor = DirectionalColor.rgb * DirectionalColor.a;
    float shadow = ComputeShadowFactor(worldPos, normal, viewDepth);

    float3 color = ambient.rgb * directionalColor * albedo;
    color += shadow * ComputePhongContribution(
        directionalDir,
        directionalColor,
        albedo,
        normal,
        viewDir,
        specularColor,
        shininess);

    color += AccumulatePointLights(worldPos, normal, viewDir, albedo, specularColor, shininess);
    color += AccumulateSpotLights(worldPos, normal, viewDir, albedo, specularColor, shininess);
    color += TrailColor.rgb * ComputeTrailGlow(worldPos, normal, groundMask);

    return float4(saturate(color), 1.0f);
}
