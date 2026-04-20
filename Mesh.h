#pragma once
#include "ObjLoader.h"
#include <d3d11.h>
#include <wrl/client.h>

class Game;

class Mesh
{
public:
    Mesh() = default;

    void Upload(Game* game, const MeshData& data);

    void Bind(ID3D11DeviceContext* ctx) const;
    void Draw(ID3D11DeviceContext* ctx) const;

    float BoundingRadius() const { return boundingRadius; }
    bool  IsValid() const { return indexCount > 0; }

    ID3D11ShaderResourceView* GetTexture() const { return textureView.Get(); }
    void LoadTexture(ID3D11Device* device, const std::wstring& path);

private:
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> textureView;

    UINT  indexCount = 0;
    UINT  vertexStride = 0;
    float boundingRadius = 0.0f;
};
