#pragma once

#include "GBuffer.h"
#include "ShadowMap.h"

#include <DirectXMath.h>
#include <array>
#include <d3d11.h>
#include <string>
#include <vector>
#include <wrl/client.h>

class Game;

static constexpr int kMaxDeferredPointLights = 16;
static constexpr int kMaxDeferredSpotLights = 8;
static constexpr int kMaxDeferredTrailStamps = 64;

struct DeferredPointLight
{
    DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };
    float Range = 1.0f;
    DirectX::XMFLOAT3 Color = { 1.0f, 1.0f, 1.0f };
    float Intensity = 1.0f;
};

struct DeferredSpotLight
{
    DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };
    float Range = 1.0f;
    DirectX::XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f };
    float OuterConeCos = 0.75f;
    DirectX::XMFLOAT3 Color = { 1.0f, 1.0f, 1.0f };
    float Intensity = 1.0f;
    float InnerConeCos = 0.9f;
    float Padding[3] = {};
};

struct DeferredLightingData
{
    DirectX::XMFLOAT4 DirectionalDirection = { 0.577f, 0.577f, 0.577f, 0.0f };
    DirectX::XMFLOAT4 DirectionalColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT4 CameraPosition = { 0.0f, 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT4 CascadeSplits = { 12.0f, 17.0f, 90.0f, 0.0f };
    DirectX::XMFLOAT4 SkyColor = { 0.53f, 0.81f, 0.98f, 1.0f };
    DirectX::XMFLOAT4 TrailColor = { 1.0f, 0.75f, 0.35f, 1.0f };
    std::array<DirectX::XMFLOAT4X4, kCascadeCount> LightViewProj = {};
    std::vector<DeferredPointLight> PointLights;
    std::vector<DeferredSpotLight> SpotLights;
    std::vector<DirectX::XMFLOAT4> TrailStamps;
};

class RenderingSystem
{
public:
    explicit RenderingSystem(
        std::wstring geometryShaderPath = L"shaders/DeferredGeometry.hlsl",
        std::wstring lightingShaderPath = L"shaders/DeferredLighting.hlsl");

    void Initialize(Game* owner, int width, int height);
    void BeginGeometryPass(
        ID3D11DepthStencilView* depthView,
        ID3D11Buffer* perObjectConstantBuffer,
        ID3D11SamplerState* textureSampler,
        int width,
        int height);
    void EndGeometryPass();
    void RenderLighting(
        const DeferredLightingData& lightingData,
        ID3D11ShaderResourceView* shadowMapSrv,
        ID3D11SamplerState* shadowSampler,
        ID3D11RenderTargetView* outputTarget,
        int width,
        int height);
    void DestroyResources();

    GBuffer& GetGBuffer() { return gBuffer; }
    const GBuffer& GetGBuffer() const { return gBuffer; }

private:
    struct alignas(16) LightingCB
    {
        DirectX::XMFLOAT4 DirectionalDirection;
        DirectX::XMFLOAT4 DirectionalColor;
        DirectX::XMFLOAT4 CameraPosition;
        DirectX::XMFLOAT4 CascadeSplits;
        DirectX::XMFLOAT4 SkyColor;
        DirectX::XMFLOAT4 TrailColor;
        DirectX::XMFLOAT4 LightCounts;
        DirectX::XMFLOAT4 Padding;
        DirectX::XMFLOAT4X4 LightViewProj[kCascadeCount];
        DirectX::XMFLOAT4 PointPositionRange[kMaxDeferredPointLights];
        DirectX::XMFLOAT4 PointColorIntensity[kMaxDeferredPointLights];
        DirectX::XMFLOAT4 SpotPositionRange[kMaxDeferredSpotLights];
        DirectX::XMFLOAT4 SpotDirectionOuterCos[kMaxDeferredSpotLights];
        DirectX::XMFLOAT4 SpotColorIntensity[kMaxDeferredSpotLights];
        DirectX::XMFLOAT4 SpotConeCos[kMaxDeferredSpotLights];
        DirectX::XMFLOAT4 TrailStamps[kMaxDeferredTrailStamps];
    };

    Game* game = nullptr;
    std::wstring geometryShaderPath;
    std::wstring lightingShaderPath;
    GBuffer gBuffer;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> geometryVertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> geometryPixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> geometryInputLayout;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> lightingVertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> lightingPixelShader;
    Microsoft::WRL::ComPtr<ID3D11Buffer> lightingConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> geometryRasterizerState;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> fullscreenRasterizerState;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> geometryDepthState;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> lightingDepthState;
    Microsoft::WRL::ComPtr<ID3D11BlendState> opaqueBlendState;

    void CompileShaders();
    void CreateInputLayout(ID3DBlob* vertexShaderBytecode);
    void CreateConstantBuffers();
    void CreateStates();
    void UpdateLightingConstants(const DeferredLightingData& lightingData);
};
