#pragma once

#include <array>
#include <d3d11.h>
#include <wrl/client.h>

class Game;

const unsigned int kGBufferTargetCount = 6;

class GBuffer
{
public:
    enum Target : unsigned int
    {
        AlbedoGroundMask = 0,
        NormalShininess = 1,
        SpecularViewDepth = 2,
        WorldPosition = 3,
        Ambient = 4,
        ObjectId = 5
    };

    void Initialize(Game* owner, int width, int height);
    void Resize(int width, int height);
    void BindForWriting(ID3D11DepthStencilView* depthView);
    void Clear(ID3D11DepthStencilView* depthView);
    void BindForReading(unsigned int startSlot) const;
    void UnbindFromShader(unsigned int startSlot) const;
    void DestroyResources();

    int Width() const { return width; }
    int Height() const { return height; }
    ID3D11ShaderResourceView* GetShaderResourceView(Target target) const
    {
        return shaderResourceViews[static_cast<unsigned int>(target)].Get();
    }

private:
    Game* game = nullptr;
    int width = 0;
    int height = 0;

    std::array<DXGI_FORMAT, kGBufferTargetCount> formats =
    {
        DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_R16G16B16A16_FLOAT,
        DXGI_FORMAT_R16G16B16A16_FLOAT,
        DXGI_FORMAT_R32G32B32A32_FLOAT,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_R32_UINT
    };

    std::array<Microsoft::WRL::ComPtr<ID3D11Texture2D>, kGBufferTargetCount> textures;
    std::array<Microsoft::WRL::ComPtr<ID3D11RenderTargetView>, kGBufferTargetCount> renderTargetViews;
    std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, kGBufferTargetCount> shaderResourceViews;

    void CreateResources();
};
