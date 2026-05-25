#define NOMINMAX

#include "ShadowMapHud.h"
#include "Game.h"

#include <d3dcompiler.h>
#include <algorithm>
#include <array>
#include <stdexcept>
#include <iostream>
#include <utility>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

ShadowMapHud::ShadowMapHud(std::wstring shaderPath)
    : shaderPath(std::move(shaderPath))
{
}

void ShadowMapHud::Initialize(Game* owner)
{
    game = owner;

    CompileShaders();
    CreateConstantBuffer();
    CreateSamplerState();
    CreateBlendState();
    CreateDepthStencilState();
    CreateRasterizerState();
}

void ShadowMapHud::CompileShaders()
{
    ComPtr<ID3DBlob> errors;
    UINT flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;

    ComPtr<ID3DBlob> vsBytecode;
    HRESULT hr = D3DCompileFromFile(
        shaderPath.c_str(), nullptr, nullptr,
        "VSMain", "vs_5_0", flags, 0,
        vsBytecode.GetAddressOf(), errors.GetAddressOf());
    if (FAILED(hr))
    {
        if (errors) std::cerr << "[ShadowMapHudVS] " << static_cast<char*>(errors->GetBufferPointer()) << '\n';
        throw std::runtime_error("ShadowMapHud: vertex shader compilation failed.");
    }

    hr = game->Device->CreateVertexShader(
        vsBytecode->GetBufferPointer(), vsBytecode->GetBufferSize(),
        nullptr, vertexShader.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("ShadowMapHud: CreateVertexShader failed.");

    ComPtr<ID3DBlob> psBytecode;
    errors.Reset();
    hr = D3DCompileFromFile(
        shaderPath.c_str(), nullptr, nullptr,
        "PSMain", "ps_5_0", flags, 0,
        psBytecode.GetAddressOf(), errors.GetAddressOf());
    if (FAILED(hr))
    {
        if (errors) std::cerr << "[ShadowMapHudPS] " << static_cast<char*>(errors->GetBufferPointer()) << '\n';
        throw std::runtime_error("ShadowMapHud: pixel shader compilation failed.");
    }

    hr = game->Device->CreatePixelShader(
        psBytecode->GetBufferPointer(), psBytecode->GetBufferSize(),
        nullptr, pixelShader.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("ShadowMapHud: CreatePixelShader failed.");
}

void ShadowMapHud::CreateConstantBuffer()
{
    D3D11_BUFFER_DESC desc = {};
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    desc.ByteWidth = sizeof(HudCB);

    HRESULT hr = game->Device->CreateBuffer(&desc, nullptr, constantBuffer.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("ShadowMapHud: CreateBuffer failed.");
}

void ShadowMapHud::CreateSamplerState()
{
    D3D11_SAMPLER_DESC desc = {};
    desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    desc.MinLOD = 0.0f;
    desc.MaxLOD = D3D11_FLOAT32_MAX;

    HRESULT hr = game->Device->CreateSamplerState(&desc, samplerState.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("ShadowMapHud: CreateSamplerState failed.");
}

void ShadowMapHud::CreateBlendState()
{
    D3D11_BLEND_DESC desc = {};
    desc.RenderTarget[0].BlendEnable = TRUE;
    desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    HRESULT hr = game->Device->CreateBlendState(&desc, blendState.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("ShadowMapHud: CreateBlendState failed.");
}

void ShadowMapHud::CreateDepthStencilState()
{
    D3D11_DEPTH_STENCIL_DESC desc = {};
    desc.DepthEnable = FALSE;
    desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    desc.StencilEnable = FALSE;

    HRESULT hr = game->Device->CreateDepthStencilState(&desc, depthDisabledState.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("ShadowMapHud: CreateDepthStencilState failed.");
}

void ShadowMapHud::CreateRasterizerState()
{
    CD3D11_RASTERIZER_DESC desc(D3D11_DEFAULT);
    desc.CullMode = D3D11_CULL_NONE;
    desc.FillMode = D3D11_FILL_SOLID;
    desc.DepthClipEnable = TRUE;

    HRESULT hr = game->Device->CreateRasterizerState(&desc, rasterizerState.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("ShadowMapHud: CreateRasterizerState failed.");
}

void ShadowMapHud::UpdateConstantBuffer(int cascadeIndex, int panelCount, int screenWidth, int screenHeight)
{
    const float availableWidth = static_cast<float>(screenWidth) - PanelMargin * 2.0f -
        PanelPadding * static_cast<float>(std::max(0, panelCount - 1));
    const float availableHeight = static_cast<float>(screenHeight) - PanelMargin * 2.0f;
    const float width = std::min({ PanelSize, std::max(32.0f, availableWidth / static_cast<float>(panelCount)), std::max(32.0f, availableHeight) });
    const float height = width;
    const float x = PanelMargin + static_cast<float>(cascadeIndex) * (width + PanelPadding);
    const float y = PanelMargin;

    static const std::array<XMFLOAT4, 3> kBorderColors =
    {
        XMFLOAT4(1.0f, 0.25f, 0.25f, 1.0f),
        XMFLOAT4(0.25f, 1.0f, 0.35f, 1.0f),
        XMFLOAT4(0.35f, 0.45f, 1.0f, 1.0f)
    };

    D3D11_MAPPED_SUBRESOURCE mapped;
    if (FAILED(game->Context->Map(constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        return;
    }

    auto* cb = reinterpret_cast<HudCB*>(mapped.pData);
    cb->PanelRect = XMFLOAT4(x, y, width, height);
    cb->ScreenSize = XMFLOAT4(
        static_cast<float>(screenWidth),
        static_cast<float>(screenHeight),
        screenWidth > 0 ? 1.0f / static_cast<float>(screenWidth) : 0.0f,
        screenHeight > 0 ? 1.0f / static_cast<float>(screenHeight) : 0.0f);
    cb->Params = XMFLOAT4(
        static_cast<float>(cascadeIndex),
        Exposure,
        UseDepthContours ? 1.0f : 0.0f,
        2.0f);
    cb->BorderColor = kBorderColors[static_cast<size_t>(cascadeIndex) % kBorderColors.size()];

    game->Context->Unmap(constantBuffer.Get(), 0);
}

void ShadowMapHud::Draw(ID3D11ShaderResourceView* shadowSrv, int cascadeCount, int screenWidth, int screenHeight)
{
    if (!Enabled || !shadowSrv || cascadeCount <= 0 || screenWidth <= 0 || screenHeight <= 0)
    {
        return;
    }

    auto* ctx = game->Context.Get();

    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(screenWidth);
    viewport.Height = static_cast<float>(screenHeight);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    ctx->RSSetViewports(1, &viewport);

    ID3D11Buffer* noVertexBuffers[] = { nullptr };
    UINT strides[] = { 0 };
    UINT offsets[] = { 0 };
    ctx->IASetInputLayout(nullptr);
    ctx->IASetVertexBuffers(0, 1, noVertexBuffers, strides, offsets);
    ctx->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    float blendFactor[4] = { 0, 0, 0, 0 };
    ctx->OMSetBlendState(blendState.Get(), blendFactor, 0xFFFFFFFF);
    ctx->OMSetDepthStencilState(depthDisabledState.Get(), 0);
    ctx->RSSetState(rasterizerState.Get());
    ctx->VSSetShader(vertexShader.Get(), nullptr, 0);
    ctx->PSSetShader(pixelShader.Get(), nullptr, 0);
    ctx->VSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());
    ctx->PSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());
    ctx->PSSetSamplers(0, 1, samplerState.GetAddressOf());
    ctx->PSSetShaderResources(0, 1, &shadowSrv);

    const int visibleCascades = std::min(cascadeCount, 3);
    for (int cascade = 0; cascade < visibleCascades; ++cascade)
    {
        UpdateConstantBuffer(cascade, visibleCascades, screenWidth, screenHeight);
        ctx->Draw(6, 0);
    }

    ID3D11ShaderResourceView* nullSrv = nullptr;
    ctx->PSSetShaderResources(0, 1, &nullSrv);
}

void ShadowMapHud::DestroyResources()
{
    rasterizerState.Reset();
    depthDisabledState.Reset();
    blendState.Reset();
    samplerState.Reset();
    constantBuffer.Reset();
    pixelShader.Reset();
    vertexShader.Reset();
    game = nullptr;
}
