#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <string>

class Game;

class ShadowMapHud
{
public:
    bool Enabled = false;
    bool UseDepthContours = false;
    float Exposure = 1.0f;
    float PanelSize = 220.0f;
    float PanelPadding = 12.0f;
    float PanelMargin = 16.0f;

    explicit ShadowMapHud(std::wstring shaderPath = L"shaders/ShadowMapHud.hlsl");

    void Initialize(Game* owner);
    void Draw(ID3D11ShaderResourceView* shadowSrv, int cascadeCount, int screenWidth, int screenHeight);
    void DestroyResources();

private:
    struct alignas(16) HudCB
    {
        DirectX::XMFLOAT4 PanelRect;     // x, y, width, height in pixels.
        DirectX::XMFLOAT4 ScreenSize;    // width, height, 1 / width, 1 / height.
        DirectX::XMFLOAT4 Params;        // x = cascade index, y = exposure, z = contour mode, w = border px.
        DirectX::XMFLOAT4 BorderColor;   // rgba debug border color.
    };

    Game* game = nullptr;
    std::wstring shaderPath;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerState;
    Microsoft::WRL::ComPtr<ID3D11BlendState> blendState;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthDisabledState;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerState;

    void CompileShaders();
    void CreateConstantBuffer();
    void CreateSamplerState();
    void CreateBlendState();
    void CreateDepthStencilState();
    void CreateRasterizerState();
    void UpdateConstantBuffer(int cascadeIndex, int panelCount, int screenWidth, int screenHeight);
};
