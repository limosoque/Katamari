#pragma once

#include <DirectXMath.h>
#include <cstddef>
#include <cstdint>
#include <d3d11.h>
#include <string>
#include <vector>
#include <wrl/client.h>

class Game;

const int kMaxParticleSdfSpheres = 8;

struct ParticleGroundCollisionSettings
{
    bool enabled = true;
    float terrainAmplitude = 2.0f;
    float terrainFrequency = 0.2f;
    float terrainNormalSampleOffset = 0.05f;
    float surfaceOffset = 0.04f;
    float restitution = 0.30f;
    float friction = 0.65f;
};

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
    ParticleGroundCollisionSettings groundCollision;
};

struct GpuParticleData
{
    DirectX::XMFLOAT4 PositionAge = { 0.0f, 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4 VelocityLifetime = { 0.0f, 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT4 StartColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT4 EndColor = { 1.0f, 1.0f, 1.0f, 0.0f };
    DirectX::XMFLOAT4 SizeRotation = { 0.1f, 0.2f, 0.0f, 0.0f };
    DirectX::XMFLOAT4 Params = { 1.0f, 1.0f, 1.0f, 0.0f }; // x = weight, y = edge softness, z = brightness, w = alive flag.
};

struct ParticleSdfSphere
{
    DirectX::XMFLOAT3 center = { 0.0f, 0.0f, 0.0f };
    float radius = 0.0f;
    DirectX::XMFLOAT3 velocity = { 0.0f, 0.0f, 0.0f };
    float influenceDistance = 0.0f;
    float repelStrength = 0.0f;
    float surfaceOffset = 0.03f;
    float velocityTransfer = 0.0f;
    float enabled = 0.0f;
};

class ParticleSystem
{
public:
    explicit ParticleSystem(std::wstring computeShaderPath = L"shaders/ParticlesCompute.hlsl");
    ParticleSystem(const ParticleSystem&) = delete;
    ParticleSystem& operator=(const ParticleSystem&) = delete;
    ParticleSystem(ParticleSystem&&) noexcept = default;
    ParticleSystem& operator=(ParticleSystem&&) noexcept = default;

    void Initialize(Game* owner, const ParticleEmitterSettings& emitterSettings);
    void SetSettings(const ParticleEmitterSettings& emitterSettings);
    const ParticleEmitterSettings& Settings() const { return settings; }

    void Update(float dt);
    void Update(float dt, const ParticleSdfSphere& sdfSphere);
    void Update(float dt, const std::vector<ParticleSdfSphere>& sdfSpheres);
    void EmitBurst(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, int count);
    void EmitContinuous(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, float dt);
    void EmitBoxBurst(
        const DirectX::XMFLOAT3& center,
        const DirectX::XMFLOAT3& widthAxis,
        const DirectX::XMFLOAT3& depthAxis,
        float width,
        float depth,
        const DirectX::XMFLOAT3& direction,
        int count);
    void Clear();
    void DestroyResources();

    size_t MaxParticles() const { return maxParticles; }
    size_t AliveCount() const { return maxParticles; } // GPU-side upper bound; rendering uses the alive flag.
    ID3D11ShaderResourceView* GetParticleSrv() const { return particleSrv.Get(); }

private:
    struct PendingEmit
    {
        DirectX::XMFLOAT3 origin = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 direction = { 0.0f, 1.0f, 0.0f };
        DirectX::XMFLOAT3 widthAxis = { 1.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 depthAxis = { 0.0f, 0.0f, 1.0f };
        float width = 0.0f;
        float depth = 0.0f;
        uint32_t count = 0;
        uint32_t seed = 1;
    };

    struct alignas(16) ParticleComputeCB
    {
        DirectX::XMFLOAT4 GravityDrag;
        DirectX::XMFLOAT4 WindDt;
        DirectX::XMFLOAT4 OriginCount;
        DirectX::XMFLOAT4 DirectionSpread;
        DirectX::XMFLOAT4 SpeedLifetime;
        DirectX::XMFLOAT4 SizeRanges;
        DirectX::XMFLOAT4 WeightRotation;
        DirectX::XMFLOAT4 StartColor;
        DirectX::XMFLOAT4 EndColor;
        DirectX::XMFLOAT4 EmitParams;
        DirectX::XMFLOAT4 SpawnInfo;
        DirectX::XMFLOAT4 EmitWidthAxis;
        DirectX::XMFLOAT4 EmitDepthAxis;
        DirectX::XMFLOAT4 SdfCenterRadius[kMaxParticleSdfSpheres];
        DirectX::XMFLOAT4 SdfVelocityInfluence[kMaxParticleSdfSpheres];
        DirectX::XMFLOAT4 SdfParams[kMaxParticleSdfSpheres];
        DirectX::XMFLOAT4 SdfControl;
        DirectX::XMFLOAT4 TerrainParams;
        DirectX::XMFLOAT4 GroundCollisionParams;
    };

    Game* game = nullptr;
    std::wstring shaderPath;
    ParticleEmitterSettings settings;
    size_t maxParticles = 0;
    float emissionAccumulator = 0.0f;
    uint32_t spawnCursor = 0;
    uint32_t nextSeed = 1;
    std::vector<PendingEmit> pendingEmits;

    Microsoft::WRL::ComPtr<ID3D11ComputeShader> updateShader;
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> emitShader;
    Microsoft::WRL::ComPtr<ID3D11Buffer> particleBuffer;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> particleSrv;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> particleUav;
    Microsoft::WRL::ComPtr<ID3D11Buffer> computeConstantBuffer;

    void CompileShaders();
    void CreateBuffers();
    void RecreateParticleResources();
    void UpdateWithSdfSpheres(float dt, const ParticleSdfSphere* sdfSpheres, size_t sdfSphereCount);
    void DispatchUpdate(float dt, const ParticleSdfSphere* sdfSpheres, size_t sdfSphereCount);
    void DispatchEmit(const PendingEmit& emit);
    void UpdateComputeConstants(const ParticleComputeCB& constants);
    void UnbindComputeResources();
};
