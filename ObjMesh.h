#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <vector>
#include <string>

struct MeshVertex
{
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT3 Normal;
    DirectX::XMFLOAT2 TexCoord;
};

/// ТАКЖЕ переименовываем структуру здесь
struct MeshBoundingSphere
{
    DirectX::XMFLOAT3 Center = { 0.f, 0.f, 0.f };
    float             Radius = 1.f;
};

class ObjMesh
{
public:
    Microsoft::WRL::ComPtr<ID3D11Buffer> VertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> IndexBuffer;
    UINT                                 IndexCount = 0;

    // ИСПОЛЬЗУЕМ новое имя
    MeshBoundingSphere                   Bounds;

    static ObjMesh LoadFromFile(ID3D11Device* device, const std::wstring& path);

private:
    // И здесь в сигнатуре функции
    static MeshBoundingSphere ComputeBounds(const std::vector<MeshVertex>& verts);
};