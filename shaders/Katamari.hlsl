#include "../LightSource.h"

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
    
    int ActiveLightCount;
    float3 PaddingLights;

    LightSource Lights[MAX_LIGHTS];
    
    float4x4 LightViewProj[3];
    float4 CascadeSplits;
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

            shadow += (fragmentDepth + 0.001f < storedDepth) ? 1.0f : 0.0f; //lil bias to prevent acne
        }
    }

    return shadow / 9.0f; //normalise, where 1 is fully lit
}

float4 ComputeShadowFactor(float3 worldPos, float viewDepth)
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

    //project world position into light clip space
    float4 lightClip = mul(float4(worldPos, 1.0f), LightViewProj[cascadeIndex]);

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
    {
        return 1.0f;
    }

    //PCF
    float shadow = SampleShadowPCF(cascadeIndex, shadowUV, fragmentDepth);

    return float4(debugColor, shadow);
}

float3 ComputeLight(LightSource light, 
                    float3 pixelWorldPos, 
                    float3 pixelNormal, 
                    float3 vectorToCamera, 
                    float materialShininess, 
                    float3 materialSpecularColor)
{
    float3 directionToLight;
    float distanceAttenuation = 1.f;
    
    //directional
    if (light.Type == 0)
    {
        directionToLight = normalize(light.Position.xyz);
        distanceAttenuation = 1.f;
    }
    //point
    else
    {
        float3 vectorToLightSource = light.Position.xyz - pixelWorldPos;
        float distanceToLight = length(vectorToLightSource);
        
        //normalize
        directionToLight = vectorToLightSource / distanceToLight;
        
        //calculate attenuation - 0 in the center, 1 in the corner of range radius
        float normalizedDistance = saturate(distanceToLight / light.Range);
        
        //smooth attenuation
        distanceAttenuation = pow(1.f - normalizedDistance, 2.f);
    }
    
    //diffuse lighting
    float diffuseIntensity = max(dot(pixelNormal, directionToLight), 0.f);
    
    //glare lighing
    float3 reflectionVector = reflect(-directionToLight, pixelNormal);
    float specularIntensity = pow(max(dot(reflectionVector, vectorToCamera), 0.f), materialShininess);

    float3 lightColorWithIntensity = light.Color.rgb * light.Color.a;
    
    float3 diffusePart = diffuseIntensity * lightColorWithIntensity;
    float3 specularPart = specularIntensity * materialSpecularColor * lightColorWithIntensity;
    
    return (diffusePart + specularPart) * distanceAttenuation;
}

float4 PSMain(PS_IN input) : SV_Target
{
    float3 Normal = normalize(input.normal);
    float3 LightDirection = normalize(SunlightDirection.xyz);
    float3 ViewDirection = normalize(CameraPosition.xyz - input.posW);
    
    
    float4 shadowData = ComputeShadowFactor(input.posW, input.depth);
    float3 debugTint = shadowData.xyz;
    float shadow = shadowData.w;
    
    float3 totalLighting = MaterialAmbientColor.rgb;
    for (int i = 0; i < ActiveLightCount; ++i)
    {
        float currentShadow = (i == 0) ? shadow : 1.f;
        
        totalLighting += ComputeLight(Lights[i], input.posW, Normal, ViewDirection, MaterialShininess, currentShadow);
    }    
    
    float4 texColor = DiffuseMap.Sample(TextureSampler, input.uv);
    float3 finalColor = totalLighting * texColor.rgb;

    //return float4(saturate(finalColor * debugTint), texColor.a);
    return float4(saturate(finalColor), texColor.a);
}