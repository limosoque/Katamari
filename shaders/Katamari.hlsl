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

    // 3×3 kernel
    [unroll]
    for (int dy = -1; dy <= 1; ++dy)
    {
        [unroll]
        for (int dx = -1; dx <= 1; ++dx)
        {
            float2 offset = float2(dx, dy) * texelSize;
            float3 uvw = float3(shadowUV + offset, (float) cascadeIdx);
            float storedDepth = ShadowMapArray.Sample(ShadowSampler, uvw).r;

            // Compare fragment depth against stored depth.
            // A small bias (0.001) prevents shadow acne — self-shadowing due to
            // floating-point precision.  The slope-scaled bias in the rasterizer
            // state handles most of it; this is a safety margin.
            shadow += (fragmentDepth - 0.001f < storedDepth) ? 1.0f : 0.0f;
        }
    }

    return shadow / 9.0f; // normalise: 1 = fully lit, 0 = fully in shadow
}

float4 ComputeShadowFactor(float3 worldPos, float viewDepth)
{
    // ── 1. Select cascade index ───────────────────────────────────────────────
    int cascadeIndex = 2; // default: furthest cascade
    if (viewDepth < CascadeSplits.x)
        cascadeIndex = 0;
    else if (viewDepth < CascadeSplits.y)
        cascadeIndex = 1;
    
    float3 debugColor = float3(1, 1, 1);
    if (cascadeIndex == 0)
        debugColor = float3(1.0, 0.5, 0.5); // Первый каскад — Красный
    else if (cascadeIndex == 1)
        debugColor = float3(0.5, 1.0, 0.5); // Второй — Зеленый
    else
        debugColor = float3(0.5, 0.5, 1.0); // Третий — Синий

    // ── 2. Project world position into light clip space ───────────────────────
    float4x4 lvp = LightViewProj[cascadeIndex];
    float4 lightClip = mul(float4(worldPos, 1.0f), lvp);

    // Perspective divide (for ortho proj w==1, but good habit)
    float3 lightNDC = lightClip.xyz / lightClip.w;

    // ── 3. Convert NDC → shadow map UV ────────────────────────────────────────
    // NDC x ∈ [-1,+1] → UV x ∈ [0,1]
    // NDC y ∈ [-1,+1] → UV y ∈ [1,0]  (flipped)
    float2 shadowUV;
    shadowUV.x = lightNDC.x * 0.5f + 0.5f;
    shadowUV.y = -lightNDC.y * 0.5f + 0.5f;

    // Fragment depth in light space (already [0,1] for D3D ortho)
    float fragmentDepth = lightNDC.z;

    // ── 4. Reject fragments outside the shadow map ────────────────────────────
    // If the UV is outside [0,1] or depth is out of range, assume lit.
    if (shadowUV.x < 0.0f || shadowUV.x > 1.0f ||
        shadowUV.y < 0.0f || shadowUV.y > 1.0f ||
        fragmentDepth < 0.0f || fragmentDepth > 1.0f)
        return 1.0f;

    // ── 5. PCF sample from the correct cascade ────────────────────────────────
    float shadow = SampleShadowPCF(cascadeIndex, shadowUV, fragmentDepth);

    return float4(debugColor, shadow);
}


float4 PSMain(PS_IN input) : SV_Target
{
    float3 Normal = normalize(input.normal);
    float3 LightDirection = normalize(SunlightDirection.xyz);
    float3 ViewDirection = normalize(CameraPosition.xyz - input.posW);
    
    float4 texColor = DiffuseMap.Sample(TextureSampler, input.uv);
       
    float3 ambient = MaterialAmbientColor.rgb * SunlightColor.rgb;

    float diffuseIntensity = max(dot(Normal, LightDirection), 0.0f);
    float3 diffuse = diffuseIntensity * MaterialDiffuseColor.rgb * SunlightColor.rgb;

    //Calc reflection vector
    float3 ReflectionDirection = reflect(-LightDirection, Normal);
    float specularIntensity = pow(max(dot(ReflectionDirection, ViewDirection), 0.0f), MaterialShininess);
    float3 specular = specularIntensity * MaterialSpecularColor.rgb * SunlightColor.rgb;
    
    //float shadow = ComputeShadowFactor(input.posW, input.depth);
    //float viewDepth = input.posH.w;
    float4 shadowData = ComputeShadowFactor(input.posW, input.depth);
    
    float3 debugTint = shadowData.xyz;
    float shadowAmt = shadowData.w;
    
    //ambient + diffuse is the light contribution, painted by the texture color, then add the specular highlight
    //float3 finalColor = (ambient + shadow * (diffuse + specular)) * texColor.rgb;
    float3 finalColor = (ambient + shadowAmt * (diffuse + specular)) * texColor.rgb;

    return float4(saturate(finalColor * debugTint), texColor.a);
    //return float4(saturate(finalColor), texColor.a);
}