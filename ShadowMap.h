#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <array>
#include <stdexcept>

// Number of shadow cascades.
static constexpr int kCascadeCount = 3;

// Resolution of each cascade shadow map (square).
// 2048 gives good quality; reduce to 1024 if GPU memory is tight.
static constexpr int kShadowMapSize = 2048;

// ─────────────────────────────────────────────────────────────────────────────
// GetFrustumCornersWorldSpace
// ─────────────────────────────────────────────────────────────────────────────
// Exactly as shown by the teacher: iterate x,y,z in {0,1} to enumerate all 8
// NDC corners, unproject each through (view*proj)^-1, perspective-divide.
//
// Returns 8 world-space float4 vectors (w=1 after divide).
// ─────────────────────────────────────────────────────────────────────────────
inline std::vector<DirectX::XMFLOAT4> GetFrustumCornersWorldSpace(
    const DirectX::XMMATRIX& view,
    const DirectX::XMMATRIX& proj)
{
    using namespace DirectX;

    XMMATRIX viewProj = view * proj;
    XMMATRIX inv = XMMatrixInverse(nullptr, viewProj);

    std::vector<XMFLOAT4> corners;
    corners.reserve(8);

    for (unsigned int x = 0; x < 2; ++x)
        for (unsigned int y = 0; y < 2; ++y)
            for (unsigned int z = 0; z < 2; ++z)
            {
                // NDC corners: x,y ∈ {-1,+1},  z ∈ {0,1} (D3D depth convention)
                XMVECTOR pt = XMVectorSet(
                    2.0f * x - 1.0f,
                    2.0f * y - 1.0f,
                    static_cast<float>(z),
                    1.0f);

                XMVECTOR world = XMVector4Transform(pt, inv);
                XMFLOAT4 w;
                XMStoreFloat4(&w, world / XMVectorSplatW(world));   // perspective divide
                corners.push_back(w);
            }

    return corners;
}

// ─────────────────────────────────────────────────────────────────────────────
// ShadowData
//
// GPU resources: one Texture2DArray (ArraySize = kCascadeCount) with a single
// DSV covering all slices and a single SRV — matching the teacher's approach.
// The GS will route each triangle into the correct array slice via
// SV_RenderTargetArrayIndex.
// ─────────────────────────────────────────────────────────────────────────────
struct ShadowData
{
    // One texture array holds all cascade depth maps as slices.
    Microsoft::WRL::ComPtr<ID3D11Texture2D>          shadowTexArray;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView>   dsv;        // all slices at once
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;        // Texture2DArray SRV

    // Per-cascade light ViewProj matrices (transposed, row-major for HLSL).
    std::array<DirectX::XMFLOAT4X4, kCascadeCount> lightViewProj;

    // Split depths in camera view space (positive). Pixel shader reads these
    // to choose which cascade slice to sample.
    std::array<float, kCascadeCount> splitDepths = { 12.0f, 17.0f, 90.0f };

    // ─── Create all GPU resources ─────────────────────────────────────────────
    void Create(ID3D11Device* device)
    {
        // Single Texture2DArray: depth only, kCascadeCount slices.
        // R32_TYPELESS so we can create both a D32_FLOAT DSV and an R32_FLOAT SRV.
        D3D11_TEXTURE2D_DESC td = {};
        td.Width = kShadowMapSize;
        td.Height = kShadowMapSize;
        td.MipLevels = 1;
        td.ArraySize = kCascadeCount;          // ← all cascades in one array
        td.Format = DXGI_FORMAT_R32_TYPELESS;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

        if (FAILED(device->CreateTexture2D(&td, nullptr, shadowTexArray.GetAddressOf())))
            throw std::runtime_error("ShadowMap: CreateTexture2D (array) failed");

        // DSV — covers the entire array (all slices rendered in one GS pass).
        D3D11_DEPTH_STENCIL_VIEW_DESC dsvd = {};
        dsvd.Format = DXGI_FORMAT_D32_FLOAT;
        dsvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
        dsvd.Texture2DArray.MipSlice = 0;
        dsvd.Texture2DArray.FirstArraySlice = 0;
        dsvd.Texture2DArray.ArraySize = kCascadeCount;

        if (FAILED(device->CreateDepthStencilView(shadowTexArray.Get(), &dsvd, dsv.GetAddressOf())))
            throw std::runtime_error("ShadowMap: CreateDepthStencilView failed");

        // SRV — Texture2DArray so the pixel shader can index by slice.
        D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {};
        srvd.Format = DXGI_FORMAT_R32_FLOAT;
        srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
        srvd.Texture2DArray.MostDetailedMip = 0;
        srvd.Texture2DArray.MipLevels = 1;
        srvd.Texture2DArray.FirstArraySlice = 0;
        srvd.Texture2DArray.ArraySize = kCascadeCount;

        if (FAILED(device->CreateShaderResourceView(shadowTexArray.Get(), &srvd, srv.GetAddressOf())))
            throw std::runtime_error("ShadowMap: CreateShaderResourceView failed");
    }

    // ─── Compute light ViewProj per cascade (teacher's AABB approach) ─────────
    //
    // Algorithm (verbatim from teacher):
    //   1. GetFrustumCornersWorldSpace for [nearZ..farZ]
    //   2. Compute centroid → CreateLookAt from centroid + lightDir
    //   3. Transform corners into light space, find AABB (minX..maxZ)
    //   4. Expand near/far by zMult=10 to catch off-screen casters
    //   5. CreateOrthographicOffCenter(minX, maxX, minY, maxY, minZ, maxZ)
    //
    // Stability (shadow swimming fix):
    //   After building lightView, snap the projected centroid to a texel
    //   boundary so the shadow map only shifts by whole texels when camera moves.
    void UpdateCascades(
        DirectX::XMVECTOR  lightDir,
        DirectX::XMMATRIX  cameraView,
        DirectX::XMMATRIX  cameraProj,
        float              cameraNear)
    {
        using namespace DirectX;

        // Build sub-frustum projection for each cascade slice.
        // We need a projection that covers exactly [nearZ..farZ] to pass
        // to GetFrustumCornersWorldSpace. Re-use the camera FOV/aspect stored
        // inside cameraProj: just replace the near/far rows.
        //
        // For XMMatrixPerspectiveFovLH (row-major):
        //   proj.r[2].z = farZ  / (farZ - nearZ)          (_33)
        //   proj.r[3].z = -nearZ*farZ / (farZ - nearZ)    (_43)
        // We rebuild these two rows for each slice.

        float prevSplit = cameraNear;

        // Up vector for light LookAt — fixed per frame, independent of camera.
        XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        if (fabsf(XMVectorGetY(XMVector3Normalize(lightDir))) > 0.99f)
            up = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

        for (int i = 0; i < kCascadeCount; ++i)
        {
            float nearZ = prevSplit;
            float farZ = splitDepths[i];
            prevSplit = farZ;

            // ── 1. Build a sub-frustum projection for [nearZ..farZ] ───────────────
            // Copy camera proj, replace only the depth rows.
            XMMATRIX sliceProj = cameraProj;
            sliceProj.r[2] = XMVectorSetZ(sliceProj.r[2],
                farZ / (farZ - nearZ));
            sliceProj.r[3] = XMVectorSetZ(sliceProj.r[3],
                -nearZ * farZ / (farZ - nearZ));

            // ── 2. GetFrustumCornersWorldSpace ────────────────────────────────────
            std::vector<XMFLOAT4> corners =
                GetFrustumCornersWorldSpace(cameraView, sliceProj);

            // ── 3. Centroid → light LookAt (teacher's approach) ───────────────────
            XMVECTOR center = XMVectorZero();
            for (const auto& v : corners)
                center += XMVectorSet(v.x, v.y, v.z, 0.0f);
            center /= XMVectorReplicate(static_cast<float>(corners.size()));

            XMMATRIX lightView = XMMatrixLookAtLH(
                center + lightDir,   // eye: one unit along lightDir from centre
                center,              // look at: centroid of the frustum slice
                up);

            // ── 4. AABB in light space (teacher's code verbatim) ──────────────────
            float minX = FLT_MAX, maxX = -FLT_MAX;
            float minY = FLT_MAX, maxY = -FLT_MAX;
            float minZ = FLT_MAX, maxZ = -FLT_MAX;

            for (const auto& v : corners)
            {
                XMVECTOR trf = XMVector4Transform(
                    XMVectorSet(v.x, v.y, v.z, 1.0f), lightView);

                float x = XMVectorGetX(trf);
                float y = XMVectorGetY(trf);
                float z = XMVectorGetZ(trf);

                if (x < minX) minX = x;  if (x > maxX) maxX = x;
                if (y < minY) minY = y;  if (y > maxY) maxY = y;
                if (z < minZ) minZ = z;  if (z > maxZ) maxZ = z;
            }

            // ── 5. Expand near/far by zMult to catch off-screen shadow casters ────
            //
            // Teacher uses zMult = 10: this extends the light frustum well beyond
            // the visible frustum so objects outside the camera view still cast
            // shadows into it.
            constexpr float zMult = 10.0f;
            minZ = (minZ < 0.0f) ? minZ * zMult : minZ / zMult;
            maxZ = (maxZ < 0.0f) ? maxZ / zMult : maxZ * zMult;

            // ── 6. Texel-snap the AABB origin to eliminate shadow swimming ─────────
            //
            // The AABB (minX..maxX, minY..maxY) may shift by a sub-texel amount each
            // frame as the camera moves, causing the shadow border to shimmer.
            //
            // Fix: compute what 1 texel is in world units for X and Y, then round
            // the AABB bounds to the nearest texel boundary.
            //
            //   worldTexelX = (maxX - minX) / kShadowMapSize
            //   worldTexelY = (maxY - minY) / kShadowMapSize
            //
            // Snap minX so it is always a multiple of worldTexelX, same for Y.
            // maxX/Y follow by adding the (now constant) total width/height.
            float width = maxX - minX;
            float height = maxY - minY;
            float texelX = width / static_cast<float>(kShadowMapSize);
            float texelY = height / static_cast<float>(kShadowMapSize);

            minX = floorf(minX / texelX) * texelX;
            minY = floorf(minY / texelY) * texelY;
            maxX = minX + width;
            maxY = minY + height;

            // ── 7. Orthographic projection from AABB ──────────────────────────────
            XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(
                minX, maxX, minY, maxY, minZ, maxZ);

            // Store transposed for HLSL row-major mul().
            XMStoreFloat4x4(&lightViewProj[i],
                XMMatrixTranspose(lightView * lightProj));
        }
    }
};