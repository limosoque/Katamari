#define NOMINMAX

#include "ParticleSystem.h"

#include <algorithm>
#include <cmath>

using namespace DirectX;

void ParticleSystem::Initialize(const ParticleEmitterSettings& emitterSettings)
{
    settings = emitterSettings;
    particles.clear();
    particles.reserve(settings.maxParticles);
    emissionAccumulator = 0.0f;
}

void ParticleSystem::SetSettings(const ParticleEmitterSettings& emitterSettings)
{
    settings = emitterSettings;
    if (particles.size() > settings.maxParticles)
    {
        particles.erase(particles.begin(), particles.begin() + (particles.size() - settings.maxParticles));
    }
    particles.reserve(settings.maxParticles);
}

void ParticleSystem::Update(float dt)
{
    dt = std::min(dt, 0.05f);

    XMVECTOR gravity = XMLoadFloat3(&settings.gravity);
    XMVECTOR wind = XMLoadFloat3(&settings.wind);
    float dragScale = std::max(0.0f, 1.0f - settings.drag * dt);

    for (auto& particle : particles)
    {
        particle.age += dt;
        if (particle.age >= particle.lifetime)
        {
            continue;
        }

        XMVECTOR position = XMLoadFloat3(&particle.position);
        XMVECTOR velocity = XMLoadFloat3(&particle.velocity);
        XMStoreFloat3(&particle.prevPosition, position);

        XMVECTOR acceleration = gravity * particle.weight + wind;
        velocity += acceleration * dt;
        velocity *= dragScale;
        position += velocity * dt;

        particle.rotation += particle.angularVelocity * dt;
        XMStoreFloat3(&particle.velocity, velocity);
        XMStoreFloat3(&particle.position, position);
    }

    particles.erase(
        std::remove_if(particles.begin(), particles.end(),
            [](const Particle& particle)
            {
                return particle.age >= particle.lifetime;
            }),
        particles.end());
}

void ParticleSystem::EmitBurst(const XMFLOAT3& origin, const XMFLOAT3& direction, int count)
{
    if (count <= 0 || settings.maxParticles == 0)
    {
        return;
    }

    for (int i = 0; i < count; ++i)
    {
        EmitOne(origin, direction);
    }
}

void ParticleSystem::EmitContinuous(const XMFLOAT3& origin, const XMFLOAT3& direction, float dt)
{
    if (settings.emissionRate <= 0.0f || settings.maxParticles == 0)
    {
        return;
    }

    emissionAccumulator += settings.emissionRate * std::max(0.0f, dt);
    int emitCount = static_cast<int>(std::floor(emissionAccumulator));
    emissionAccumulator -= static_cast<float>(emitCount);

    emitCount = std::min(emitCount, 32);
    EmitBurst(origin, direction, emitCount);
}

void ParticleSystem::EmitOne(const XMFLOAT3& origin, const XMFLOAT3& direction)
{
    if (particles.size() >= settings.maxParticles)
    {
        particles.erase(particles.begin());
    }

    XMVECTOR randomDirection = RandomDirectionInCone(XMLoadFloat3(&direction));
    float speed = RandomFloat(settings.speedMin, settings.speedMax);

    Particle particle;
    particle.position = origin;
    particle.prevPosition = origin;
    XMStoreFloat3(&particle.velocity, randomDirection * speed);
    particle.age = 0.0f;
    particle.lifetime = std::max(0.01f, RandomFloat(settings.lifetimeMin, settings.lifetimeMax));
    particle.startSize = RandomFloat(settings.startSizeMin, settings.startSizeMax);
    particle.endSize = RandomFloat(settings.endSizeMin, settings.endSizeMax);
    particle.weight = RandomFloat(settings.weightMin, settings.weightMax);
    particle.rotation = RandomFloat(0.0f, XM_2PI);
    particle.angularVelocity = RandomFloat(settings.rotationSpeedMin, settings.rotationSpeedMax);
    particle.edgeSoftness = settings.edgeSoftness;
    particle.brightness = RandomFloat(settings.brightnessMin, settings.brightnessMax);
    particle.startColor = settings.startColor;
    particle.endColor = settings.endColor;

    particles.push_back(particle);
}

XMVECTOR ParticleSystem::RandomDirectionInCone(XMVECTOR direction)
{
    XMVECTOR base = XMVector3Normalize(direction);
    if (XMVectorGetX(XMVector3LengthSq(base)) < 0.0001f)
    {
        base = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    }

    XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMVECTOR right = XMVector3Cross(worldUp, base);
    if (XMVectorGetX(XMVector3LengthSq(right)) < 0.0001f)
    {
        right = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
    }
    right = XMVector3Normalize(right);
    XMVECTOR up = XMVector3Normalize(XMVector3Cross(base, right));

    float spread = std::tan(std::max(0.0f, settings.spreadRadians));
    float side = RandomFloat(-spread, spread);
    float lift = RandomFloat(-spread, spread);
    float forwardBias = RandomFloat(0.65f, 1.0f);

    return XMVector3Normalize(base * forwardBias + right * side + up * lift);
}

void ParticleSystem::BuildVertices(const XMFLOAT3& cameraPosition, std::vector<ParticleVertex>& outVertices) const
{
    struct SortItem
    {
        ParticleVertex vertex;
        float distanceSq = 0.0f;
    };

    std::vector<SortItem> sorted;
    sorted.reserve(particles.size());

    XMVECTOR camera = XMLoadFloat3(&cameraPosition);
    for (const Particle& particle : particles)
    {
        float t = std::min(std::max(particle.age / std::max(0.001f, particle.lifetime), 0.0f), 1.0f);
        float size = particle.startSize + (particle.endSize - particle.startSize) * t;
        XMFLOAT4 color = LerpColor(particle.startColor, particle.endColor, t);

        float fadeIn = std::min(particle.age / 0.06f, 1.0f);
        color.w *= fadeIn;

        ParticleVertex vertex;
        vertex.Position = particle.position;
        vertex.Size = size;
        vertex.Color = color;
        vertex.Params = XMFLOAT4(particle.rotation, particle.edgeSoftness, t, particle.brightness);

        XMVECTOR toCamera = XMLoadFloat3(&particle.position) - camera;
        SortItem item;
        item.vertex = vertex;
        item.distanceSq = XMVectorGetX(XMVector3LengthSq(toCamera));
        sorted.push_back(item);
    }

    std::sort(sorted.begin(), sorted.end(),
        [](const SortItem& a, const SortItem& b)
        {
            return a.distanceSq > b.distanceSq;
        });

    outVertices.clear();
    outVertices.reserve(sorted.size());
    for (const SortItem& item : sorted)
    {
        outVertices.push_back(item.vertex);
    }
}

void ParticleSystem::Clear()
{
    particles.clear();
    emissionAccumulator = 0.0f;
}

float ParticleSystem::RandomFloat(float lo, float hi)
{
    if (hi < lo)
    {
        std::swap(lo, hi);
    }

    return lo + std::uniform_real_distribution<float>(0.0f, 1.0f)(rng) * (hi - lo);
}

XMFLOAT4 ParticleSystem::LerpColor(const XMFLOAT4& a, const XMFLOAT4& b, float t)
{
    return XMFLOAT4(
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
        a.w + (b.w - a.w) * t);
}
