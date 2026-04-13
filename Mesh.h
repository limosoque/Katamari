#pragma once
#include "ObjLoader.h"
#include <d3d11.h>
#include <wrl/client.h>

class Game;

/// GPU representation of a mesh: vertex buffer + index buffer.
class Mesh
{
public:
    Mesh() = default;

    /// Upload MeshData to GPU.  Throws on D3D failure.
    void Upload(Game* game, const MeshData& data);

    void Bind(ID3D11DeviceContext* ctx) const;
    void Draw(ID3D11DeviceContext* ctx) const;

    float BoundingRadius() const { return boundingRadius; }
    bool  IsValid()        const { return indexCount > 0; }

private:
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
    UINT  indexCount     = 0;
    UINT  vertexStride   = 0;
    float boundingRadius = 0.0f;
};
