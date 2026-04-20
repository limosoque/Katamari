#include "MeshGenerator.h"
#include <cmath>
#include <algorithm>

using namespace DirectX;

void MeshGenerator::ComputeBoundingRadius(MeshData& data)
{
    float maxR2 = 0.0f;
    for (const auto& v : data.Vertices)
    {
        float r2 = v.Position.x * v.Position.x
            + v.Position.y * v.Position.y
            + v.Position.z * v.Position.z;
        if (r2 > maxR2) maxR2 = r2;
    }
    data.BoundingRadius = std::sqrt(maxR2);
}

//Sphere//
//  UV-sphere layout:
//
//        north pole  (0, +r, 0)
//        stack 0 … stacks-1   (latitude rings, sin/cos parameterised)
//        south pole  (0, -r, 0)
//
//  Each stack ring has (slices+1) vertices (first == last for UV seam).
//  Triangles: top cap (fan), body quads, bottom cap (fan).

MeshData MeshGenerator::CreateSphere(float radius, int stacks, int slices)
{
    stacks = std::max(stacks, 2);
    slices = std::max(slices, 3);

    MeshData data;
    const float pi = XM_PI;
    const float tau = XM_2PI;

    // North pole
    data.Vertices.push_back({ { 0.0f, radius, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.5f, 0.0f } });

    // Rings
    for (int i = 1; i < stacks; ++i)
    {
        float phi = pi * static_cast<float>(i) / static_cast<float>(stacks);  // 0..pi
        float y = radius * std::cos(phi);
        float r = radius * std::sin(phi);

        for (int j = 0; j <= slices; ++j)
        {
            float theta = tau * static_cast<float>(j) / static_cast<float>(slices);
            float x = r * std::sin(theta);
            float z = r * std::cos(theta);

            XMFLOAT3 pos = { x, y, z };
            // Normal is just the normalised position for a unit sphere
            XMFLOAT3 n = { x / radius, y / radius, z / radius };
            XMFLOAT2 uv = { static_cast<float>(j) / static_cast<float>(slices),
                             static_cast<float>(i) / static_cast<float>(stacks) };
            data.Vertices.push_back({ pos, n, uv });
        }
    }

    //South pole
    data.Vertices.push_back({ { 0.0f, -radius, 0.0f }, { 0.0f, -1.0f, 0.0f }, { 0.5f, 1.0f } });

    // ── indices ──────────────────────────────────────────────────────────────
    // Each ring has (slices+1) vertices; ring i starts at index 1 + i*(slices+1).

    auto ringStart = [&](int i) -> uint32_t
        {
            return 1u + static_cast<uint32_t>(i) * static_cast<uint32_t>(slices + 1);
        };

    // Top cap — fan from north pole (index 0) to first ring
    for (int j = 0; j < slices; ++j)
    {
        data.Indices.push_back(0);
        data.Indices.push_back(ringStart(0) + j);
        data.Indices.push_back(ringStart(0) + j + 1);
    }

    // Body — quads between adjacent rings
    for (int i = 0; i < stacks - 2; ++i)
    {
        uint32_t top = ringStart(i);
        uint32_t bot = ringStart(i + 1);
        for (int j = 0; j < slices; ++j)
        {
            // Two triangles per quad
            data.Indices.push_back(top + j);
            data.Indices.push_back(bot + j);
            data.Indices.push_back(top + j + 1);

            data.Indices.push_back(bot + j);
            data.Indices.push_back(bot + j + 1);
            data.Indices.push_back(top + j + 1);
        }
    }

    // Bottom cap — fan from last ring to south pole
    uint32_t lastRing = ringStart(stacks - 2);
    uint32_t southPole = static_cast<uint32_t>(data.Vertices.size()) - 1u;
    for (int j = 0; j < slices; ++j)
    {
        data.Indices.push_back(lastRing + j);
        data.Indices.push_back(southPole);
        data.Indices.push_back(lastRing + j + 1);
    }

    data.BoundingRadius = radius;
    return data;
}