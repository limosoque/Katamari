#define MAX_PARTICLE_SDF_SPHERES 8

struct GpuParticle
{
    float4 PositionAge;       // xyz = world position, w = age.
    float4 VelocityLifetime;  // xyz = velocity, w = lifetime.
    float4 StartColor;
    float4 EndColor;
    float4 SizeRotation;      // x = start size, y = end size, z = rotation, w = angular velocity.
    float4 ParticleParams;    // x = weight, y = edge softness, z = brightness, w = alive flag.
};

cbuffer ParticleComputeCB : register(b0)
{
    float4 GravityDrag;       // xyz = gravity, w = drag.
    float4 WindDt;            // xyz = wind, w = delta time.
    float4 OriginCount;       // xyz = emit origin, w = emit count.
    float4 DirectionSpread;   // xyz = emit direction, w = cone spread in radians.
    float4 SpeedLifetime;     // x/y = speed min/max, z/w = lifetime min/max.
    float4 SizeRanges;        // x/y = start size min/max, z/w = end size min/max.
    float4 WeightRotation;    // x/y = weight min/max, z/w = angular speed min/max.
    float4 StartColor;
    float4 EndColor;
    float4 EmitParams;        // x = edge softness, y/z = brightness min/max.
    float4 SpawnInfo;         // x = spawn start index, y = random seed, z = max particles.
    float4 EmitWidthAxis;     // xyz = axis, w = full width.
    float4 EmitDepthAxis;     // xyz = axis, w = full depth.
    float4 SdfCenterRadius[MAX_PARTICLE_SDF_SPHERES]; // xyz = SDF sphere center, w = radius.
    float4 SdfVelocityInfluence[MAX_PARTICLE_SDF_SPHERES]; // xyz = sphere velocity, w = influence distance.
    float4 SdfParams[MAX_PARTICLE_SDF_SPHERES]; // x = repel strength, y = surface offset, z = velocity transfer, w = enabled.
    float4 SdfControl;        // x = active SDF sphere count.
    float4 TerrainParams;     // x = terrain amplitude, y = terrain frequency, z = normal sample offset.
    float4 GroundCollisionParams; // x = enabled, y = surface offset, z = restitution, w = friction.
};

RWStructuredBuffer<GpuParticle> Particles : register(u0);

uint Hash(uint value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

float Random01(inout uint state)
{
    state = Hash(state);
    return (float)(state & 0x00ffffffu) / 16777216.0f;
}

float RandomRange(inout uint state, float lo, float hi)
{
    return lerp(lo, hi, Random01(state));
}

float3 RandomDirectionInCone(float3 direction, float spreadRadians, inout uint state)
{
    float3 base = direction;
    if (dot(base, base) < 0.0001f)
    {
        base = float3(0.0f, 1.0f, 0.0f);
    }
    else
    {
        base = normalize(base);
    }

    float3 worldUp = float3(0.0f, 1.0f, 0.0f);
    float3 right = cross(worldUp, base);
    if (dot(right, right) < 0.0001f)
    {
        right = float3(1.0f, 0.0f, 0.0f);
    }
    right = normalize(right);
    float3 up = normalize(cross(base, right));

    float spread = tan(max(spreadRadians, 0.0f));
    float side = RandomRange(state, -spread, spread);
    float lift = RandomRange(state, -spread, spread);
    float forwardBias = RandomRange(state, 0.65f, 1.0f);

    return normalize(base * forwardBias + right * side + up * lift);
}

float3 SafeNormalize(float3 value, float3 fallback)
{
    float lenSq = dot(value, value);
    return lenSq > 0.0001f ? value * rsqrt(lenSq) : fallback;
}

float TerrainHeight(float x, float z)
{
    return sin(x * TerrainParams.y) * cos(z * TerrainParams.y) * TerrainParams.x;
}

float3 TerrainNormal(float x, float z)
{
    float eps = max(TerrainParams.z, 0.001f);
    float hL = TerrainHeight(x - eps, z);
    float hR = TerrainHeight(x + eps, z);
    float hD = TerrainHeight(x, z - eps);
    float hU = TerrainHeight(x, z + eps);
    return SafeNormalize(float3(hL - hR, 2.0f * eps, hD - hU), float3(0.0f, 1.0f, 0.0f));
}

void ResolveGroundCollision(inout float3 position, inout float3 velocity)
{
    if (GroundCollisionParams.x < 0.5f)
    {
        return;
    }

    float terrainY = TerrainHeight(position.x, position.z);
    float3 terrainNormal = TerrainNormal(position.x, position.z);
    float3 terrainPoint = float3(position.x, terrainY, position.z);
    float surfaceOffset = max(GroundCollisionParams.y, 0.0f);
    float signedDistance = dot(position - terrainPoint, terrainNormal);

    if (signedDistance >= surfaceOffset)
    {
        return;
    }

    position += terrainNormal * (surfaceOffset - signedDistance);

    float normalVelocity = dot(velocity, terrainNormal);
    if (normalVelocity < 0.0f)
    {
        float restitution = saturate(GroundCollisionParams.z);
        float friction = saturate(GroundCollisionParams.w);
        float3 normalVelocityPart = terrainNormal * normalVelocity;
        float3 tangentVelocityPart = velocity - normalVelocityPart;
        velocity = tangentVelocityPart * friction - normalVelocityPart * restitution;
    }
}

void ApplySdfSphereForce(uint sphereIndex, float3 position, inout float3 velocity, float dt)
{
    float4 centerRadius = SdfCenterRadius[sphereIndex];
    float4 velocityInfluence = SdfVelocityInfluence[sphereIndex];
    float4 sdfParams = SdfParams[sphereIndex];

    if (sdfParams.w < 0.5f || centerRadius.w <= 0.0f)
    {
        return;
    }

    float3 toParticle = position - centerRadius.xyz;
    float distanceToCenter = length(toParticle);
    float3 sdfNormal = distanceToCenter > 0.0001f
        ? toParticle / distanceToCenter
        : float3(0.0f, 1.0f, 0.0f);
    float sdf = distanceToCenter - centerRadius.w;
    float influenceDistance = max(velocityInfluence.w, 0.001f);

    if (sdf < influenceDistance)
    {
        float outsideDistance = max(sdf, 0.0f);
        float falloff = saturate(1.0f - outsideDistance / influenceDistance);
        float force = falloff * falloff * sdfParams.x;
        velocity += sdfNormal * force * dt;
        velocity += velocityInfluence.xyz * (sdfParams.z * falloff);
    }
}

void ResolveSdfSpherePenetration(uint sphereIndex, inout float3 position, inout float3 velocity, float dt)
{
    float4 centerRadius = SdfCenterRadius[sphereIndex];
    float4 sdfParams = SdfParams[sphereIndex];

    if (sdfParams.w < 0.5f || centerRadius.w <= 0.0f)
    {
        return;
    }

    float3 toParticle = position - centerRadius.xyz;
    float distanceToCenter = length(toParticle);
    float3 sdfNormal = distanceToCenter > 0.0001f
        ? toParticle / distanceToCenter
        : float3(0.0f, 1.0f, 0.0f);
    float minDistance = centerRadius.w + max(sdfParams.y, 0.0f);

    if (distanceToCenter < minDistance)
    {
        position = centerRadius.xyz + sdfNormal * minDistance;

        float inwardVelocity = dot(velocity, sdfNormal);
        if (inwardVelocity < 0.0f)
        {
            velocity -= sdfNormal * inwardVelocity;
        }

        velocity += sdfNormal * sdfParams.x * dt;
    }
}

[numthreads(256, 1, 1)]
void CSUpdate(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint index = dispatchThreadId.x;
    uint maxParticles = (uint)SpawnInfo.z;
    if (index >= maxParticles)
    {
        return;
    }

    GpuParticle particle = Particles[index];
    if (particle.ParticleParams.w < 0.5f)
    {
        return;
    }

    float dt = min(max(WindDt.w, 0.0f), 0.05f);
    particle.PositionAge.w += dt;
    if (particle.PositionAge.w >= particle.VelocityLifetime.w)
    {
        particle.ParticleParams.w = 0.0f;
        Particles[index] = particle;
        return;
    }

    float3 velocity = particle.VelocityLifetime.xyz;
    float weight = particle.ParticleParams.x;
    float dragScale = max(0.0f, 1.0f - GravityDrag.w * dt);

    velocity += (GravityDrag.xyz * weight + WindDt.xyz) * dt;

    uint sdfCount = min((uint)SdfControl.x, (uint)MAX_PARTICLE_SDF_SPHERES);
    for (uint sdfForceIndex = 0; sdfForceIndex < sdfCount; ++sdfForceIndex)
    {
        ApplySdfSphereForce(sdfForceIndex, particle.PositionAge.xyz, velocity, dt);
    }

    velocity *= dragScale;
    particle.PositionAge.xyz += velocity * dt;

    for (uint sdfResolveIndex = 0; sdfResolveIndex < sdfCount; ++sdfResolveIndex)
    {
        ResolveSdfSpherePenetration(sdfResolveIndex, particle.PositionAge.xyz, velocity, dt);
    }

    ResolveGroundCollision(particle.PositionAge.xyz, velocity);

    particle.VelocityLifetime.xyz = velocity;
    particle.SizeRotation.z += particle.SizeRotation.w * dt;

    Particles[index] = particle;
}

[numthreads(256, 1, 1)]
void CSEmit(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint localIndex = dispatchThreadId.x;
    uint emitCount = (uint)OriginCount.w;
    uint maxParticles = (uint)SpawnInfo.z;
    if (localIndex >= emitCount || maxParticles == 0)
    {
        return;
    }

    uint spawnStart = (uint)SpawnInfo.x;
    uint particleIndex = (spawnStart + localIndex) % maxParticles;
    uint randomState = Hash((uint)SpawnInfo.y + localIndex * 747796405u + particleIndex * 2891336453u);

    float speed = RandomRange(randomState, SpeedLifetime.x, SpeedLifetime.y);
    float3 direction = RandomDirectionInCone(DirectionSpread.xyz, DirectionSpread.w, randomState);
    float3 widthAxis = SafeNormalize(EmitWidthAxis.xyz, float3(1.0f, 0.0f, 0.0f));
    float3 depthAxis = SafeNormalize(EmitDepthAxis.xyz, float3(0.0f, 0.0f, 1.0f));
    float widthOffset = RandomRange(randomState, -0.5f * EmitWidthAxis.w, 0.5f * EmitWidthAxis.w);
    float depthOffset = RandomRange(randomState, -0.5f * EmitDepthAxis.w, 0.5f * EmitDepthAxis.w);
    float3 spawnPosition = OriginCount.xyz + widthAxis * widthOffset + depthAxis * depthOffset;

    GpuParticle particle;
    particle.PositionAge = float4(spawnPosition, 0.0f);
    particle.VelocityLifetime = float4(direction * speed, max(0.01f, RandomRange(randomState, SpeedLifetime.z, SpeedLifetime.w)));
    particle.StartColor = StartColor;
    particle.EndColor = EndColor;
    particle.SizeRotation = float4(
        RandomRange(randomState, SizeRanges.x, SizeRanges.y),
        RandomRange(randomState, SizeRanges.z, SizeRanges.w),
        RandomRange(randomState, 0.0f, 6.28f),
        RandomRange(randomState, WeightRotation.z, WeightRotation.w));
    particle.ParticleParams = float4(
        RandomRange(randomState, WeightRotation.x, WeightRotation.y),
        EmitParams.x,
        RandomRange(randomState, EmitParams.y, EmitParams.z),
        1.0f);

    Particles[particleIndex] = particle;
}
