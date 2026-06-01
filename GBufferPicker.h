#pragma once

#include "GBuffer.h"

#include <DirectXMath.h>
#include <cstdint>
#include <d3d11.h>
#include <string>
#include <wrl/client.h>

class Game;

struct GBufferPickResult
{
    DirectX::XMFLOAT3 WorldPosition = { 0.0f, 0.0f, 0.0f };
    float ViewDepth = 0.0f;
    DirectX::XMFLOAT3 Normal = { 0.0f, 1.0f, 0.0f };
    uint32_t ObjectId = 0;
    uint32_t PixelX = 0;
    uint32_t PixelY = 0;
    bool Valid = false;
};

class GBufferPicker
{
public:
    explicit GBufferPicker(std::wstring shaderPath = L"shaders/GBufferPick.hlsl");

    void Initialize(Game* owner);
    bool Pick(
        const GBuffer& gBuffer,
        uint32_t pixelX,
        uint32_t pixelY,
        uint32_t screenWidth,
        uint32_t screenHeight,
        GBufferPickResult& result);
    void DestroyResources();

private:
    struct alignas(16) PickCB
    {
        uint32_t PixelX = 0;
        uint32_t PixelY = 0;
        uint32_t ScreenWidth = 0;
        uint32_t ScreenHeight = 0;
    };

    struct alignas(16) PickResultGpu
    {
        DirectX::XMFLOAT4 WorldPositionViewDepth;
        DirectX::XMFLOAT4 NormalValid;
        DirectX::XMUINT4 ObjectIdPixelValid;
    };

    Game* game = nullptr;
    std::wstring shaderPath;

    Microsoft::WRL::ComPtr<ID3D11ComputeShader> computeShader;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> outputBuffer;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> outputUav;
    Microsoft::WRL::ComPtr<ID3D11Buffer> stagingBuffer;

    void CompileShader();
    void CreateBuffers();
    void UpdateConstants(uint32_t pixelX, uint32_t pixelY, uint32_t screenWidth, uint32_t screenHeight);
};
