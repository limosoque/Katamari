#pragma once

#include "ParticleSystem.h"

#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <d3d11.h>
#include <string>
#include <wrl/client.h>

class Game;

class ParticleRenderer
{
public:
    explicit ParticleRenderer(std::wstring shaderPath = L"shaders/Particles.hlsl");

    void Initialize(Game* owner, size_t maxParticles);
    void Draw(
        const ParticleSystem& particles,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& projection,
        ID3D11RenderTargetView* outputTarget,
        ID3D11DepthStencilView* depthView,
        ID3D11ShaderResourceView* sceneViewDepthSrv,
        int screenWidth,
        int screenHeight);
    void DestroyResources();

private:
    struct alignas(16) ParticleFrameCB
    {
        DirectX::XMFLOAT4X4 ViewProjection;
        DirectX::XMFLOAT4X4 ViewMatrix;
        DirectX::XMFLOAT4 CameraRight;
        DirectX::XMFLOAT4 CameraUp;
        DirectX::XMFLOAT4 ScreenSize;
        DirectX::XMFLOAT4 Params; // x = soft particle fade distance.
    };

    Game* game = nullptr;
    std::wstring shaderPath;
    size_t maxParticles = 0;
    float softParticleFadeDistance = 1.35f;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    Microsoft::WRL::ComPtr<ID3D11GeometryShader> geometryShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
    Microsoft::WRL::ComPtr<ID3D11BlendState> blendState;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthReadState;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerState;

    void CompileShaders();
    void CreateBuffers();
    void CreateStates();
    void UpdateFrameConstants(
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& projection,
        int screenWidth,
        int screenHeight);
};
