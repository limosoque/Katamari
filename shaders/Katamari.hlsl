#define MAX_SHOT_LIGHTS 8     //Must match kMaxShotLights on CPU
#define MAX_SHOT_TRAILS 64    //Must match kMaxShotTrailStamps on CPU

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

    float4 ShotLightColorAndRange;                 //rgb = shot color, a = point-light range
    float4 ShotLightCountsAndFlags;                 //x = light count, y = trail count, z = ground mask, w = emissive
    float4 ShotLights[MAX_SHOT_LIGHTS];             //xyz = light position, w = faded intensity
    float4 ShotTrailStamps[MAX_SHOT_TRAILS];        //xy = ground XZ center, z = radius, w = faded intensity
};

Texture2D DiffuseMap : register(t0);
SamplerState TextureSampler : register(s0);

Texture2DArray ShadowMapArray : register(t1);

SamplerState ShadowSampler : register(s1);

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
    float depth : TEXCOORD3;
};

PS_IN VSMain(VS_IN input)
{
    PS_IN output;

    float4 worldPos = mul(float4(input.pos, 1.0f), WorldMatrix);
    float4 viewPos = mul(worldPos, ViewMatrix);

    output.posW = worldPos.xyz;
    output.posH = mul(viewPos, ProjectionMatrix);
    output.normal = normalize(mul(input.normal, (float3x3) WorldMatrix));
    output.uv = input.uv;

    output.depth = viewPos.z;

    return output;
}

float SampleShadowPCF(int cascadeIdx, float2 shadowUV, float fragmentDepth)
{
    float shadow = 0.0f;
    float texelSize = 1.0f / 2048.0f; // TODO: must match kShadowMapSize

    float bias = 0.0002f;

    //3×3 kernel
    [unroll]
    for (int dy = -1; dy <= 1; ++dy)
    {
        [unroll]
        for (int dx = -1; dx <= 1; ++dx)
        {
            float2 offset = float2(dx, dy) * texelSize;
            float3 uvw = float3(shadowUV + offset, (float) cascadeIdx);
            float storedDepth = ShadowMapArray.Sample(ShadowSampler, uvw).r;

            shadow += (fragmentDepth - bias < storedDepth) ? 1.0f : 0.0f; //lil bias to prevent acne
        }
    }

    return shadow / 9.0f; //normalise, where 1 is fully lit
}

float4 ComputeShadowFactor(float3 worldPos, float3 normal, float viewDepth)
{
    //select cascade index
    int cascadeIndex = 2;
    if (viewDepth < CascadeSplits.x)
    {
            cascadeIndex = 0;
    }
    else if (viewDepth < CascadeSplits.y)
    {
        cascadeIndex = 1;
    }

    //debug cascades
    float3 debugColor = float3(1, 1, 1);
    if (cascadeIndex == 0)
    {
        debugColor = float3(1.0, 0.5, 0.5); //red
    }
    else if (cascadeIndex == 1)
    {
        debugColor = float3(0.5, 1.0, 0.5); //green
    }
    else
    {
        debugColor = float3(0.5, 0.5, 1.0); //
    }

    float3 lightDir = normalize(SunlightDirection.xyz);
    float offsetScale = lerp(0.005, 0.05, (float) cascadeIndex / 2.0);
    float3 offsetPos = worldPos + normal * offsetScale;

    //project world position into light clip space
    float4x4 lvp = LightViewProj[cascadeIndex];
    float4 lightClip = mul(float4(offsetPos, 1.0f), lvp);

    //perspective divide
    float3 lightNDC = lightClip.xyz / lightClip.w;

    //convert normalized coords to sh uv
    // NDC x - [-1,+1] = UV x - [0,1]
    // NDC y - [-1,+1] = UV y - [1,0]
    float2 shadowUV;
    shadowUV.x = lightNDC.x * 0.5f + 0.5f;
    shadowUV.y = -lightNDC.y * 0.5f + 0.5f;

    //fragment depth in light space
    float fragmentDepth = lightNDC.z;

    //reject fragments outside the shadow map
    if (shadowUV.x < 0.0f || shadowUV.x > 1.0f ||
        shadowUV.y < 0.0f || shadowUV.y > 1.0f ||
        fragmentDepth < 0.0f || fragmentDepth > 1.0f)
        return 1.0f;

    //PCF
    float shadow = SampleShadowPCF(cascadeIndex, shadowUV, fragmentDepth);

    return float4(debugColor, shadow);
}

float SmoothRadialFalloff(float normalizedDistance)
{
    float x = saturate(1.0f - normalizedDistance);    // 1 at center, 0 at radius edge
    return x * x * (3.0f - 2.0f * x);                 // Smoothstep-like soft edge
}

float3 ComputeShotPointLighting(float3 worldPos, float3 normal, float3 viewDirection)
{
    int lightCount = (int) ShotLightCountsAndFlags.x;      
    float range = max(ShotLightColorAndRange.a, 0.001f);    
    float3 lightColor = ShotLightColorAndRange.rgb;         
    float3 result = float3(0.0f, 0.0f, 0.0f); //accumulated point-light contribution

    [loop]
    for (int i = 0; i < MAX_SHOT_LIGHTS; ++i)
    {
        if (i >= lightCount)
        {
            break;    // Ignore cleared slots
        }

        float3 toLight = ShotLights[i].xyz - worldPos;    // Vector from fragment to shot light
        float dist = max(length(toLight), 0.001f);         // Distance with zero-division guard
        float3 lightDir = toLight / dist;                  // Normalized light direction

        float radial = SmoothRadialFalloff(dist / range);                 //Hard range with soft edge
        float physicalFalloff = 1.0f / (1.0f + dist * dist * 0.12f);       // Extra distance attenuation
        float attenuation = radial * physicalFalloff * ShotLights[i].w;   // Final light strength

        float diffuseAmount = max(dot(normal, lightDir), 0.0f);    // Lambert diffuse term
        float3 reflectionDirection = reflect(-lightDir, normal);   // Reflection vector for specular
        float specularAmount = pow(max(dot(reflectionDirection, viewDirection), 0.0f), MaterialShininess) * 0.35f;    // Softer shot specular

        result += lightColor * attenuation *
            (MaterialDiffuseColor.rgb * diffuseAmount + MaterialSpecularColor.rgb * specularAmount);    // Add material response
    }

    return result;    // Returned in linear RGB-like lighting space
}

float ComputeShotTrailGlow(float3 worldPos, float3 normal)
{
    if (ShotLightCountsAndFlags.z < 0.5f)
    {
        return 0.0f;    // Trail glow is disabled for non-ground objects
    }

    int trailCount = (int) ShotLightCountsAndFlags.y;   
    float glow = 0.0f;                                   // Accumulated ground glow

    [loop]
    for (int i = 0; i < MAX_SHOT_TRAILS; ++i)
    {
        if (i >= trailCount)
        {
            break;    // Ignore cleared slots
        }

        float4 stamp = ShotTrailStamps[i];                  // xy = ground XZ, z = radius, w = intensity
        float radius = max(stamp.z, 0.001f);        
        float d = length(worldPos.xz - stamp.xy);            // Ground-plane distance to stamp center
        float radial = SmoothRadialFalloff(d / radius);      // Soft circular footprint
        float hotCore = radial * radial;                     // Brighter center of the footprint
        glow += (radial * 0.65f + hotCore * 0.35f) * stamp.w;    // Blend halo and core
    }

    float slopeMask = saturate(dot(normal, float3(0.0f, 1.0f, 0.0f)) * 1.25f);    // Fade glow on steep slopes
    return min(glow * slopeMask, 3.0f); // Clamp stacked glow
}

float4 PSMain(PS_IN input) : SV_Target
{
    float3 Normal = normalize(input.normal);
    float3 LightDirection = normalize(SunlightDirection.xyz);
    float3 ViewDirection = normalize(CameraPosition.xyz - input.posW);

    float4 texColor = DiffuseMap.Sample(TextureSampler, input.uv);

    float emissiveAmount = ShotLightCountsAndFlags.w; // Non-zero only for visible shot spheres
    if (emissiveAmount > 0.0f)
    {
        float rim = pow(1.0f - saturate(dot(Normal, ViewDirection)), 2.0f);    // Brighten sphere edge
        float3 glow = ShotLightColorAndRange.rgb * emissiveAmount * (0.85f + rim * 0.45f); // Self-lit sphere color
        return float4(saturate(glow), 1.0f);  // Bypass normal lighting for emissive geometry
    }

    float3 ambient = MaterialAmbientColor.rgb * SunlightColor.rgb;

    float diffuseIntensity = max(dot(Normal, LightDirection), 0.0f); // Sun Lambert term
    float3 diffuse = diffuseIntensity * MaterialDiffuseColor.rgb * SunlightColor.rgb;

    //Calc reflection vector
    float3 ReflectionDirection = reflect(-LightDirection, Normal);
    float specularIntensity = pow(max(dot(ReflectionDirection, ViewDirection), 0.0f), MaterialShininess);
    float3 specular = specularIntensity * MaterialSpecularColor.rgb * SunlightColor.rgb;

    float4 shadowData = ComputeShadowFactor(input.posW, Normal, input.depth);

    float3 debugTint = shadowData.xyz;
    float shadowAmt = shadowData.w;

    float3 sunLighting = (ambient + shadowAmt * (diffuse + specular)) * texColor.rgb;
    float3 shotLighting = ComputeShotPointLighting(input.posW, Normal, ViewDirection) * texColor.rgb;    // Dynamic projectile lights
    float trailGlow = ComputeShotTrailGlow(input.posW, Normal);                                         // Ground-only glow mask
    float3 finalColor = sunLighting + shotLighting + ShotLightColorAndRange.rgb * trailGlow;             // Combine all lighting

    //return float4(saturate(finalColor * debugTint), texColor.a);
    return float4(saturate(finalColor), texColor.a);
}
