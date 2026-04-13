#include "ObjMesh.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <algorithm>
#include <cmath>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

// ─── Helpers ──────────────────────────────────────────────────────────────────

namespace
{
    struct ObjIndex { int v = 0, vt = 0, vn = 0; };

    bool ParseFaceToken(const std::string& token, ObjIndex& out)
    {
        // formats: "v", "v/vt", "v//vn", "v/vt/vn"
        int vals[3] = { 0, 0, 0 };
        int idx = 0;
        std::string cur;
        for (char c : token + "/")
        {
            if (c == '/')
            {
                if (!cur.empty()) vals[idx] = std::stoi(cur);
                cur.clear();
                ++idx;
                if (idx == 3) break;
            }
            else cur += c;
        }
        out.v = vals[0];
        out.vt = vals[1];
        out.vn = vals[2];
        return out.v != 0;
    }

    // Convert negative (relative) indices
    int ResolveIndex(int raw, int count)
    {
        return raw < 0 ? count + raw : raw - 1;  // OBJ is 1-based
    }

    struct IndexKey
    {
        int v, vt, vn;
        bool operator==(const IndexKey& o) const
        {
            return v == o.v && vt == o.vt && vn == o.vn;
        }
    };

    struct IndexKeyHash
    {
        size_t operator()(const IndexKey& k) const
        {
            size_t h = 0;
            auto mix = [&](int x) { h ^= std::hash<int>{}(x)+0x9e3779b9 + (h << 6) + (h >> 2); };
            mix(k.v); mix(k.vt); mix(k.vn);
            return h;
        }
    };
}

// ─── Load ─────────────────────────────────────────────────────────────────────

ObjMesh ObjMesh::LoadFromFile(ID3D11Device* device, const std::wstring& path)
{
    // Преобразуем wstring в string для совместимости с ifstream
    std::string narrowPath(path.begin(), path.end());
    std::ifstream file(narrowPath);
    if (!file.is_open())
    {
        throw std::runtime_error("ObjMesh: cannot open file at " + narrowPath);
    }

    std::vector<XMFLOAT3> positions;
    std::vector<XMFLOAT3> normals;
    std::vector<XMFLOAT2> texCoords;

    std::vector<MeshVertex>  finalVerts;
    std::vector<uint32_t>    finalIdx;

    std::unordered_map<IndexKey, uint32_t, IndexKeyHash> cache;

    auto getVertex = [&](ObjIndex i) -> uint32_t
        {
            IndexKey key{ ResolveIndex(i.v,  (int)positions.size()),
                          ResolveIndex(i.vt, (int)texCoords.size()),
                          ResolveIndex(i.vn, (int)normals.size()) };

            auto it = cache.find(key);
            if (it != cache.end()) return it->second;

            MeshVertex mv = {};
            if (key.v >= 0 && key.v < (int)positions.size())  mv.Position = positions[key.v];
            if (key.vn >= 0 && key.vn < (int)normals.size())    mv.Normal = normals[key.vn];
            if (key.vt >= 0 && key.vt < (int)texCoords.size())  mv.TexCoord = texCoords[key.vt];

            uint32_t newIdx = static_cast<uint32_t>(finalVerts.size());
            finalVerts.push_back(mv);
            cache[key] = newIdx;
            return newIdx;
        };

    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        std::string token;
        ss >> token;

        if (token == "v")
        {
            XMFLOAT3 p; ss >> p.x >> p.y >> p.z;
            positions.push_back(p);
        }
        else if (token == "vn")
        {
            XMFLOAT3 n; ss >> n.x >> n.y >> n.z;
            normals.push_back(n);
        }
        else if (token == "vt")
        {
            XMFLOAT2 uv; ss >> uv.x >> uv.y;
            uv.y = 1.f - uv.y; // flip V for D3D
            texCoords.push_back(uv);
        }
        else if (token == "f")
        {
            // Triangulate polygon fan (handles quads & ngons)
            std::vector<uint32_t> faceVerts;
            std::string ft;
            while (ss >> ft)
            {
                ObjIndex oi;
                if (ParseFaceToken(ft, oi))
                    faceVerts.push_back(getVertex(oi));
            }
            for (size_t k = 1; k + 1 < faceVerts.size(); ++k)
            {
                finalIdx.push_back(faceVerts[0]);
                finalIdx.push_back(faceVerts[k]);
                finalIdx.push_back(faceVerts[k + 1]);
            }
        }
    }

    if (finalVerts.empty())
        throw std::runtime_error("ObjMesh: no geometry parsed.");

    // Generate flat normals if missing
    bool hasNormals = !normals.empty();
    if (!hasNormals)
    {
        for (auto& v : finalVerts) v.Normal = { 0.f, 1.f, 0.f };
        for (size_t i = 0; i + 2 < finalIdx.size(); i += 3)
        {
            auto& v0 = finalVerts[finalIdx[i]];
            auto& v1 = finalVerts[finalIdx[i + 1]];
            auto& v2 = finalVerts[finalIdx[i + 2]];
            XMVECTOR p0 = XMLoadFloat3(&v0.Position);
            XMVECTOR e1 = XMLoadFloat3(&v1.Position) - p0;
            XMVECTOR e2 = XMLoadFloat3(&v2.Position) - p0;
            XMVECTOR n = XMVector3Normalize(XMVector3Cross(e1, e2));
            XMFLOAT3 nf; XMStoreFloat3(&nf, n);
            v0.Normal = v1.Normal = v2.Normal = nf;
        }
    }

    // ── Upload to GPU ──────────────────────────────────────────────────────────
    ObjMesh mesh;

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.ByteWidth = static_cast<UINT>(sizeof(MeshVertex) * finalVerts.size());

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = finalVerts.data();

    HRESULT hr = device->CreateBuffer(&vbDesc, &vbData, mesh.VertexBuffer.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("ObjMesh: CreateBuffer (vertex) failed.");

    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibDesc.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * finalIdx.size());

    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = finalIdx.data();

    hr = device->CreateBuffer(&ibDesc, &ibData, mesh.IndexBuffer.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("ObjMesh: CreateBuffer (index) failed.");

    mesh.IndexCount = static_cast<UINT>(finalIdx.size());
    mesh.Bounds = ComputeBounds(finalVerts);

    return mesh;
}

// ─── Bounding sphere (Ritter's algorithm) ─────────────────────────────────────

MeshBoundingSphere ObjMesh::ComputeBounds(const std::vector<MeshVertex>& verts)
{
    if (verts.empty()) return {};

    // Initial sphere from AABB center
    XMFLOAT3 mn = verts[0].Position, mx = verts[0].Position;
    for (auto& v : verts)
    {
        mn.x = (std::min)(mn.x, v.Position.x);
        mn.y = (std::min)(mn.y, v.Position.y);
        mn.z = (std::min)(mn.z, v.Position.z);
        mx.x = (std::max)(mx.x, v.Position.x);
        mx.y = (std::max)(mx.y, v.Position.y);
        mx.z = (std::max)(mx.z, v.Position.z);
    }

    MeshBoundingSphere bs;
    bs.Center = { (mn.x + mx.x) * .5f, (mn.y + mx.y) * .5f, (mn.z + mx.z) * .5f };
    bs.Radius = 0.f;

    XMVECTOR c = XMLoadFloat3(&bs.Center);
    for (auto& v : verts)
    {
        float d = XMVectorGetX(XMVector3Length(XMLoadFloat3(&v.Position) - c));
        if (d > bs.Radius) bs.Radius = d;
    }
    return bs;
}