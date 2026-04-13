#include "ObjLoader.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <algorithm>
#include <cmath>

using namespace DirectX;

// ─── helpers ─────────────────────────────────────────────────────────────────

struct FaceIndex
{
    int v = 0, vt = 0, vn = 0;
    bool operator==(const FaceIndex& o) const { return v == o.v && vt == o.vt && vn == o.vn; }
};

struct FaceIndexHash
{
    size_t operator()(const FaceIndex& f) const
    {
        size_t h = std::hash<int>{}(f.v);
        h ^= std::hash<int>{}(f.vt) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<int>{}(f.vn) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

static FaceIndex ParseVertex(const std::string& token)
{
    // formats: v   v/vt   v//vn   v/vt/vn
    FaceIndex fi;
    std::istringstream ss(token);
    std::string part;
    int idx = 0;
    while (std::getline(ss, part, '/'))
    {
        int val = part.empty() ? 0 : std::stoi(part);
        if (idx == 0) fi.v  = val;
        else if (idx == 1) fi.vt = val;
        else if (idx == 2) fi.vn = val;
        ++idx;
    }
    return fi;
}

// ─── ObjLoader::Load ─────────────────────────────────────────────────────────

MeshData ObjLoader::Load(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
        throw std::runtime_error("ObjLoader: cannot open file: " + path);

    std::vector<XMFLOAT3> positions;
    std::vector<XMFLOAT3> normals;
    std::vector<XMFLOAT2> uvs;

    MeshData result;
    std::unordered_map<FaceIndex, uint32_t, FaceIndexHash> indexCache;

    auto resolveVertex = [&](const FaceIndex& fi) -> uint32_t
    {
        auto it = indexCache.find(fi);
        if (it != indexCache.end()) return it->second;

        MeshVertex mv = {};

        // OBJ indices are 1-based; negative means from-end
        auto resolve1 = [](int idx, int size) -> int
        {
            return idx > 0 ? idx - 1 : size + idx;
        };

        if (fi.v  != 0 && fi.v  <= (int)positions.size()) mv.Position = positions[resolve1(fi.v,  (int)positions.size())];
        if (fi.vt != 0 && fi.vt <= (int)uvs.size())       mv.UV       = uvs      [resolve1(fi.vt, (int)uvs.size())];
        if (fi.vn != 0 && fi.vn <= (int)normals.size())   mv.Normal   = normals  [resolve1(fi.vn, (int)normals.size())];

        uint32_t newIdx = static_cast<uint32_t>(result.Vertices.size());
        result.Vertices.push_back(mv);
        indexCache[fi] = newIdx;
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
            XMFLOAT3 p;
            ss >> p.x >> p.y >> p.z;
            positions.push_back(p);
        }
        else if (token == "vn")
        {
            XMFLOAT3 n;
            ss >> n.x >> n.y >> n.z;
            normals.push_back(n);
        }
        else if (token == "vt")
        {
            XMFLOAT2 uv;
            ss >> uv.x >> uv.y;
            uv.y = 1.0f - uv.y;   // flip V for D3D
            uvs.push_back(uv);
        }
        else if (token == "f")
        {
            // Collect all vertex refs on the face, triangulate as fan
            std::vector<uint32_t> faceVerts;
            std::string vToken;
            while (ss >> vToken)
                faceVerts.push_back(resolveVertex(ParseVertex(vToken)));

            for (size_t i = 1; i + 1 < faceVerts.size(); ++i)
            {
                result.Indices.push_back(faceVerts[0]);
                result.Indices.push_back(faceVerts[i]);
                result.Indices.push_back(faceVerts[i + 1]);
            }
        }
    }

    if (result.Vertices.empty())
        throw std::runtime_error("ObjLoader: no geometry in file: " + path);

    // Generate flat normals if the OBJ has none
    bool hasNormals = normals.empty() == false;
    if (!hasNormals)
    {
        // zero out, then accumulate face normals
        for (auto& mv : result.Vertices)
            mv.Normal = { 0, 0, 0 };

        for (size_t i = 0; i + 2 < result.Indices.size(); i += 3)
        {
            auto& v0 = result.Vertices[result.Indices[i + 0]];
            auto& v1 = result.Vertices[result.Indices[i + 1]];
            auto& v2 = result.Vertices[result.Indices[i + 2]];

            XMVECTOR p0 = XMLoadFloat3(&v0.Position);
            XMVECTOR p1 = XMLoadFloat3(&v1.Position);
            XMVECTOR p2 = XMLoadFloat3(&v2.Position);
            XMVECTOR n  = XMVector3Normalize(XMVector3Cross(p1 - p0, p2 - p0));

            XMFLOAT3 fn;
            XMStoreFloat3(&fn, n);
            v0.Normal.x += fn.x; v0.Normal.y += fn.y; v0.Normal.z += fn.z;
            v1.Normal.x += fn.x; v1.Normal.y += fn.y; v1.Normal.z += fn.z;
            v2.Normal.x += fn.x; v2.Normal.y += fn.y; v2.Normal.z += fn.z;
        }

        for (auto& mv : result.Vertices)
        {
            XMVECTOR n = XMVector3Normalize(XMLoadFloat3(&mv.Normal));
            XMStoreFloat3(&mv.Normal, n);
        }
    }

    Center(result);
    ComputeBoundingRadius(result);
    return result;
}

void ObjLoader::Center(MeshData& data)
{
    XMFLOAT3 mn = { 1e30f, 1e30f, 1e30f };
    XMFLOAT3 mx = {-1e30f,-1e30f,-1e30f };

    for (const auto& v : data.Vertices)
    {
        mn.x = std::min(mn.x, v.Position.x); mn.y = std::min(mn.y, v.Position.y); mn.z = std::min(mn.z, v.Position.z);
        mx.x = std::max(mx.x, v.Position.x); mx.y = std::max(mx.y, v.Position.y); mx.z = std::max(mx.z, v.Position.z);
    }

    XMFLOAT3 center = { (mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f, (mn.z + mx.z) * 0.5f };
    for (auto& v : data.Vertices)
    {
        v.Position.x -= center.x;
        v.Position.y -= center.y;
        v.Position.z -= center.z;
    }
}

void ObjLoader::ComputeBoundingRadius(MeshData& data)
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
