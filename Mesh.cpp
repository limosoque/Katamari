#include "Mesh.h"
#include "Game.h"
#include <stdexcept>

void Mesh::Upload(Game* game, const MeshData& data)
{
    auto* device = game->Device.Get();
    boundingRadius = data.BoundingRadius;
    vertexStride   = sizeof(MeshVertex);
    indexCount     = static_cast<UINT>(data.Indices.size());

    // Vertex buffer
    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage     = D3D11_USAGE_DEFAULT;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.ByteWidth = static_cast<UINT>(sizeof(MeshVertex) * data.Vertices.size());

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = data.Vertices.data();

    HRESULT hr = device->CreateBuffer(&vbDesc, &vbData, vertexBuffer.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Mesh: CreateBuffer (vertex) failed.");

    // Index buffer
    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.Usage     = D3D11_USAGE_DEFAULT;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibDesc.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * data.Indices.size());

    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = data.Indices.data();

    hr = device->CreateBuffer(&ibDesc, &ibData, indexBuffer.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Mesh: CreateBuffer (index) failed.");
}

void Mesh::Bind(ID3D11DeviceContext* ctx) const
{
    UINT offset = 0;
    ID3D11Buffer* vb = vertexBuffer.Get();
    ctx->IASetVertexBuffers(0, 1, &vb, &vertexStride, &offset);
    ctx->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void Mesh::Draw(ID3D11DeviceContext* ctx) const
{
    ctx->DrawIndexed(indexCount, 0, 0);
}
