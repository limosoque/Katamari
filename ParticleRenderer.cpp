#define NOMINMAX

#include "ParticleRenderer.h"
#include "Game.h"

#include <algorithm>
#include <d3dcompiler.h>
#include <iostream>
#include <stdexcept>
#include <utility>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

ParticleRenderer::ParticleRenderer(std::wstring path)
    : shaderPath(std::move(path))
{
}

void ParticleRenderer::Initialize(Game* owner, size_t particleCapacity)
{
    game = owner;
    maxParticles = std::max<size_t>(1, particleCapacity);
    CompileShaders();
    CreateBuffers();
    CreateStates();
}

void ParticleRenderer::CompileShaders()
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
        if (errors) std::cerr << "[ParticlesVS] " << static_cast<char*>(errors->GetBufferPointer()) << '\n';
        throw std::runtime_error("ParticleRenderer: vertex shader compilation failed.");
    }

    hr = game->Device->CreateVertexShader(
        vsBytecode->GetBufferPointer(),
        vsBytecode->GetBufferSize(),
        nullptr,
        vertexShader.GetAddressOf());
    if (FAILED(hr))
    {
        throw std::runtime_error("ParticleRenderer: CreateVertexShader failed.");
    }

    CreateInputLayout(vsBytecode.Get());

    ComPtr<ID3DBlob> gsBytecode;
    errors.Reset();
    hr = D3DCompileFromFile(
        shaderPath.c_str(), nullptr, nullptr,
        "GSMain", "gs_5_0", flags, 0,
        gsBytecode.GetAddressOf(), errors.GetAddressOf());
    if (FAILED(hr))
    {
        if (errors) std::cerr << "[ParticlesGS] " << static_cast<char*>(errors->GetBufferPointer()) << '\n';
        throw std::runtime_error("ParticleRenderer: geometry shader compilation failed.");
    }

    hr = game->Device->CreateGeometryShader(
        gsBytecode->GetBufferPointer(),
        gsBytecode->GetBufferSize(),
        nullptr,
        geometryShader.GetAddressOf());
    if (FAILED(hr))
    {
        throw std::runtime_error("ParticleRenderer: CreateGeometryShader failed.");
    }

    ComPtr<ID3DBlob> psBytecode;
    errors.Reset();
    hr = D3DCompileFromFile(
        shaderPath.c_str(), nullptr, nullptr,
        "PSMain", "ps_5_0", flags, 0,
        psBytecode.GetAddressOf(), errors.GetAddressOf());
    if (FAILED(hr))
    {
        if (errors) std::cerr << "[ParticlesPS] " << static_cast<char*>(errors->GetBufferPointer()) << '\n';
        throw std::runtime_error("ParticleRenderer: pixel shader compilation failed.");
    }

    hr = game->Device->CreatePixelShader(
        psBytecode->GetBufferPointer(),
        psBytecode->GetBufferSize(),
        nullptr,
        pixelShader.GetAddressOf());
    if (FAILED(hr))
    {
        throw std::runtime_error("ParticleRenderer: CreatePixelShader failed.");
    }
}

void ParticleRenderer::CreateInputLayout(ID3DBlob* vertexShaderBytecode)
{
    D3D11_INPUT_ELEMENT_DESC elements[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    HRESULT hr = game->Device->CreateInputLayout(
        elements,
        static_cast<unsigned int>(sizeof(elements) / sizeof(elements[0])),
        vertexShaderBytecode->GetBufferPointer(),
        vertexShaderBytecode->GetBufferSize(),
        inputLayout.GetAddressOf());
    if (FAILED(hr))
    {
        throw std::runtime_error("ParticleRenderer: CreateInputLayout failed.");
    }
}

void ParticleRenderer::CreateBuffers()
{
    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage = D3D11_USAGE_DYNAMIC;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    vbDesc.ByteWidth = static_cast<unsigned int>(sizeof(ParticleVertex) * maxParticles);

    HRESULT hr = game->Device->CreateBuffer(&vbDesc, nullptr, vertexBuffer.GetAddressOf());
    if (FAILED(hr))
    {
        throw std::runtime_error("ParticleRenderer: CreateBuffer vertex failed.");
    }

    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    cbDesc.ByteWidth = sizeof(ParticleFrameCB);

    hr = game->Device->CreateBuffer(&cbDesc, nullptr, constantBuffer.GetAddressOf());
    if (FAILED(hr))
    {
        throw std::runtime_error("ParticleRenderer: CreateBuffer constant failed.");
    }
}

void ParticleRenderer::CreateStates()
{
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    HRESULT hr = game->Device->CreateBlendState(&blendDesc, blendState.GetAddressOf());
    if (FAILED(hr))
    {
        throw std::runtime_error("ParticleRenderer: CreateBlendState failed.");
    }

    D3D11_DEPTH_STENCIL_DESC depthDesc = {};
    depthDesc.DepthEnable = TRUE;
    depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    depthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

    hr = game->Device->CreateDepthStencilState(&depthDesc, depthReadState.GetAddressOf());
    if (FAILED(hr))
    {
        throw std::runtime_error("ParticleRenderer: CreateDepthStencilState failed.");
    }

    CD3D11_RASTERIZER_DESC rasterDesc(D3D11_DEFAULT);
    rasterDesc.CullMode = D3D11_CULL_NONE;
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.DepthClipEnable = TRUE;

    hr = game->Device->CreateRasterizerState(&rasterDesc, rasterizerState.GetAddressOf());
    if (FAILED(hr))
    {
        throw std::runtime_error("ParticleRenderer: CreateRasterizerState failed.");
    }
}

void ParticleRenderer::UpdateFrameConstants(
    const XMMATRIX& view,
    const XMMATRIX& projection,
    int screenWidth,
    int screenHeight)
{
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(game->Context->Map(constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        return;
    }

    XMMATRIX invView = XMMatrixInverse(nullptr, view);
    XMVECTOR cameraRight = XMVectorSetW(XMVector3Normalize(invView.r[0]), 0.0f);
    XMVECTOR cameraUp = XMVectorSetW(XMVector3Normalize(invView.r[1]), 0.0f);

    auto* cb = reinterpret_cast<ParticleFrameCB*>(mapped.pData);
    XMStoreFloat4x4(&cb->ViewProjection, XMMatrixTranspose(view * projection));
    XMStoreFloat4x4(&cb->ViewMatrix, XMMatrixTranspose(view));
    XMStoreFloat4(&cb->CameraRight, cameraRight);
    XMStoreFloat4(&cb->CameraUp, cameraUp);
    cb->ScreenSize = XMFLOAT4(
        static_cast<float>(screenWidth),
        static_cast<float>(screenHeight),
        screenWidth > 0 ? 1.0f / static_cast<float>(screenWidth) : 0.0f,
        screenHeight > 0 ? 1.0f / static_cast<float>(screenHeight) : 0.0f);
    cb->Params = XMFLOAT4(softParticleFadeDistance, 0.0f, 0.0f, 0.0f);

    game->Context->Unmap(constantBuffer.Get(), 0);
}

void ParticleRenderer::Draw(
    const std::vector<ParticleVertex>& particles,
    const XMMATRIX& view,
    const XMMATRIX& projection,
    ID3D11RenderTargetView* outputTarget,
    ID3D11DepthStencilView* depthView,
    ID3D11ShaderResourceView* sceneViewDepthSrv,
    int screenWidth,
    int screenHeight)
{
    if (!game || particles.empty() || !outputTarget || !depthView || !sceneViewDepthSrv)
    {
        return;
    }

    size_t drawCount = std::min(particles.size(), maxParticles);
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(game->Context->Map(vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        return;
    }

    std::copy_n(particles.data(), drawCount, reinterpret_cast<ParticleVertex*>(mapped.pData));
    game->Context->Unmap(vertexBuffer.Get(), 0);
    UpdateFrameConstants(view, projection, screenWidth, screenHeight);

    auto* ctx = game->Context.Get();
    ctx->OMSetRenderTargets(1, &outputTarget, depthView);

    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(std::max(1, screenWidth));
    viewport.Height = static_cast<float>(std::max(1, screenHeight));
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    ctx->RSSetViewports(1, &viewport);

    float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    ctx->OMSetBlendState(blendState.Get(), blendFactor, 0xFFFFFFFF);
    ctx->OMSetDepthStencilState(depthReadState.Get(), 0);
    ctx->RSSetState(rasterizerState.Get());

    unsigned int stride = sizeof(ParticleVertex);
    unsigned int offset = 0;
    ID3D11Buffer* vb = vertexBuffer.Get();
    ctx->IASetInputLayout(inputLayout.Get());
    ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    ctx->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

    ctx->VSSetShader(vertexShader.Get(), nullptr, 0);
    ctx->GSSetShader(geometryShader.Get(), nullptr, 0);
    ctx->PSSetShader(pixelShader.Get(), nullptr, 0);
    ctx->VSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());
    ctx->GSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());
    ctx->PSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());
    ctx->PSSetShaderResources(0, 1, &sceneViewDepthSrv);

    ctx->Draw(static_cast<unsigned int>(drawCount), 0);

    ID3D11ShaderResourceView* nullSrv = nullptr;
    ctx->PSSetShaderResources(0, 1, &nullSrv);
    ctx->GSSetShader(nullptr, nullptr, 0);
}

void ParticleRenderer::DestroyResources()
{
    rasterizerState.Reset();
    depthReadState.Reset();
    blendState.Reset();
    constantBuffer.Reset();
    vertexBuffer.Reset();
    inputLayout.Reset();
    pixelShader.Reset();
    geometryShader.Reset();
    vertexShader.Reset();
    game = nullptr;
    maxParticles = 0;
}
