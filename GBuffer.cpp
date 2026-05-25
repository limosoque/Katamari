#define NOMINMAX

#include "GBuffer.h"
#include "Game.h"

#include <algorithm>
#include <stdexcept>

using Microsoft::WRL::ComPtr;

void GBuffer::Initialize(Game* owner, int initialWidth, int initialHeight)
{
    game = owner;
    Resize(initialWidth, initialHeight);
}

void GBuffer::Resize(int newWidth, int newHeight)
{
    newWidth = std::max(1, newWidth);
    newHeight = std::max(1, newHeight);

    if (width == newWidth && height == newHeight && shaderResourceViews[0])
    {
        return;
    }

    DestroyResources();
    width = newWidth;
    height = newHeight;
    CreateResources();
}

void GBuffer::CreateResources()
{
    if (!game)
    {
        throw std::runtime_error("GBuffer: Initialize must be called before creating resources.");
    }

    for (unsigned int i = 0; i < kGBufferTargetCount; ++i)
    {
        D3D11_TEXTURE2D_DESC textureDesc = {};
        textureDesc.Width = static_cast<unsigned int>(width);
        textureDesc.Height = static_cast<unsigned int>(height);
        textureDesc.MipLevels = 1;
        textureDesc.ArraySize = 1;
        textureDesc.Format = formats[i];
        textureDesc.SampleDesc.Count = 1;
        textureDesc.Usage = D3D11_USAGE_DEFAULT;
        textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        HRESULT hr = game->Device->CreateTexture2D(&textureDesc, nullptr, textures[i].GetAddressOf());
        if (FAILED(hr))
        {
            throw std::runtime_error("GBuffer: CreateTexture2D failed.");
        }

        hr = game->Device->CreateRenderTargetView(textures[i].Get(), nullptr, renderTargetViews[i].GetAddressOf());
        if (FAILED(hr))
        {
            throw std::runtime_error("GBuffer: CreateRenderTargetView failed.");
        }

        hr = game->Device->CreateShaderResourceView(textures[i].Get(), nullptr, shaderResourceViews[i].GetAddressOf());
        if (FAILED(hr))
        {
            throw std::runtime_error("GBuffer: CreateShaderResourceView failed.");
        }
    }
}

void GBuffer::BindForWriting(ID3D11DepthStencilView* depthView)
{
    auto* ctx = game->Context.Get();

    std::array<ID3D11ShaderResourceView*, kGBufferTargetCount> nullSrvs = {};
    ctx->PSSetShaderResources(0, static_cast<unsigned int>(nullSrvs.size()), nullSrvs.data());

    std::array<ID3D11RenderTargetView*, kGBufferTargetCount> rtvs = {};
    for (unsigned int i = 0; i < kGBufferTargetCount; ++i)
    {
        rtvs[i] = renderTargetViews[i].Get();
    }

    ctx->OMSetRenderTargets(static_cast<unsigned int>(rtvs.size()), rtvs.data(), depthView);
}

void GBuffer::Clear(ID3D11DepthStencilView* depthView)
{
    auto* ctx = game->Context.Get();
    const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    for (const auto& rtv : renderTargetViews)
    {
        ctx->ClearRenderTargetView(rtv.Get(), clearColor);
    }

    if (depthView)
    {
        ctx->ClearDepthStencilView(depthView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    }
}

void GBuffer::BindForReading(unsigned int startSlot) const
{
    std::array<ID3D11ShaderResourceView*, kGBufferTargetCount> srvs = {};
    for (unsigned int i = 0; i < kGBufferTargetCount; ++i)
    {
        srvs[i] = shaderResourceViews[i].Get();
    }

    game->Context->PSSetShaderResources(startSlot, static_cast<unsigned int>(srvs.size()), srvs.data());
}

void GBuffer::UnbindFromShader(unsigned int startSlot) const
{
    std::array<ID3D11ShaderResourceView*, kGBufferTargetCount> nullSrvs = {};
    game->Context->PSSetShaderResources(startSlot, static_cast<unsigned int>(nullSrvs.size()), nullSrvs.data());
}

void GBuffer::DestroyResources()
{
    for (auto& srv : shaderResourceViews) srv.Reset();
    for (auto& rtv : renderTargetViews) rtv.Reset();
    for (auto& texture : textures) texture.Reset();
}
