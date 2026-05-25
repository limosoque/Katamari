#pragma once

#include <DirectXMath.h>
#include <cstddef>
#include <random>
#include <vector>

struct ParticleEmitterSettings
{
    size_t maxParticles = 1536;
    float emissionRate = 120.0f;
    int burstCount = 28;

    float lifetimeMin = 0.25f;
    float lifetimeMax = 0.85f;
    float speedMin = 0.4f;
    float speedMax = 2.4f;
    float spreadRadians = 0.95f;

    float startSizeMin = 0.045f;
    float startSizeMax = 0.095f;
    float endSizeMin = 0.16f;
    float endSizeMax = 0.36f;

    float weightMin = 0.65f;
    float weightMax = 1.2f;
    float drag = 1.45f;
    DirectX::XMFLOAT3 gravity = { 0.0f, -1.4f, 0.0f };
    DirectX::XMFLOAT3 wind = { 0.35f, 0.05f, 0.0f };

    float rotationSpeedMin = -5.0f;
    float rotationSpeedMax = 5.0f;
    float edgeSoftness = 1.85f;
    float brightnessMin = 0.95f;
    float brightnessMax = 1.55f;

    DirectX::XMFLOAT4 startColor = { 1.0f, 0.72f, 0.28f, 0.78f };
    DirectX::XMFLOAT4 endColor = { 0.24f, 0.10f, 0.035f, 0.0f };
};

struct ParticleVertex
{
    DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };
    float Size = 0.1f;
    DirectX::XMFLOAT4 Color = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT4 Params = { 0.0f, 1.0f, 0.0f, 1.0f }; // x = rotation, y = edge softness, z = normalized age, w = brightness.
};

class ParticleSystem
{
public:
    void Initialize(const ParticleEmitterSettings& emitterSettings);
    void SetSettings(const ParticleEmitterSettings& emitterSettings);
    const ParticleEmitterSettings& Settings() const { return settings; }

    void Update(float dt);
    void EmitBurst(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, int count);
    void EmitContinuous(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, float dt);
    void BuildVertices(const DirectX::XMFLOAT3& cameraPosition, std::vector<ParticleVertex>& outVertices) const;
    void Clear();

    size_t AliveCount() const { return particles.size(); }

private:
    struct Particle
    {
        DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 prevPosition = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 velocity = { 0.0f, 0.0f, 0.0f };
        float age = 0.0f;
        float lifetime = 1.0f;
        float startSize = 0.1f;
        float endSize = 0.2f;
        float weight = 1.0f;
        float rotation = 0.0f;
        float angularVelocity = 0.0f;
        float edgeSoftness = 1.0f;
        float brightness = 1.0f;
        DirectX::XMFLOAT4 startColor = { 1.0f, 1.0f, 1.0f, 1.0f };
        DirectX::XMFLOAT4 endColor = { 1.0f, 1.0f, 1.0f, 0.0f };
    };

    ParticleEmitterSettings settings;
    std::vector<Particle> particles;
    float emissionAccumulator = 0.0f;
    std::mt19937 rng{ std::random_device{}() };

    void EmitOne(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction);
    DirectX::XMVECTOR RandomDirectionInCone(DirectX::XMVECTOR direction);
    float RandomFloat(float lo, float hi);
    static DirectX::XMFLOAT4 LerpColor(const DirectX::XMFLOAT4& a, const DirectX::XMFLOAT4& b, float t);
};
