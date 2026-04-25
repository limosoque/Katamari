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
    float3 normal : TEXCOORD1;
    float2 uv : TEXCOORD2;
};

PS_IN VSMain(VS_IN input)
{
    PS_IN output;
    float4 worldPos = mul(float4(input.pos, 1.0f), WorldMatrix);
    output.posW = worldPos.xyz;
    output.posH = mul(mul(worldPos, ViewMatrix), ProjectionMatrix);
    output.normal = normalize(mul(input.normal, (float3x3) WorldMatrix));
    output.uv = input.uv;
    return output;
}

float4 PSMain(PS_IN input) : SV_Target
{
    float3 Normal = normalize(input.normal);
    float3 LightDirection = normalize(SunlightDirection.xyz);
    float3 ViewDirection = normalize(CameraPosition.xyz - input.posW);
       
    float3 ambient = MaterialAmbientColor.rgb * SunlightColor.rgb;

    float diffuseIntensity = max(dot(Normal, LightDirection), 0.0f);
    float3 diffuse = diffuseIntensity * MaterialDiffuseColor.rgb * SunlightColor.rgb;

    //Calc reflection vector
    float3 ReflectionDirection = reflect(-LightDirection, Normal);
    float specularIntensity = pow(max(dot(ReflectionDirection, ViewDirection), 0.0f), MaterialShininess);
    float3 specular = specularIntensity * MaterialSpecularColor.rgb * SunlightColor.rgb;

    float4 texColor = DiffuseMap.Sample(TextureSampler, input.uv);
    
    //ambient + diffuse is the light contribution, painted by the texture color, then add the specular highlight
    float3 finalColor = (ambient + diffuse) * texColor.rgb + specular;

    return float4(saturate(finalColor), texColor.a);
}