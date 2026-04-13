#include "MeshGenerator.h"
#include <cmath>
#include <algorithm>

using namespace DirectX;

// ─── helper ──────────────────────────────────────────────────────────────────

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

// ─── Sphere ──────────────────────────────────────────────────────────────────
//
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

    // ── vertices ─────────────────────────────────────────────────────────────
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

    // South pole
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
        data.Indices.push_back(ringStart(0) + j + 1);
        data.Indices.push_back(ringStart(0) + j);
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
            data.Indices.push_back(top + j + 1);
            data.Indices.push_back(bot + j);

            data.Indices.push_back(bot + j);
            data.Indices.push_back(top + j + 1);
            data.Indices.push_back(bot + j + 1);
        }
    }

    // Bottom cap — fan from last ring to south pole
    uint32_t lastRing = ringStart(stacks - 2);
    uint32_t southPole = static_cast<uint32_t>(data.Vertices.size()) - 1u;
    for (int j = 0; j < slices; ++j)
    {
        data.Indices.push_back(lastRing + j);
        data.Indices.push_back(lastRing + j + 1);
        data.Indices.push_back(southPole);
    }

    data.BoundingRadius = radius;
    return data;
}

// ─── Box ─────────────────────────────────────────────────────────────────────
//
//  6 faces, 4 vertices each (no shared vertices — sharp normals per face).

MeshData MeshGenerator::CreateBox(float hx, float hy, float hz)
{
    MeshData data;

    // Each face is defined by its outward normal and 4 corners (CCW from outside).
    // We avoid nested brace-initialisation of XMFLOAT3 arrays to stay compatible
    // with MSVC in C++17 mode where aggregate-init of DirectXMath types can fail.

    struct FaceDef
    {
        XMFLOAT3 normal;
        XMFLOAT3 v[4];
    };

    FaceDef faces[6];

    // +X
    faces[0].normal = XMFLOAT3(1, 0, 0);
    faces[0].v[0] = XMFLOAT3(hx, -hy, -hz);
    faces[0].v[1] = XMFLOAT3(hx, hy, -hz);
    faces[0].v[2] = XMFLOAT3(hx, hy, hz);
    faces[0].v[3] = XMFLOAT3(hx, -hy, hz);

    // -X
    faces[1].normal = XMFLOAT3(-1, 0, 0);
    faces[1].v[0] = XMFLOAT3(-hx, -hy, hz);
    faces[1].v[1] = XMFLOAT3(-hx, hy, hz);
    faces[1].v[2] = XMFLOAT3(-hx, hy, -hz);
    faces[1].v[3] = XMFLOAT3(-hx, -hy, -hz);

    // +Y
    faces[2].normal = XMFLOAT3(0, 1, 0);
    faces[2].v[0] = XMFLOAT3(-hx, hy, -hz);
    faces[2].v[1] = XMFLOAT3(hx, hy, -hz);
    faces[2].v[2] = XMFLOAT3(hx, hy, hz);
    faces[2].v[3] = XMFLOAT3(-hx, hy, hz);

    // -Y
    faces[3].normal = XMFLOAT3(0, -1, 0);
    faces[3].v[0] = XMFLOAT3(-hx, -hy, hz);
    faces[3].v[1] = XMFLOAT3(hx, -hy, hz);
    faces[3].v[2] = XMFLOAT3(hx, -hy, -hz);
    faces[3].v[3] = XMFLOAT3(-hx, -hy, -hz);

    // +Z
    faces[4].normal = XMFLOAT3(0, 0, 1);
    faces[4].v[0] = XMFLOAT3(hx, -hy, hz);
    faces[4].v[1] = XMFLOAT3(hx, hy, hz);
    faces[4].v[2] = XMFLOAT3(-hx, hy, hz);
    faces[4].v[3] = XMFLOAT3(-hx, -hy, hz);

    // -Z
    faces[5].normal = XMFLOAT3(0, 0, -1);
    faces[5].v[0] = XMFLOAT3(-hx, -hy, -hz);
    faces[5].v[1] = XMFLOAT3(-hx, hy, -hz);
    faces[5].v[2] = XMFLOAT3(hx, hy, -hz);
    faces[5].v[3] = XMFLOAT3(hx, -hy, -hz);

    const XMFLOAT2 uvs[4] = {
        XMFLOAT2(0,1), XMFLOAT2(0,0), XMFLOAT2(1,0), XMFLOAT2(1,1)
    };

    for (const auto& f : faces)
    {
        uint32_t base = static_cast<uint32_t>(data.Vertices.size());
        for (int i = 0; i < 4; ++i)
            data.Vertices.push_back({ f.v[i], f.normal, uvs[i] });

        data.Indices.insert(data.Indices.end(),
            { base, base + 1, base + 2,  base, base + 2, base + 3 });
    }

    ComputeBoundingRadius(data);
    return data;
}


// ─── Cylinder ────────────────────────────────────────────────────────────────

MeshData MeshGenerator::CreateCylinder(float radius, float halfH, int slices)
{
    slices = std::max(slices, 3);
    MeshData data;
    const float tau = XM_2PI;

    // Side vertices (two rings: top and bottom, with normals pointing outward)
    for (int cap = 0; cap <= 1; ++cap)   // 0=top, 1=bottom
    {
        float y = (cap == 0) ? halfH : -halfH;
        for (int j = 0; j <= slices; ++j)
        {
            float theta = tau * static_cast<float>(j) / static_cast<float>(slices);
            float x = radius * std::sin(theta);
            float z = radius * std::cos(theta);
            float u = static_cast<float>(j) / static_cast<float>(slices);
            float v = (cap == 0) ? 0.0f : 1.0f;
            data.Vertices.push_back({ {x,y,z}, {x / radius, 0.0f, z / radius}, {u,v} });
        }
    }

    // Side indices
    for (int j = 0; j < slices; ++j)
    {
        uint32_t t = static_cast<uint32_t>(j);
        uint32_t b = static_cast<uint32_t>(slices + 1 + j);
        data.Indices.insert(data.Indices.end(), { t, b, t + 1, t + 1, b, b + 1 });
    }

    // Cap helper
    auto addCap = [&](float y, XMFLOAT3 normal, bool flipWinding)
        {
            uint32_t centre = static_cast<uint32_t>(data.Vertices.size());
            data.Vertices.push_back({ {0,y,0}, normal, {0.5f,0.5f} });

            uint32_t ringStart = centre + 1;
            for (int j = 0; j <= slices; ++j)
            {
                float theta = tau * static_cast<float>(j) / static_cast<float>(slices);
                float x = radius * std::sin(theta);
                float z = radius * std::cos(theta);
                float u = 0.5f + 0.5f * std::sin(theta);
                float v = 0.5f + 0.5f * std::cos(theta);
                data.Vertices.push_back({ {x,y,z}, normal, {u,v} });
            }

            for (int j = 0; j < slices; ++j)
            {
                if (!flipWinding)
                    data.Indices.insert(data.Indices.end(),
                        { centre, ringStart + j, ringStart + j + 1 });
                else
                    data.Indices.insert(data.Indices.end(),
                        { centre, ringStart + j + 1, ringStart + j });
            }
        };

    addCap(halfH, { 0, 1,0 }, false);   // top cap, normal +Y
    addCap(-halfH, { 0,-1,0 }, true);    // bottom cap, normal -Y

    ComputeBoundingRadius(data);
    return data;
}

// ─── Cone ────────────────────────────────────────────────────────────────────

MeshData MeshGenerator::CreateCone(float radius, float halfH, int slices)
{
    slices = std::max(slices, 3);
    MeshData data;
    const float tau = XM_2PI;

    // Side: tip at (0,+halfH,0), base ring at y=-halfH
    // The outward side normal for a cone: n = (cos(theta), r/height, sin(theta)) normalised
    float height = 2.0f * halfH;
    float nY = radius / std::sqrt(radius * radius + height * height);
    float nXZ = height / std::sqrt(radius * radius + height * height);

    // Base ring vertices (for the side surface)
    uint32_t baseRingStart = 0;
    for (int j = 0; j <= slices; ++j)
    {
        float theta = tau * static_cast<float>(j) / static_cast<float>(slices);
        float x = radius * std::sin(theta);
        float z = radius * std::cos(theta);
        float nx = nXZ * std::sin(theta);
        float nz = nXZ * std::cos(theta);
        float u = static_cast<float>(j) / static_cast<float>(slices);
        data.Vertices.push_back({ {x,-halfH,z}, {nx, nY, nz}, {u, 1.0f} });
    }

    // Tip vertex (replicated per-slice for unique UVs, but one shared is fine here)
    uint32_t tipStart = static_cast<uint32_t>(data.Vertices.size());
    for (int j = 0; j < slices; ++j)
    {
        float theta = tau * (static_cast<float>(j) + 0.5f) / static_cast<float>(slices);
        float nx = nXZ * std::sin(theta);
        float nz = nXZ * std::cos(theta);
        float u = (static_cast<float>(j) + 0.5f) / static_cast<float>(slices);
        data.Vertices.push_back({ {0, halfH, 0}, {nx, nY, nz}, {u, 0.0f} });
    }

    // Side triangles
    for (int j = 0; j < slices; ++j)
    {
        data.Indices.insert(data.Indices.end(),
            {
                baseRingStart + j,
                tipStart + j,
                baseRingStart + j + 1
            });
    }

    // Bottom cap
    uint32_t capCentre = static_cast<uint32_t>(data.Vertices.size());
    data.Vertices.push_back({ {0,-halfH,0}, {0,-1,0}, {0.5f,0.5f} });
    uint32_t capRing = capCentre + 1;
    for (int j = 0; j <= slices; ++j)
    {
        float theta = tau * static_cast<float>(j) / static_cast<float>(slices);
        float x = radius * std::sin(theta);
        float z = radius * std::cos(theta);
        float u = 0.5f + 0.5f * std::sin(theta);
        float v = 0.5f + 0.5f * std::cos(theta);
        data.Vertices.push_back({ {x,-halfH,z}, {0,-1,0}, {u,v} });
    }
    for (int j = 0; j < slices; ++j)
    {
        data.Indices.insert(data.Indices.end(),
            { capCentre, capRing + j + 1, capRing + j });
    }

    ComputeBoundingRadius(data);
    return data;
}
