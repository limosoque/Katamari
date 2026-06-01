#define NOMINMAX

#include "RenderingSystem.h"
#include "Game.h"

#include <algorithm>
#include <d3dcompiler.h>
#include <iostream>
#include <stdexcept>
#include <utility>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

RenderingSystem::RenderingSystem(std::wstring geometryPath, std::wstring lightingPath)
    : geometryShaderPath(std::move(geometryPath))
    , lightingShaderPath(std::move(lightingPath))
{
}

void RenderingSystem::Initialize(Game* owner, int width, int height)
{
    game = owner;
    gBuffer.Initialize(owner, width, height);
    CompileShaders();
    CreateConstantBuffers();
    CreateStates();
}

void RenderingSystem::CompileShaders()
{
    ComPtr<ID3DBlob> errors;
    UINT flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;

    ComPtr<ID3DBlob> geometryVsBytecode;
    HRESULT hr = D3DCompileFromFile(
        geometryShaderPath.c_str(), nullptr, nullptr,
        "VSMain", "vs_5_0", flags, 0,
        geometryVsBytecode.GetAddressOf(), errors.GetAddressOf());
    if (FAILED(hr))
    {
        if (errors) std::cerr << "[DeferredGeometryVS] " << static_cast<char*>(errors->GetBufferPointer()) << '\n';
        throw std::runtime_error("RenderingSystem: deferred geometry vertex shader compilation failed.");
    }

    hr = game->Device->CreateVertexShader(
        geometryVsBytecode->GetBufferPointer(),
        geometryVsBytecode->GetBufferSize(),
        nullptr,
        geometryVertexShader.GetAddressOf());
    if (FAILED(hr))
    {
        throw std::runtime_error("RenderingSystem: CreateVertexShader for geometry pass failed.");
    }

    CreateInputLayout(geometryVsBytecode.Get());

    ComPtr<ID3DBlob> geometryPsBytecode;
    errors.Reset();
    hr = D3DCompileFromFile(
        geometryShaderPath.c_str(), nullptr, nullptr,
        "PSMain", "ps_5_0", flags, 0,
        geometryPsBytecode.GetAddressOf(), errors.GetAddressOf());
    if (FAILED(hr))
    {
        if (errors) std::cerr << "[DeferredGeometryPS] " << static_cast<char*>(errors->GetBufferPointer()) << '\n';
        throw std::runtime_error("RenderingSystem: deferred geometry pixel shader compilation failed.");
    }

    hr = game->Device->CreatePixelShader(
        geometryPsBytecode->GetBufferPointer(),
        geometryPsBytecode->GetBufferSize(),
        nullptr,
        geometryPixelShader.GetAddressOf());
    if (FAILED(hr))
    {
        throw std::runtime_error("RenderingSystem: CreatePixelShader for geometry pass failed.");
    }

    ComPtr<ID3DBlob> lightingVsBytecode;
    errors.Reset();
    hr = D3DCompileFromFile(
        lightingShaderPath.c_str(), nullptr, nullptr,
        "VSMain", "vs_5_0", flags, 0,
        lightingVsBytecode.GetAddressOf(), errors.GetAddressOf());
    if (FAILED(hr))
    {
        if (errors) std::cerr << "[DeferredLightingVS] " << static_cast<char*>(errors->GetBufferPointer()) << '\n';
        throw std::runtime_error("RenderingSystem: deferred lighting vertex shader compilation failed.");
    }

    hr = game->Device->CreateVertexShader(
        lightingVsBytecode->GetBufferPointer(),
        lightingVsBytecode->GetBufferSize(),
        nullptr,
        lightingVertexShader.GetAddressOf());
    if (FAILED(hr))
    {
        throw std::runtime_error("RenderingSystem: CreateVertexShader for lighting pass failed.");
    }

    ComPtr<ID3DBlob> lightingPsBytecode;
    errors.Reset();
    hr = D3DCompileFromFile(
        lightingShaderPath.c_str(), nullptr, nullptr,
        "PSMain", "ps_5_0", flags, 0,
        lightingPsBytecode.GetAddressOf(), errors.GetAddressOf());
    if (FAILED(hr))
    {
        if (errors) std::cerr << "[DeferredLightingPS] " << static_cast<char*>(errors->GetBufferPointer()) << '\n';
        throw std::runtime_error("RenderingSystem: deferred lighting pixel shader compilation failed.");
    }

    hr = game->Device->CreatePixelShader(
        lightingPsBytecode->GetBufferPointer(),
        lightingPsBytecode->GetBufferSize(),
        nullptr,
        lightingPixelShader.GetAddressOf());
    if (FAILED(hr))
    {
        throw std::runtime_error("RenderingSystem: CreatePixelShader for lighting pass failed.");
    }
}

void RenderingSystem::CreateInputLayout(ID3DBlob* vertexShaderBytecode)
{
    D3D11_INPUT_ELEMENT_DESC elements[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    HRESULT hr = game->Device->CreateInputLayout(
        elements,
        static_cast<unsigned int>(sizeof(elements) / sizeof(elements[0])),
        vertexShaderBytecode->GetBufferPointer(),
        vertexShaderBytecode->GetBufferSize(),
        geometryInputLayout.GetAddressOf());
    if (FAILED(hr))
    {
        throw std::runtime_error("RenderingSystem: CreateInputLayout failed.");
    }
}

void RenderingSystem::CreateConstantBuffers()
{
    D3D11_BUFFER_DESC desc = {};
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    desc.ByteWidth = sizeof(LightingCB);

    HRESULT hr = game->Device->CreateBuffer(&desc, nullptr, lightingConstantBuffer.GetAddressOf());
    if (FAILED(hr))
    {
        throw std::runtime_error("RenderingSystem: CreateBuffer for lighting constants failed.");
    }
}

void RenderingSystem::CreateStates()
{
    CD3D11_RASTERIZER_DESC geometryRasterDesc(D3D11_DEFAULT);
    geometryRasterDesc.CullMode = D3D11_CULL_BACK;
    geometryRasterDesc.FillMode = D3D11_FILL_SOLID;
    geometryRasterDesc.DepthClipEnable = TRUE;
    HRESULT hr = game->Device->CreateRasterizerState(&geometryRasterDesc, geometryRasterizerState.GetAddressOf());
    if (FAILED(hr))
    {
        throw std::runtime_error("RenderingSystem: CreateRasterizerState for geometry pass failed.");
    }

    CD3D11_RASTERIZER_DESC fullscreenRasterDesc(D3D11_DEFAULT);
    fullscreenRasterDesc.CullMode = D3D11_CULL_NONE;
    fullscreenRasterDesc.FillMode = D3D11_FILL_SOLID;
    fullscreenRasterDesc.DepthClipEnable = TRUE;
    hr = game->Device->CreateRasterizerState(&fullscreenRasterDesc, fullscreenRasterizerState.GetAddressOf());
    if (FAILED(hr))
    {
        throw std::runtime_error("RenderingSystem: CreateRasterizerState for fullscreen pass failed.");
    }

    D3D11_DEPTH_STENCIL_DESC geometryDepthDesc = {};
    geometryDepthDesc.DepthEnable = TRUE;
    geometryDepthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    geometryDepthDesc.DepthFunc = D3D11_COMPARISON_LESS;
    hr = game->Device->CreateDepthStencilState(&geometryDepthDesc, geometryDepthState.GetAddressOf());
    if (FAILED(hr))
    {
        throw std::runtime_error("RenderingSystem: CreateDepthStencilState for geometry pass failed.");
    }

    D3D11_DEPTH_STENCIL_DESC lightingDepthDesc = {};
    lightingDepthDesc.DepthEnable = FALSE;
    lightingDepthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    lightingDepthDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    hr = game->Device->CreateDepthStencilState(&lightingDepthDesc, lightingDepthState.GetAddressOf());
    if (FAILED(hr))
    {
        throw std::runtime_error("RenderingSystem: CreateDepthStencilState for lighting pass failed.");
    }

    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable = FALSE;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    hr = game->Device->CreateBlendState(&blendDesc, opaqueBlendState.GetAddressOf());
    if (FAILED(hr))
    {
        throw std::runtime_error("RenderingSystem: CreateBlendState failed.");
    }
}

void RenderingSystem::BeginGeometryPass(
    ID3D11DepthStencilView* depthView,
    ID3D11Buffer* perObjectConstantBuffer,
    ID3D11SamplerState* textureSampler,
    int width,
    int height)
{
    gBuffer.Resize(width, height);
    gBuffer.BindForWriting(depthView);
    gBuffer.Clear(depthView);

    auto* ctx = game->Context.Get();

    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(gBuffer.Width());
    viewport.Height = static_cast<float>(gBuffer.Height());
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    ctx->RSSetViewports(1, &viewport);

    float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    ctx->OMSetBlendState(opaqueBlendState.Get(), blendFactor, 0xFFFFFFFF);
    ctx->OMSetDepthStencilState(geometryDepthState.Get(), 0);
    ctx->RSSetState(geometryRasterizerState.Get());

    ctx->IASetInputLayout(geometryInputLayout.Get());
    ctx->VSSetShader(geometryVertexShader.Get(), nullptr, 0);
    ctx->PSSetShader(geometryPixelShader.Get(), nullptr, 0);
    ctx->GSSetShader(nullptr, nullptr, 0);

    ctx->VSSetConstantBuffers(0, 1, &perObjectConstantBuffer);
    ctx->PSSetConstantBuffers(0, 1, &perObjectConstantBuffer);
    ctx->PSSetSamplers(0, 1, &textureSampler);
}

void RenderingSystem::EndGeometryPass()
{
    std::array<ID3D11RenderTargetView*, kGBufferTargetCount> nullRtvs = {};
    game->Context->OMSetRenderTargets(static_cast<unsigned int>(nullRtvs.size()), nullRtvs.data(), nullptr);
}

void RenderingSystem::UpdateLightingConstants(const DeferredLightingData& lightingData)
{
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(game->Context->Map(lightingConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        return;
    }

    auto* cb = reinterpret_cast<LightingCB*>(mapped.pData);
    *cb = {};

    cb->DirectionalDirection = lightingData.DirectionalDirection;
    cb->DirectionalColor = lightingData.DirectionalColor;
    cb->CameraPosition = lightingData.CameraPosition;
    cb->CascadeSplits = lightingData.CascadeSplits;
    cb->SkyColor = lightingData.SkyColor;
    cb->TrailColor = lightingData.TrailColor;
    cb->DebugFlags = lightingData.DebugFlags;

    for (int i = 0; i < kCascadeCount; ++i)
    {
        cb->LightViewProj[i] = lightingData.LightViewProj[i];
    }

    const size_t pointCount = std::min(
        lightingData.PointLights.size(),
        static_cast<size_t>(kMaxDeferredPointLights));
    const size_t spotCount = std::min(
        lightingData.SpotLights.size(),
        static_cast<size_t>(kMaxDeferredSpotLights));
    const size_t trailCount = std::min(
        lightingData.TrailStamps.size(),
        static_cast<size_t>(kMaxDeferredTrailStamps));

    cb->LightCounts = XMFLOAT4(
        static_cast<float>(pointCount),
        static_cast<float>(spotCount),
        static_cast<float>(trailCount),
        0.0f);

    for (size_t i = 0; i < pointCount; ++i)
    {
        const DeferredPointLight& light = lightingData.PointLights[i];
        cb->PointPositionRange[i] = XMFLOAT4(
            light.Position.x,
            light.Position.y,
            light.Position.z,
            light.Range);
        cb->PointColorIntensity[i] = XMFLOAT4(
            light.Color.x,
            light.Color.y,
            light.Color.z,
            light.Intensity);
    }

    for (size_t i = 0; i < spotCount; ++i)
    {
        const DeferredSpotLight& light = lightingData.SpotLights[i];
        cb->SpotPositionRange[i] = XMFLOAT4(
            light.Position.x,
            light.Position.y,
            light.Position.z,
            light.Range);
        cb->SpotDirectionOuterCos[i] = XMFLOAT4(
            light.Direction.x,
            light.Direction.y,
            light.Direction.z,
            light.OuterConeCos);
        cb->SpotColorIntensity[i] = XMFLOAT4(
            light.Color.x,
            light.Color.y,
            light.Color.z,
            light.Intensity);
        cb->SpotConeCos[i] = XMFLOAT4(
            light.InnerConeCos,
            light.OuterConeCos,
            0.0f,
            0.0f);
    }

    for (size_t i = 0; i < trailCount; ++i)
    {
        cb->TrailStamps[i] = lightingData.TrailStamps[i];
    }

    game->Context->Unmap(lightingConstantBuffer.Get(), 0);
}

void RenderingSystem::RenderLighting(
    const DeferredLightingData& lightingData,
    ID3D11ShaderResourceView* shadowMapSrv,
    ID3D11SamplerState* shadowSampler,
    ID3D11RenderTargetView* outputTarget,
    int width,
    int height)
{
    UpdateLightingConstants(lightingData);

    auto* ctx = game->Context.Get();
    ID3D11RenderTargetView* rtv = outputTarget;
    ctx->OMSetRenderTargets(1, &rtv, nullptr);

    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(std::max(1, width));
    viewport.Height = static_cast<float>(std::max(1, height));
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    ctx->RSSetViewports(1, &viewport);

    float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    ctx->OMSetBlendState(opaqueBlendState.Get(), blendFactor, 0xFFFFFFFF);
    ctx->OMSetDepthStencilState(lightingDepthState.Get(), 0);
    ctx->RSSetState(fullscreenRasterizerState.Get());

    ID3D11Buffer* noVertexBuffers[] = { nullptr };
    unsigned int strides[] = { 0 };
    unsigned int offsets[] = { 0 };
    ctx->IASetInputLayout(nullptr);
    ctx->IASetVertexBuffers(0, 1, noVertexBuffers, strides, offsets);
    ctx->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ctx->VSSetShader(lightingVertexShader.Get(), nullptr, 0);
    ctx->PSSetShader(lightingPixelShader.Get(), nullptr, 0);
    ctx->GSSetShader(nullptr, nullptr, 0);
    ctx->PSSetConstantBuffers(0, 1, lightingConstantBuffer.GetAddressOf());

    gBuffer.BindForReading(0);
    ctx->PSSetShaderResources(6, 1, &shadowMapSrv);
    ctx->PSSetSamplers(1, 1, &shadowSampler);

    ctx->Draw(6, 0);

    gBuffer.UnbindFromShader(0);
    ID3D11ShaderResourceView* nullShadow = nullptr;
    ctx->PSSetShaderResources(6, 1, &nullShadow);
}

void RenderingSystem::DestroyResources()
{
    gBuffer.DestroyResources();
    opaqueBlendState.Reset();
    lightingDepthState.Reset();
    geometryDepthState.Reset();
    fullscreenRasterizerState.Reset();
    geometryRasterizerState.Reset();
    lightingConstantBuffer.Reset();
    lightingPixelShader.Reset();
    lightingVertexShader.Reset();
    geometryInputLayout.Reset();
    geometryPixelShader.Reset();
    geometryVertexShader.Reset();
    game = nullptr;
}
