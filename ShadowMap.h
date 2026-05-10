#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <array>
#include <stdexcept>

const int kCascadeCount = 3;

const int kShadowMapSize = 2048;

//iterate x,y,z in {0,1} to enumerate all 8 corners
//return 8 worldspace float4 vectors (w=1 after divide)
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
                XMVECTOR pt = XMVectorSet(
                    2.0f * x - 1.0f,
                    2.0f * y - 1.0f,
                    static_cast<float>(z),
                    1.0f);

                XMVECTOR world = XMVector4Transform(pt, inv);
                XMFLOAT4 w;
                XMStoreFloat4(&w, world / XMVectorSplatW(world));   //perspective divide
                corners.push_back(w);
            }

    return corners;
}

struct ShadowData
{
    //one texture array holds all cascade depth maps as slices
    Microsoft::WRL::ComPtr<ID3D11Texture2D> shadowTexArray;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> dsv; //all slices at once
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv; //texture2DArray SRV

    //light ViewProj matrices for every cascade
    std::array<DirectX::XMFLOAT4X4, kCascadeCount> lightViewProj;

    //cascade split deths
    std::array<float, kCascadeCount> splitDepths = { 12.0f, 17.0f, 90.0f };

    void Create(ID3D11Device* device)
    {
        D3D11_TEXTURE2D_DESC td = {};
        td.Width = kShadowMapSize;
        td.Height = kShadowMapSize;
        td.MipLevels = 1;
        td.ArraySize = kCascadeCount;
        td.Format = DXGI_FORMAT_R32_TYPELESS;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

        if (FAILED(device->CreateTexture2D(&td, nullptr, shadowTexArray.GetAddressOf())))
            throw std::runtime_error("ShadowMap: CreateTexture2D (array) failed");

        D3D11_DEPTH_STENCIL_VIEW_DESC dsvd = {};
        dsvd.Format = DXGI_FORMAT_D32_FLOAT;
        dsvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
        dsvd.Texture2DArray.MipSlice = 0;
        dsvd.Texture2DArray.FirstArraySlice = 0;
        dsvd.Texture2DArray.ArraySize = kCascadeCount;

        if (FAILED(device->CreateDepthStencilView(shadowTexArray.Get(), &dsvd, dsv.GetAddressOf())))
            throw std::runtime_error("ShadowMap: CreateDepthStencilView failed");

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

    //compute light ViewProj per cascade:
    //1 GetFrustumCornersWorldSpace for [nearZ..farZ]
    //2 compute centroid than CreateLookAt from centroid + lightDir
    //3 transform corners into light space and find AABB (minX..maxZ)
    //4 expand near/far by zMult=10 to catch off-screen casters
    //5 CreateOrthographicOffCenter(minX, maxX, minY, maxY, minZ, maxZ)
    void UpdateCascades(
        DirectX::XMVECTOR lightDir,
        DirectX::XMMATRIX cameraView,
        DirectX::XMMATRIX cameraProj,
        float cameraNear)
    {
        using namespace DirectX;

        float prevSplit = cameraNear;

        XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        if (fabsf(XMVectorGetY(XMVector3Normalize(lightDir))) > 0.99f)
        {
            up = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
        }

        for (int i = 0; i < kCascadeCount; ++i)
        {
            float nearZ = prevSplit;
            float farZ = splitDepths[i];
            prevSplit = farZ;

            //Build a sub frustum projection for [nearZ..farZ]
            //copy camera proj, replace only the depth rows
            XMMATRIX sliceProj = cameraProj;
            sliceProj.r[2] = XMVectorSetZ(sliceProj.r[2], farZ / (farZ - nearZ));
            sliceProj.r[3] = XMVectorSetZ(sliceProj.r[3], -nearZ * farZ / (farZ - nearZ));

            std::vector<XMFLOAT4> corners = GetFrustumCornersWorldSpace(cameraView, sliceProj);

            XMVECTOR center = XMVectorZero();
            for (const auto& v : corners)
            {
                center += XMVectorSet(v.x, v.y, v.z, 0.0f);
            }
            center /= XMVectorReplicate(static_cast<float>(corners.size()));

            XMMATRIX lightView = XMMatrixLookAtLH(center + lightDir, center, up);

            float minX = FLT_MAX, maxX = -FLT_MAX;
            float minY = FLT_MAX, maxY = -FLT_MAX;
            float minZ = FLT_MAX, maxZ = -FLT_MAX;

            for (const auto& v : corners)
            {
                XMVECTOR trf = XMVector4Transform(XMVectorSet(v.x, v.y, v.z, 1.0f), lightView);

                float x = XMVectorGetX(trf);
                float y = XMVectorGetY(trf);
                float z = XMVectorGetZ(trf);

                if (x < minX) minX = x;  if (x > maxX) maxX = x;
                if (y < minY) minY = y;  if (y > maxY) maxY = y;
                if (z < minZ) minZ = z;  if (z > maxZ) maxZ = z;
            }

            //expand near/far by zMult to catch offscreen shadow casters
            constexpr float zMult = 10.0f;
            minZ = (minZ < 0.0f) ? minZ * zMult : minZ / zMult;
            maxZ = (maxZ < 0.0f) ? maxZ / zMult : maxZ * zMult;

			//texel snap the AABB to stabilize shimmering
            float width = maxX - minX;
            float height = maxY - minY;
            float texelX = width / static_cast<float>(kShadowMapSize);
            float texelY = height / static_cast<float>(kShadowMapSize);

            minX = floorf(minX / texelX) * texelX;
            minY = floorf(minY / texelY) * texelY;
            maxX = minX + width;
            maxY = minY + height;

            //orthographic projection from AABB
            XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(
                minX, maxX, minY, maxY, minZ, maxZ);

            //store transposed for HLSL row-major mul()
            XMStoreFloat4x4(&lightViewProj[i], XMMatrixTranspose(lightView * lightProj));
        }
    }
};