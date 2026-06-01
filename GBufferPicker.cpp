#define NOMINMAX

#include "GBufferPicker.h"
#include "Game.h"

#include <d3dcompiler.h>
#include <iostream>
#include <stdexcept>
#include <utility>

using Microsoft::WRL::ComPtr;

GBufferPicker::GBufferPicker(std::wstring path)
    : shaderPath(std::move(path))
{
}

void GBufferPicker::Initialize(Game* owner)
{
    game = owner;
    CompileShader();
    CreateBuffers();
}

void GBufferPicker::CompileShader()
{
    ComPtr<ID3DBlob> bytecode;
    ComPtr<ID3DBlob> errors;
    UINT flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;

    HRESULT hr = D3DCompileFromFile(
        shaderPath.c_str(),
        nullptr,
        nullptr,
        "CSMain",
        "cs_5_0",
        flags,
        0,
        bytecode.GetAddressOf(),
        errors.GetAddressOf());
    if (FAILED(hr))
    {
        if (errors)
        {
            std::cerr << "[GBufferPickCS] " << static_cast<char*>(errors->GetBufferPointer()) << '\n';
        }
        throw std::runtime_error("GBufferPicker: compute shader compilation failed.");
    }

    hr = game->Device->CreateComputeShader(
        bytecode->GetBufferPointer(),
        bytecode->GetBufferSize(),
        nullptr,
        computeShader.GetAddressOf());
    if (FAILED(hr))
    {
        throw std::runtime_error("GBufferPicker: CreateComputeShader failed.");
    }
}

void GBufferPicker::CreateBuffers()
{
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    cbDesc.ByteWidth = sizeof(PickCB);

    HRESULT hr = game->Device->CreateBuffer(&cbDesc, nullptr, constantBuffer.GetAddressOf());
    if (FAILED(hr))
    {
        throw std::runtime_error("GBufferPicker: CreateBuffer constant failed.");
    }

    D3D11_BUFFER_DESC outputDesc = {};
    outputDesc.Usage = D3D11_USAGE_DEFAULT;
    outputDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    outputDesc.ByteWidth = sizeof(PickResultGpu);
    outputDesc.StructureByteStride = sizeof(PickResultGpu);
    outputDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

    hr = game->Device->CreateBuffer(&outputDesc, nullptr, outputBuffer.GetAddressOf());
    if (FAILED(hr))
    {
        throw std::runtime_error("GBufferPicker: CreateBuffer output failed.");
    }

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = 1;

    hr = game->Device->CreateUnorderedAccessView(outputBuffer.Get(), &uavDesc, outputUav.GetAddressOf());
    if (FAILED(hr))
    {
        throw std::runtime_error("GBufferPicker: CreateUnorderedAccessView failed.");
    }

    D3D11_BUFFER_DESC stagingDesc = outputDesc;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    hr = game->Device->CreateBuffer(&stagingDesc, nullptr, stagingBuffer.GetAddressOf());
    if (FAILED(hr))
    {
        throw std::runtime_error("GBufferPicker: CreateBuffer staging failed.");
    }
}

void GBufferPicker::UpdateConstants(uint32_t pixelX, uint32_t pixelY, uint32_t screenWidth, uint32_t screenHeight)
{
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(game->Context->Map(constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        return;
    }

    auto* cb = reinterpret_cast<PickCB*>(mapped.pData);
    cb->PixelX = pixelX;
    cb->PixelY = pixelY;
    cb->ScreenWidth = screenWidth;
    cb->ScreenHeight = screenHeight;

    game->Context->Unmap(constantBuffer.Get(), 0);
}

bool GBufferPicker::Pick(
    const GBuffer& gBuffer,
    uint32_t pixelX,
    uint32_t pixelY,
    uint32_t screenWidth,
    uint32_t screenHeight,
    GBufferPickResult& result)
{
    result = {};
    if (!game || screenWidth == 0 || screenHeight == 0)
    {
        return false;
    }

    UpdateConstants(pixelX, pixelY, screenWidth, screenHeight);

    ID3D11ShaderResourceView* srvs[] =
    {
        gBuffer.GetShaderResourceView(GBuffer::NormalShininess),
        gBuffer.GetShaderResourceView(GBuffer::SpecularViewDepth),
        gBuffer.GetShaderResourceView(GBuffer::WorldPosition),
        gBuffer.GetShaderResourceView(GBuffer::ObjectId)
    };

    auto* ctx = game->Context.Get();
    ID3D11UnorderedAccessView* uav = outputUav.Get();
    UINT initialCounts[] = { 0 };

    ctx->CSSetShader(computeShader.Get(), nullptr, 0);
    ctx->CSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());
    ctx->CSSetShaderResources(0, static_cast<UINT>(sizeof(srvs) / sizeof(srvs[0])), srvs);
    ctx->CSSetUnorderedAccessViews(0, 1, &uav, initialCounts);
    ctx->Dispatch(1, 1, 1);

    ID3D11UnorderedAccessView* nullUav = nullptr;
    ID3D11ShaderResourceView* nullSrvs[] = { nullptr, nullptr, nullptr, nullptr };
    ctx->CSSetUnorderedAccessViews(0, 1, &nullUav, initialCounts);
    ctx->CSSetShaderResources(0, static_cast<UINT>(sizeof(nullSrvs) / sizeof(nullSrvs[0])), nullSrvs);
    ctx->CSSetShader(nullptr, nullptr, 0);

    ctx->CopyResource(stagingBuffer.Get(), outputBuffer.Get());

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(ctx->Map(stagingBuffer.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
    {
        return false;
    }

    const auto* gpuResult = reinterpret_cast<const PickResultGpu*>(mapped.pData);
    result.WorldPosition = DirectX::XMFLOAT3(
        gpuResult->WorldPositionViewDepth.x,
        gpuResult->WorldPositionViewDepth.y,
        gpuResult->WorldPositionViewDepth.z);
    result.ViewDepth = gpuResult->WorldPositionViewDepth.w;
    result.Normal = DirectX::XMFLOAT3(
        gpuResult->NormalValid.x,
        gpuResult->NormalValid.y,
        gpuResult->NormalValid.z);
    result.ObjectId = gpuResult->ObjectIdPixelValid.x;
    result.PixelX = gpuResult->ObjectIdPixelValid.y;
    result.PixelY = gpuResult->ObjectIdPixelValid.z;
    result.Valid = gpuResult->ObjectIdPixelValid.w != 0;

    ctx->Unmap(stagingBuffer.Get(), 0);
    return result.Valid;
}

void GBufferPicker::DestroyResources()
{
    stagingBuffer.Reset();
    outputUav.Reset();
    outputBuffer.Reset();
    constantBuffer.Reset();
    computeShader.Reset();
    game = nullptr;
}
