#pragma once
#include <vector>
#include <string>
#include <DirectXMath.h>

struct MeshVertex
{
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT3 Normal;
    DirectX::XMFLOAT2 UV;
};

struct MeshData
{
    std::vector<MeshVertex> Vertices;
    std::vector<uint32_t>   Indices;

    float BoundingRadius = 0.0f;
};

/// Minimal Wavefront OBJ loader.
/// Supports: v, vn, vt, f (triangulated or quads).
/// Does NOT support materials — pure geometry only.
class ObjLoader
{
public:
    /// Load an OBJ file.  Centers the mesh at the origin and computes the bounding radius.
    /// Throws std::runtime_error on failure.
    static MeshData Load(const std::string& path);

private:
    static void Center(MeshData& data);
    static void ComputeBoundingRadius(MeshData& data);
};
