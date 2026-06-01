#define NOMINMAX

#include "ParticleSystem.h"
#include "Game.h"

#include <algorithm>
#include <d3dcompiler.h>
#include <iostream>
#include <stdexcept>
#include <utility>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

ParticleSystem::ParticleSystem(std::wstring computePath)
    : shaderPath(std::move(computePath))
{
}

void ParticleSystem::Initialize(Game* owner, const ParticleEmitterSettings& emitterSettings)
{
    game = owner;
    settings = emitterSettings;
    maxParticles = std::max<size_t>(1, settings.maxParticles);
    emissionAccumulator = 0.0f;
    spawnCursor = 0;
    nextSeed = 1;
    pendingEmits.clear();

    CompileShaders();
    CreateBuffers();
    Clear();
}

void ParticleSystem::SetSettings(const ParticleEmitterSettings& emitterSettings)
{
    size_t oldMaxParticles = maxParticles;
    settings = emitterSettings;
    maxParticles = std::max<size_t>(1, settings.maxParticles);

    if (game && oldMaxParticles != maxParticles)
    {
        RecreateParticleResources();
        Clear();
    }
}

void ParticleSystem::CompileShaders()
{
    ComPtr<ID3DBlob> errors;
    UINT flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;

    ComPtr<ID3DBlob> updateBytecode;
    HRESULT hr = D3DCompileFromFile(
        shaderPath.c_str(),
        nullptr,
        nullptr,
        "CSUpdate",
        "cs_5_0",
        flags,
        0,
        updateBytecode.GetAddressOf(),
        errors.GetAddressOf());
    if (FAILED(hr))
    {
        if (errors) std::cerr << "[ParticlesUpdateCS] " << static_cast<char*>(errors->GetBufferPointer()) << '\n';
        throw std::runtime_error("ParticleSystem: update compute shader compilation failed.");
    }

    hr = game->Device->CreateComputeShader(
        updateBytecode->GetBufferPointer(),
        updateBytecode->GetBufferSize(),
        nullptr,
        updateShader.GetAddressOf());
    if (FAILED(hr))
    {
        throw std::runtime_error("ParticleSystem: CreateComputeShader update failed.");
    }

    errors.Reset();
    ComPtr<ID3DBlob> emitBytecode;
    hr = D3DCompileFromFile(
        shaderPath.c_str(),
        nullptr,
        nullptr,
        "CSEmit",
        "cs_5_0",
        flags,
        0,
        emitBytecode.GetAddressOf(),
        errors.GetAddressOf());
    if (FAILED(hr))
    {
        if (errors) std::cerr << "[ParticlesEmitCS] " << static_cast<char*>(errors->GetBufferPointer()) << '\n';
        throw std::runtime_error("ParticleSystem: emit compute shader compilation failed.");
    }

    hr = game->Device->CreateComputeShader(
        emitBytecode->GetBufferPointer(),
        emitBytecode->GetBufferSize(),
        nullptr,
        emitShader.GetAddressOf());
    if (FAILED(hr))
    {
        throw std::runtime_error("ParticleSystem: CreateComputeShader emit failed.");
    }
}

void ParticleSystem::CreateBuffers()
{
    RecreateParticleResources();

    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    cbDesc.ByteWidth = sizeof(ParticleComputeCB);

    HRESULT hr = game->Device->CreateBuffer(&cbDesc, nullptr, computeConstantBuffer.GetAddressOf());
    if (FAILED(hr))
    {
        throw std::runtime_error("ParticleSystem: CreateBuffer compute constants failed.");
    }
}

void ParticleSystem::RecreateParticleResources()
{
    particleSrv.Reset();
    particleUav.Reset();
    particleBuffer.Reset();

    std::vector<GpuParticleData> initialParticles(maxParticles);

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = initialParticles.data();

    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    bufferDesc.ByteWidth = static_cast<UINT>(sizeof(GpuParticleData) * maxParticles);
    bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bufferDesc.StructureByteStride = sizeof(GpuParticleData);

    HRESULT hr = game->Device->CreateBuffer(&bufferDesc, &initData, particleBuffer.GetAddressOf());
    if (FAILED(hr))
    {
        throw std::runtime_error("ParticleSystem: CreateBuffer particles failed.");
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = static_cast<UINT>(maxParticles);

    hr = game->Device->CreateShaderResourceView(particleBuffer.Get(), &srvDesc, particleSrv.GetAddressOf());
    if (FAILED(hr))
    {
        throw std::runtime_error("ParticleSystem: CreateShaderResourceView particles failed.");
    }

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = static_cast<UINT>(maxParticles);

    hr = game->Device->CreateUnorderedAccessView(particleBuffer.Get(), &uavDesc, particleUav.GetAddressOf());
    if (FAILED(hr))
    {
        throw std::runtime_error("ParticleSystem: CreateUnorderedAccessView particles failed.");
    }
}

void ParticleSystem::Update(float dt)
{
    UpdateWithSdfSpheres(dt, nullptr, 0);
}

void ParticleSystem::Update(float dt, const ParticleSdfSphere& sdfSphere)
{
    UpdateWithSdfSpheres(dt, &sdfSphere, 1);
}

void ParticleSystem::Update(float dt, const std::vector<ParticleSdfSphere>& sdfSpheres)
{
    UpdateWithSdfSpheres(dt, sdfSpheres.data(), sdfSpheres.size());
}

void ParticleSystem::UpdateWithSdfSpheres(float dt, const ParticleSdfSphere* sdfSpheres, size_t sdfSphereCount)
{
    if (!game || !particleUav || maxParticles == 0)
    {
        return;
    }

    dt = std::min(std::max(dt, 0.0f), 0.05f);
    DispatchUpdate(dt, sdfSpheres, sdfSphereCount);

    for (const PendingEmit& emit : pendingEmits)
    {
        DispatchEmit(emit);
    }
    pendingEmits.clear();
}

void ParticleSystem::EmitBurst(const XMFLOAT3& origin, const XMFLOAT3& direction, int count)
{
    if (count <= 0 || maxParticles == 0)
    {
        return;
    }

    PendingEmit emit;
    emit.origin = origin;
    emit.direction = direction;
    emit.count = static_cast<uint32_t>(std::min<size_t>(static_cast<size_t>(count), maxParticles));
    emit.seed = nextSeed++;
    pendingEmits.push_back(emit);
}

void ParticleSystem::EmitBoxBurst(
    const XMFLOAT3& center,
    const XMFLOAT3& widthAxis,
    const XMFLOAT3& depthAxis,
    float width,
    float depth,
    const XMFLOAT3& direction,
    int count)
{
    if (count <= 0 || maxParticles == 0)
    {
        return;
    }

    PendingEmit emit;
    emit.origin = center;
    emit.direction = direction;
    emit.widthAxis = widthAxis;
    emit.depthAxis = depthAxis;
    emit.width = std::max(0.0f, width);
    emit.depth = std::max(0.0f, depth);
    emit.count = static_cast<uint32_t>(std::min<size_t>(static_cast<size_t>(count), maxParticles));
    emit.seed = nextSeed++;
    pendingEmits.push_back(emit);
}

void ParticleSystem::EmitContinuous(const XMFLOAT3& origin, const XMFLOAT3& direction, float dt)
{
    if (settings.emissionRate <= 0.0f || maxParticles == 0)
    {
        return;
    }

    emissionAccumulator += settings.emissionRate * std::max(0.0f, dt);
    int emitCount = static_cast<int>(emissionAccumulator);
    emissionAccumulator -= static_cast<float>(emitCount);

    emitCount = std::min(emitCount, 32);
    EmitBurst(origin, direction, emitCount);
}

void ParticleSystem::DispatchUpdate(float dt, const ParticleSdfSphere* sdfSpheres, size_t sdfSphereCount)
{
    ParticleComputeCB constants = {};
    constants.GravityDrag = XMFLOAT4(settings.gravity.x, settings.gravity.y, settings.gravity.z, settings.drag);
    constants.WindDt = XMFLOAT4(settings.wind.x, settings.wind.y, settings.wind.z, dt);
    constants.SpawnInfo = XMFLOAT4(0.0f, 0.0f, static_cast<float>(maxParticles), 0.0f);

    size_t sdfCount = 0;
    if (sdfSpheres)
    {
        size_t sourceCount = std::min(sdfSphereCount, static_cast<size_t>(kMaxParticleSdfSpheres));
        for (size_t i = 0; i < sourceCount; ++i)
        {
            const ParticleSdfSphere& sdfSphere = sdfSpheres[i];
            if (sdfSphere.enabled <= 0.0f || sdfSphere.radius <= 0.0f)
            {
                continue;
            }

            constants.SdfCenterRadius[sdfCount] = XMFLOAT4(
                sdfSphere.center.x,
                sdfSphere.center.y,
                sdfSphere.center.z,
                sdfSphere.radius);
            constants.SdfVelocityInfluence[sdfCount] = XMFLOAT4(
                sdfSphere.velocity.x,
                sdfSphere.velocity.y,
                sdfSphere.velocity.z,
                sdfSphere.influenceDistance);
            constants.SdfParams[sdfCount] = XMFLOAT4(
                sdfSphere.repelStrength,
                sdfSphere.surfaceOffset,
                sdfSphere.velocityTransfer,
                sdfSphere.enabled);
            ++sdfCount;
        }
    }

    constants.SdfControl = XMFLOAT4(static_cast<float>(sdfCount), 0.0f, 0.0f, 0.0f);
    constants.TerrainParams = XMFLOAT4(
        settings.groundCollision.terrainAmplitude,
        settings.groundCollision.terrainFrequency,
        settings.groundCollision.terrainNormalSampleOffset,
        0.0f);
    constants.GroundCollisionParams = XMFLOAT4(
        settings.groundCollision.enabled ? 1.0f : 0.0f,
        settings.groundCollision.surfaceOffset,
        settings.groundCollision.restitution,
        settings.groundCollision.friction);
    UpdateComputeConstants(constants);

    auto* ctx = game->Context.Get();
    ID3D11UnorderedAccessView* uav = particleUav.Get();
    ctx->CSSetShader(updateShader.Get(), nullptr, 0);
    ctx->CSSetConstantBuffers(0, 1, computeConstantBuffer.GetAddressOf());
    ctx->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);

    const UINT threadsPerGroup = 256;
    UINT groups = static_cast<UINT>((maxParticles + threadsPerGroup - 1) / threadsPerGroup);
    ctx->Dispatch(groups, 1, 1);
    UnbindComputeResources();
}

void ParticleSystem::DispatchEmit(const PendingEmit& emit)
{
    if (emit.count == 0)
    {
        return;
    }

    uint32_t spawnStart = spawnCursor;
    spawnCursor = static_cast<uint32_t>((spawnCursor + emit.count) % maxParticles);

    ParticleComputeCB constants = {};
    constants.OriginCount = XMFLOAT4(
        emit.origin.x,
        emit.origin.y,
        emit.origin.z,
        static_cast<float>(emit.count));
    constants.DirectionSpread = XMFLOAT4(
        emit.direction.x,
        emit.direction.y,
        emit.direction.z,
        settings.spreadRadians);
    constants.SpeedLifetime = XMFLOAT4(
        settings.speedMin,
        settings.speedMax,
        settings.lifetimeMin,
        settings.lifetimeMax);
    constants.SizeRanges = XMFLOAT4(
        settings.startSizeMin,
        settings.startSizeMax,
        settings.endSizeMin,
        settings.endSizeMax);
    constants.WeightRotation = XMFLOAT4(
        settings.weightMin,
        settings.weightMax,
        settings.rotationSpeedMin,
        settings.rotationSpeedMax);
    constants.StartColor = settings.startColor;
    constants.EndColor = settings.endColor;
    constants.EmitParams = XMFLOAT4(
        settings.edgeSoftness,
        settings.brightnessMin,
        settings.brightnessMax,
        0.0f);
    constants.SpawnInfo = XMFLOAT4(
        static_cast<float>(spawnStart),
        static_cast<float>(emit.seed),
        static_cast<float>(maxParticles),
        0.0f);
    constants.EmitWidthAxis = XMFLOAT4(
        emit.widthAxis.x,
        emit.widthAxis.y,
        emit.widthAxis.z,
        emit.width);
    constants.EmitDepthAxis = XMFLOAT4(
        emit.depthAxis.x,
        emit.depthAxis.y,
        emit.depthAxis.z,
        emit.depth);
    UpdateComputeConstants(constants);

    auto* ctx = game->Context.Get();
    ID3D11UnorderedAccessView* uav = particleUav.Get();
    ctx->CSSetShader(emitShader.Get(), nullptr, 0);
    ctx->CSSetConstantBuffers(0, 1, computeConstantBuffer.GetAddressOf());
    ctx->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);

    const UINT threadsPerGroup = 256;
    UINT groups = (emit.count + threadsPerGroup - 1) / threadsPerGroup;
    ctx->Dispatch(groups, 1, 1);
    UnbindComputeResources();
}

void ParticleSystem::UpdateComputeConstants(const ParticleComputeCB& constants)
{
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(game->Context->Map(computeConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        return;
    }

    *reinterpret_cast<ParticleComputeCB*>(mapped.pData) = constants;
    game->Context->Unmap(computeConstantBuffer.Get(), 0);
}

void ParticleSystem::UnbindComputeResources()
{
    ID3D11UnorderedAccessView* nullUav = nullptr;
    game->Context->CSSetUnorderedAccessViews(0, 1, &nullUav, nullptr);
    game->Context->CSSetShader(nullptr, nullptr, 0);
}

void ParticleSystem::Clear()
{
    emissionAccumulator = 0.0f;
    spawnCursor = 0;
    pendingEmits.clear();

    if (!game || !particleBuffer)
    {
        return;
    }

    std::vector<GpuParticleData> cleared(maxParticles);
    game->Context->UpdateSubresource(
        particleBuffer.Get(),
        0,
        nullptr,
        cleared.data(),
        0,
        0);
}

void ParticleSystem::DestroyResources()
{
    pendingEmits.clear();
    computeConstantBuffer.Reset();
    particleUav.Reset();
    particleSrv.Reset();
    particleBuffer.Reset();
    emitShader.Reset();
    updateShader.Reset();
    game = nullptr;
    maxParticles = 0;
}
