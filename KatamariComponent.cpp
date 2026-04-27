#define NOMINMAX

#include "KatamariComponent.h"
#include "Game.h"
#include "InputDevice.h"
#include "ShadowMap.h"
#include <stdexcept>
#include <iostream>
#include <algorithm>
#include <cmath>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

//for clamp work
template<typename T>
static T Clamp(T val, T lo, T hi) {
    return (val < lo) ? lo : (val > hi) ? hi : val;
}

KatamariComponent::KatamariComponent(
    Game* owner,
    std::vector<ObjectDesc> descs,
    std::wstring ballTex,
    std::wstring floorTex,
    std::wstring shaderPath,
    float sceneRad)
    : GameComponent(owner),
    objectDescs(std::move(descs)), 
    ballTexPath(std::move(ballTex)),
    floorTexPath(std::move(floorTex)),
    shaderPath(std::move(shaderPath)),
    sceneRadius(sceneRad)
{
    XMStoreFloat4x4(&ballOrientMtx, XMMatrixIdentity());
}

void KatamariComponent::Initialize()
{
    CompileShaders();
    CreateInputLayout();
    CreateConstantBuffer();
    CreateRasterizerState();
    CreateDepthStencilState();
    CreateBlendState();
    CreateSamplerState();

    CompileShadowShader();
    CreateShadowConstantBuffer();
    CreateShadowCascadeBuffer();
    CreateShadowRasterizerState();
    CreateShadowSamplerState();
    shadow.Create(game->Device.Get());

    BuildBallMesh();
    BuildObjectMeshes();
    BuildFloorMesh();
    ScatterObjects();

    camera.Pitch = 0.4f;
    camera.Distance = ballRadius * 10.0f;
    camera.AspectRatio = static_cast<float>(game->Display->ClientWidth) /
        static_cast<float>(game->Display->ClientHeight);
}

void KatamariComponent::CompileShaders()
{
    ComPtr<ID3DBlob> errors;
    UINT flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;

    HRESULT hr = D3DCompileFromFile(
        shaderPath.c_str(), nullptr, nullptr,
        "VSMain", "vs_5_0", flags, 0,
        vsBytecode.GetAddressOf(), errors.GetAddressOf());
    if (FAILED(hr))
    {
        if (errors) std::cerr << "[VS] " << (char*)errors->GetBufferPointer() << '\n';
        throw std::runtime_error("Katamari: vertex shader compilation failed.");
    }
    hr = game->Device->CreateVertexShader(
        vsBytecode->GetBufferPointer(), vsBytecode->GetBufferSize(),
        nullptr, vertexShader.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("CreateVertexShader failed.");

    ComPtr<ID3DBlob> psBytecode;
    hr = D3DCompileFromFile(
        shaderPath.c_str(), nullptr, nullptr,
        "PSMain", "ps_5_0", flags, 0,
        psBytecode.GetAddressOf(), errors.GetAddressOf());
    if (FAILED(hr))
    {
        if (errors) std::cerr << "[PS] " << (char*)errors->GetBufferPointer() << '\n';
        throw std::runtime_error("Katamari: pixel shader compilation failed.");
    }
    hr = game->Device->CreatePixelShader(
        psBytecode->GetBufferPointer(), psBytecode->GetBufferSize(),
        nullptr, pixelShader.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("CreatePixelShader failed.");
}

void KatamariComponent::CreateInputLayout()
{
    D3D11_INPUT_ELEMENT_DESC elems[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0,                            D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0,  D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    HRESULT hr = game->Device->CreateInputLayout(
        elems, static_cast<UINT>(std::size(elems)),
        vsBytecode->GetBufferPointer(), vsBytecode->GetBufferSize(),
        inputLayout.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("CreateInputLayout failed.");
}

void KatamariComponent::CreateConstantBuffer()
{
    D3D11_BUFFER_DESC desc = {};
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    desc.ByteWidth = sizeof(PerObjectCB);
    HRESULT hr = game->Device->CreateBuffer(&desc, nullptr, constantBuffer.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("CreateBuffer (CB) failed.");
}

void KatamariComponent::CreateRasterizerState()
{
    CD3D11_RASTERIZER_DESC desc(D3D11_DEFAULT);
    desc.CullMode = D3D11_CULL_BACK;
    desc.FillMode = D3D11_FILL_SOLID;
    desc.FrontCounterClockwise = FALSE;
    desc.DepthClipEnable = TRUE;
    HRESULT hr = game->Device->CreateRasterizerState(&desc, rastState.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("CreateRasterizerState failed.");
}

void KatamariComponent::CreateDepthStencilState()
{
    D3D11_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable = TRUE;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsDesc.DepthFunc = D3D11_COMPARISON_LESS;

    dsDesc.StencilEnable = FALSE;

    HRESULT hr = game->Device->CreateDepthStencilState(&dsDesc, depthState.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Failed to create DepthStencilState");
}

void KatamariComponent::CreateBlendState()
{
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable = FALSE;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    HRESULT hr = game->Device->CreateBlendState(&blendDesc, blendState.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Failed to create BlendState");
}

void KatamariComponent::CreateSamplerState()
{
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP; //Tile texture
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

    HRESULT hr = game->Device->CreateSamplerState(&sampDesc, samplerState.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Failed to create SamplerState");
}

void KatamariComponent::BuildBallMesh()
{
    MeshData md = MeshGenerator::CreateSphere(ballRadius, ballStacks, ballSlices);
    ballMesh = std::make_shared<Mesh>();
    ballMesh->Upload(game, md);
    ballMesh->LoadTexture(game->Device.Get(), ballTexPath);
}

void KatamariComponent::BuildObjectMeshes()
{
    meshPool.reserve(objectDescs.size());
    for (const auto& desc : objectDescs)
    {
        MeshData md = ObjLoader::Load(desc.objPath);
        auto mesh = std::make_shared<Mesh>();
        mesh->Upload(game, md);
        mesh->LoadTexture(game->Device.Get(), desc.texPath);
        meshPool.push_back(std::move(mesh));
    }
}

void KatamariComponent::BuildFloorMesh()
{
    float S = sceneRadius;
    int gridResolution = 100;
    float step = (2.0f * S) / gridResolution;

    MeshData md;

    float textureRepeat = 5.f;

    //vertexes generation
    for (int i = 0; i <= gridResolution; ++i) {
        for (int j = 0; j <= gridResolution; ++j) {
            float x = -S + j * step;
            float z = -S + i * step;
            float y = GetTerrainHeight(x, z);

            MeshVertex v;
            v.Position = { x, y, z };
            v.UV = { (float)j / gridResolution * textureRepeat, (float)i / gridResolution * textureRepeat };

            //normal calculation for lighting
            float eps = 0.1f;
            float hX = GetTerrainHeight(x + eps, z) - GetTerrainHeight(x - eps, z);
            float hZ = GetTerrainHeight(x, z + eps) - GetTerrainHeight(x, z - eps);
            XMVECTOR normal = XMVector3Normalize(XMVectorSet(-hX, 2.0f * eps, -hZ, 0.0f));
            XMStoreFloat3(&v.Normal, normal);

            md.Vertices.push_back(v);
        }
    }

    //indexes generation
    for (int i = 0; i < gridResolution; ++i) {
        for (int j = 0; j < gridResolution; ++j) {
            int stride = gridResolution + 1;
            int v0 = i * stride + j;
            int v1 = i * stride + (j + 1);
            int v2 = (i + 1) * stride + j;
            int v3 = (i + 1) * stride + (j + 1);

            md.Indices.push_back(v0); md.Indices.push_back(v2); md.Indices.push_back(v1);
            md.Indices.push_back(v1); md.Indices.push_back(v2); md.Indices.push_back(v3);
        }
    }

    floorMesh = std::make_shared<Mesh>();
    floorMesh->Upload(game, md);
    floorMesh->LoadTexture(game->Device.Get(), floorTexPath);
}

void KatamariComponent::ScatterObjects()
{
    static const XMFLOAT4 kPalette[] =
    {
        XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f)
        //XMFLOAT4(1.0f, 0.35f, 0.35f, 1.0f),
        //XMFLOAT4(0.35f, 1.0f, 0.45f, 1.0f),
        //XMFLOAT4(0.35f, 0.55f, 1.0f, 1.0f),
        //XMFLOAT4(1.0f, 0.90f, 0.20f, 1.0f),
        //XMFLOAT4(1.0f, 0.50f, 0.0f, 1.0f),
        //XMFLOAT4(0.80f, 0.20f, 1.0f, 1.0f),
    };
    const int pal = static_cast<int>(std::size(kPalette));

    int ci = 0;
    for (size_t di = 0; di < objectDescs.size(); ++di)
    {
        const auto& desc = objectDescs[di];
        const auto& mesh = meshPool[di];

        for (int i = 0; i < desc.count; ++i)
        {
            SceneObject obj;
            obj.mesh = mesh;
            obj.scale = RandomFloat(desc.minScale, desc.maxScale);
            obj.worldRadius = obj.scale * mesh->BoundingRadius();
            obj.color = kPalette[ci++ % pal];

            obj.material = desc.material;

            float r = RandomFloat(spawnMinDist, sceneRadius - obj.worldRadius);
            float angle = RandomFloat(0.0f, XM_2PI);

            float x = r * std::sin(angle);
            float z = r * std::cos(angle);
            float y = GetTerrainHeight(x, z);
            XMVECTOR terrainNormal = GetTerrainNormal(x, z);

            obj.position = XMFLOAT3(x, y + obj.worldRadius + desc.yOffset, z);
            XMVECTOR finalQuat;
            XMVECTOR localUp = XMVectorSet(0, 1, 0, 0);

            if (desc.placement == PlacementType::Upright || desc.placement == PlacementType::Flat)
            {
                XMVECTOR localUp = XMVectorSet(0, 1, 0, 0);

                //rotation to terrain
                XMVECTOR terrainQuat = GetRotationBetweenVectors(localUp, terrainNormal);

                float randomYaw = RandomFloat(0, XM_2PI);
                XMVECTOR yawQuat = XMQuaternionRotationAxis(localUp, randomYaw);

                finalQuat = XMQuaternionMultiply(yawQuat, terrainQuat);
            }
            else {
                //random for others
                finalQuat = XMQuaternionRotationRollPitchYaw(RandomFloat(0, XM_2PI), RandomFloat(0, XM_2PI), RandomFloat(0, XM_2PI));
            }

            XMStoreFloat4x4(&obj.localRotation, XMMatrixRotationQuaternion(finalQuat));
            objects.push_back(std::move(obj));
        }
    }
}

void KatamariComponent::Update(float dt)
{
    UpdateBallPhysics(dt);
    CheckAbsorption();

    camera.Yaw = cameraYaw;
    camera.Distance = ballRadius * 8.0f;
    camera.AspectRatio = static_cast<float>(game->Display->ClientWidth) /
        static_cast<float>(game->Display->ClientHeight);
}

void KatamariComponent::UpdateBallPhysics(float dt)
{
    auto* input = game->Input.get();
    const float speed = ballSpeed + ballRadius * 0.3f;

    XMVECTOR forward = XMVectorSet(-std::sin(cameraYaw), 0.0f, -std::cos(cameraYaw), 0.0f);
    XMVECTOR right = XMVectorSet(-std::cos(cameraYaw), 0.0f, std::sin(cameraYaw), 0.0f);

    XMVECTOR move = XMVectorZero();
    if (input->IsKeyDown('W') || input->IsKeyDown(VK_UP))    move = XMVectorAdd(move, forward);
    if (input->IsKeyDown('S') || input->IsKeyDown(VK_DOWN))  move = XMVectorAdd(move, -forward);
    if (input->IsKeyDown('D') || input->IsKeyDown(VK_RIGHT)) move = XMVectorAdd(move, right);
    if (input->IsKeyDown('A') || input->IsKeyDown(VK_LEFT))  move = XMVectorAdd(move, -right);

    if (input->IsKeyDown('Q')) cameraYaw -= 1.5f * dt;
    if (input->IsKeyDown('E')) cameraYaw += 1.5f * dt;

    XMVECTOR posV = XMLoadFloat3(&ballPos);
    float moveLen = XMVectorGetX(XMVector3Length(move));
    if (moveLen > 0.001f)
    {
        XMVECTOR moveDelta = XMVectorScale(move, speed * dt / moveLen);
        posV = XMVectorAdd(posV, moveDelta);

        //Visual rotaiton
        XMVECTOR up = XMVectorSet(0, 1, 0, 0);
        XMVECTOR rollAxis = XMVector3Normalize(XMVector3Cross(up, move));
        float angle = (speed * dt) / ballRadius;
        XMVECTOR q = XMQuaternionRotationAxis(rollAxis, angle);
        XMMATRIX rot = XMMatrixRotationQuaternion(q);
        XMMATRIX prev = XMLoadFloat4x4(&ballOrientMtx);
        XMStoreFloat4x4(&ballOrientMtx, prev * rot);
    }

    //border moving 
    float px = XMVectorGetX(posV);
    float pz = XMVectorGetZ(posV);
    float lim = sceneRadius - ballRadius;
  
    px = Clamp(px, -lim, lim);
    pz = Clamp(pz, -lim, lim);
    float py = GetTerrainHeight(px, pz) + ballRadius;

    ballPos = XMFLOAT3(px, py, pz);
}

void KatamariComponent::CheckAbsorption()
{
    for (auto& obj : objects)
    {
        if (obj.absorbed) continue;

		//Check object is smaller than ball
        if (obj.worldRadius >= ballRadius) continue;

        float dx = obj.position.x - ballPos.x;
        float dy = obj.position.y - ballPos.y;
        float dz = obj.position.z - ballPos.z;
        float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

        if (dist >= ballRadius + obj.worldRadius * 0.8f) continue;

        obj.absorbed = true;
        ++absorbedCount;

        XMMATRIX ballOrient = XMLoadFloat4x4(&ballOrientMtx);
        XMMATRIX ballOrientInv = XMMatrixInverse(nullptr, ballOrient);

        XMVECTOR dirW = (dist > 0.0001f)
            ? XMVectorSet(dx / dist, dy / dist, dz / dist, 0.0f)
            : XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

        XMVECTOR dirL = XMVector3TransformNormal(dirW, ballOrientInv);

        XMVECTOR normalizedDirectionL = XMVector3Normalize(dirL);
        XMStoreFloat3(&obj.localOffset, normalizedDirectionL);

        XMMATRIX objRot = XMLoadFloat4x4(&obj.localRotation);
        XMMATRIX localRot = objRot * ballOrientInv;
        XMStoreFloat4x4(&obj.localRotation, localRot);

        //ball radius increase
        float rb3 = ballRadius * ballRadius * ballRadius;
        float ro3 = obj.worldRadius * obj.worldRadius * obj.worldRadius;
        ballRadius = std::cbrt(rb3 + ro3 * 0.3f);

        std::cout << "[Katamari] Absorbed #" << absorbedCount
            << " ballRadius = " << ballRadius << '\n';
    }
}

void KatamariComponent::Draw()
{
    auto* ctx = game->Context.Get();

    // ── Update cascade matrices (needs current camera state) ─────────────────
    {
        XMVECTOR lightDir = XMLoadFloat4(&SunlightDirection);
        lightDir = XMVector3Normalize(lightDir);
        XMMATRIX view = camera.GetView(ballPos);
        XMMATRIX proj = camera.GetProjection();
        shadow.UpdateCascades(lightDir, view, proj, camera.NearPlane);
    }

    // ── Shadow pass: render depth from sun into each cascade map ─────────────
    RenderShadowPass();

    // ── Restore main render target (shadow pass left it unbound) ─────────────
    game->RestoreTargets();

    D3D11_VIEWPORT mainVp = {};
    mainVp.Width = static_cast<float>(game->Display->ClientWidth);
    mainVp.Height = static_cast<float>(game->Display->ClientHeight);
    mainVp.MinDepth = 0.0f;
    mainVp.MaxDepth = 1.0f;
    ctx->RSSetViewports(1, &mainVp);

    // ── Bind shadow Texture2DArray SRV to slot t1 ────────────────────────────
        // One SRV covers all cascade slices (teacher's Texture2DArray approach).
    ID3D11ShaderResourceView* shadowSRV = shadow.srv.Get();
    ctx->PSSetShaderResources(1, 1, &shadowSRV);
    ctx->PSSetSamplers(1, 1, shadowSamplerState.GetAddressOf());

    ctx->OMSetDepthStencilState(depthState.Get(), 0);
    float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    ctx->OMSetBlendState(blendState.Get(), blendFactor, 0xFFFFFFFF);
    ctx->PSSetSamplers(0, 1, samplerState.GetAddressOf());

    ctx->RSSetState(rastState.Get());
    ctx->IASetInputLayout(inputLayout.Get());
    ctx->VSSetShader(vertexShader.Get(), nullptr, 0);
    ctx->PSSetShader(pixelShader.Get(), nullptr, 0);
    ctx->VSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());
    ctx->PSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());

    XMMATRIX view = camera.GetView(ballPos);
    XMMATRIX proj = camera.GetProjection();
    XMFLOAT3 camPos = camera.GetEyePosition(ballPos);

    DrawFloor(view, proj, camPos);

    for (const auto& obj : objects)
        if (!obj.absorbed)
            DrawFreeObject(obj, view, proj, camPos);

    DrawBall(view, proj, camPos);

    for (const auto& obj : objects)
        if (obj.absorbed)
            DrawStuckObject(obj, view, proj, camPos);
}

void KatamariComponent::SetConstantBuffer(
    const XMMATRIX& world, const XMMATRIX& view, const XMMATRIX& projection,
    const Material& material, const XMFLOAT3& camPos)
{
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (FAILED(game->Context->Map(constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        return;

    auto* cb = reinterpret_cast<PerObjectCB*>(mapped.pData);
    XMStoreFloat4x4(&cb->WorldMatrix, XMMatrixTranspose(world));
    XMStoreFloat4x4(&cb->ViewMatrix, XMMatrixTranspose(view));
    XMStoreFloat4x4(&cb->ProjectionMatrix, XMMatrixTranspose(projection));


    cb->SunlightDirection = SunlightDirection;
    cb->SunlightColor = SunlightColor;
    cb->CameraPosition = XMFLOAT4(camPos.x, camPos.y, camPos.z, 1.0f);

    cb->MaterialAmbientColor = material.Ambient;
    cb->MaterialDiffuseColor = material.Diffuse;
    cb->MaterialSpecularColor = material.Specular;
    cb->MaterialShininess = material.Shininess;

    // Upload CSM matrices and split depths.
    // shadow.lightViewProj[i] is already transposed (stored row-major for HLSL mul()).
    for (int i = 0; i < kCascadeCount; ++i)
        cb->LightViewProj[i] = shadow.lightViewProj[i];
    cb->CascadeSplits = XMFLOAT4(
        shadow.splitDepths[0], shadow.splitDepths[1], shadow.splitDepths[2], 0.0f);

    game->Context->Unmap(constantBuffer.Get(), 0);
}

void KatamariComponent::DrawBall(const XMMATRIX& v, const XMMATRIX& p, const XMFLOAT3& cam)
{
    SetConstantBuffer(BallWorldMatrix(), v, p, ballMaterial, cam);

    ID3D11ShaderResourceView* srv = ballMesh->GetTexture();
    game->Context->PSSetShaderResources(0, 1, &srv);

    ballMesh->Bind(game->Context.Get());
    ballMesh->Draw(game->Context.Get());
}

void KatamariComponent::DrawFreeObject(const SceneObject& obj,
    const XMMATRIX& v, const XMMATRIX& p, const XMFLOAT3& cam)
{
    SetConstantBuffer(FreeObjectWorldMatrix(obj), v, p, obj.material, cam);

    ID3D11ShaderResourceView* srv = obj.mesh->GetTexture();
    game->Context->PSSetShaderResources(0, 1, &srv);

    obj.mesh->Bind(game->Context.Get());
    obj.mesh->Draw(game->Context.Get());
}

void KatamariComponent::DrawStuckObject(const SceneObject& obj,
    const XMMATRIX& v, const XMMATRIX& p, const XMFLOAT3& cam)
{
    SetConstantBuffer(StuckObjectWorldMatrix(obj), v, p, obj.material, cam);

    ID3D11ShaderResourceView* srv = obj.mesh->GetTexture();
    game->Context->PSSetShaderResources(0, 1, &srv);

    obj.mesh->Bind(game->Context.Get());
    obj.mesh->Draw(game->Context.Get());
}

void KatamariComponent::DrawFloor(const XMMATRIX& v, const XMMATRIX& p, const XMFLOAT3& cam)
{
    SetConstantBuffer(XMMatrixIdentity(), v, p, floorMaterial, cam);

    ID3D11ShaderResourceView* srv = floorMesh->GetTexture();
    game->Context->PSSetShaderResources(0, 1, &srv);

    floorMesh->Bind(game->Context.Get());
    floorMesh->Draw(game->Context.Get());
}

XMMATRIX KatamariComponent::BallWorldMatrix() const
{
    XMMATRIX S = XMMatrixScaling(ballRadius, ballRadius, ballRadius);
    XMMATRIX orient = XMLoadFloat4x4(&ballOrientMtx);
    XMMATRIX T = XMMatrixTranslation(ballPos.x, ballPos.y, ballPos.z);
    return S * orient * T;
}

XMMATRIX KatamariComponent::FreeObjectWorldMatrix(const SceneObject& obj) const
{
    XMMATRIX S = XMMatrixScaling(obj.scale, obj.scale, obj.scale);
    XMMATRIX R = XMLoadFloat4x4(&obj.localRotation);
    XMMATRIX T = XMMatrixTranslation(obj.position.x, obj.position.y, obj.position.z);
    return S * R * T;
}

XMMATRIX KatamariComponent::StuckObjectWorldMatrix(const SceneObject& obj) const
{
    XMMATRIX S = XMMatrixScaling(obj.scale, obj.scale, obj.scale);

    XMMATRIX localRotation = XMLoadFloat4x4(&obj.localRotation);

    XMVECTOR offsetV = XMLoadFloat3(&obj.localOffset);

    float distanceToSurface = ballRadius + (obj.worldRadius * 0.5f);

    XMVECTOR finalLocalPosition = offsetV * distanceToSurface;
    XMMATRIX T_surface = XMMatrixTranslationFromVector(finalLocalPosition);

    XMMATRIX ballRotation = XMLoadFloat4x4(&ballOrientMtx);
    XMMATRIX ballTranslation = XMMatrixTranslation(ballPos.x, ballPos.y, ballPos.z);


    return S * localRotation * T_surface * ballRotation * ballTranslation;
}

// ─── Shadow: compile depth-only vertex shader ────────────────────────────────

void KatamariComponent::CompileShadowShader()
{
	//TODO: param shadowPass shader path
    std::wstring shadowShaderPath = L"shaders/ShadowPass.hlsl";
    ComPtr<ID3DBlob> blob, errors;
    UINT flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;

    // ── Vertex shader ─────────────────────────────────────────────────────────
    HRESULT hr = D3DCompileFromFile(
        shadowShaderPath.c_str(), nullptr, nullptr,
        "VShadow", "vs_5_0", flags, 0,
        blob.GetAddressOf(), errors.GetAddressOf());
    if (FAILED(hr))
    {
        if (errors) std::cerr << "[ShadowVS] " << (char*)errors->GetBufferPointer() << '\n';
        throw std::runtime_error("Shadow vertex shader compilation failed.");
    }
    hr = game->Device->CreateVertexShader(
        blob->GetBufferPointer(), blob->GetBufferSize(),
        nullptr, shadowVertexShader.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("CreateVertexShader (shadow) failed.");

    // ── Geometry shader (instanced — fills all cascade slices in one draw) ────
    ComPtr<ID3DBlob> gsBlob;
    hr = D3DCompileFromFile(
        shadowShaderPath.c_str(), nullptr, nullptr,
        "GShadow", "gs_5_0", flags, 0,
        gsBlob.GetAddressOf(), errors.GetAddressOf());
    if (FAILED(hr))
    {
        if (errors) std::cerr << "[ShadowGS] " << (char*)errors->GetBufferPointer() << '\n';
        throw std::runtime_error("Shadow geometry shader compilation failed.");
    }
    hr = game->Device->CreateGeometryShader(
        gsBlob->GetBufferPointer(), gsBlob->GetBufferSize(),
        nullptr, shadowGeometryShader.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("CreateGeometryShader (shadow) failed.");
}


// ─── Shadow: constant buffer (WorldMatrix + LightViewProj per cascade) ────────
//
// Layout must match ShadowPassCB in ShadowPass.hlsl.

struct ShadowPassCB
{
    DirectX::XMFLOAT4X4 WorldMatrix;   // only World needed; GS handles projection
};

void KatamariComponent::CreateShadowConstantBuffer()
{
    D3D11_BUFFER_DESC desc = {};
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    desc.ByteWidth = sizeof(ShadowPassCB);
    if (FAILED(game->Device->CreateBuffer(&desc, nullptr, shadowConstantBuffer.GetAddressOf())))
        throw std::runtime_error("CreateBuffer (shadow CB) failed.");
}

// b1: all cascade LightViewProj matrices + split depths (updated once per frame).
struct ShadowCascadeCB
{
    DirectX::XMFLOAT4X4 LightViewProj[kCascadeCount];
    DirectX::XMFLOAT4   CascadeSplits;
};

void KatamariComponent::CreateShadowCascadeBuffer()
{
    D3D11_BUFFER_DESC desc = {};
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    desc.ByteWidth = sizeof(ShadowCascadeCB);
    if (FAILED(game->Device->CreateBuffer(&desc, nullptr, shadowCascadeBuffer.GetAddressOf())))
        throw std::runtime_error("CreateBuffer (shadow cascade CB) failed.");
}

// ─── Shadow rasterizer: enable depth bias to fight shadow acne ───────────────
//
// Shadow acne happens because a surface compares its own depth against what was
// stored in the shadow map. Floating-point imprecision makes the surface
// slightly shadow itself.
//
// DepthBias adds a constant offset to all stored depths (shifts the shadow map
// values slightly away from the camera).
// SlopeScaledDepthBias adds extra bias proportional to the polygon slope
// relative to the light — steeper slopes need more bias.

void KatamariComponent::CreateShadowRasterizerState()
{
    CD3D11_RASTERIZER_DESC desc(D3D11_DEFAULT);
    // Teacher specifies CULL_FRONT for shadow pass.
// Culling front faces means only back faces write depth.
// This neatly avoids self-shadowing (shadow acne) on front surfaces
// because the depth stored is that of the *back* face,
// which is always deeper than the front face being lit.
    desc.CullMode = D3D11_CULL_FRONT;
    desc.DepthBias = 5000;     // constant bias in depth units
    desc.SlopeScaledDepthBias = 2.0f;    // slope-proportional bias
    desc.DepthBiasClamp = 0.01f;   // cap to avoid over-biasing near silhouettes
    if (FAILED(game->Device->CreateRasterizerState(&desc, shadowRastState.GetAddressOf())))
        throw std::runtime_error("CreateRasterizerState (shadow) failed.");
}

// ─── Shadow sampler: point clamp for manual PCF in the shader ────────────────
//
// We use a plain point sampler (no hardware PCF) so we have full control over
// the kernel in HLSL. Clamp addressing prevents wrapping at shadow map edges.

void KatamariComponent::CreateShadowSamplerState()
{
    D3D11_SAMPLER_DESC desc = {};
    desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.MinLOD = 0;
    desc.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(game->Device->CreateSamplerState(&desc, shadowSamplerState.GetAddressOf())))
        throw std::runtime_error("CreateSamplerState (shadow) failed.");
}

// ─── Shadow pass: render entire scene into each cascade depth map ─────────────
//
// Pipeline state for the shadow pass:
//   - No render target (nullptr) — only depth stencil is bound.
//   - Shadow rasterizer state (with depth bias).
//   - Shadow vertex shader only — no pixel shader needed.
//   - Shadow constant buffer at slot 0.

void KatamariComponent::RenderShadowPass()
{
    auto* ctx = game->Context.Get();

    // ── Upload cascade buffer (b1): all LightViewProj + split depths ──────────
    // Done once per frame before drawing — GS reads it for all instances.
    {
        D3D11_MAPPED_SUBRESOURCE mapped;
        if (SUCCEEDED(ctx->Map(shadowCascadeBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        {
            auto* cb = reinterpret_cast<ShadowCascadeCB*>(mapped.pData);
            for (int i = 0; i < kCascadeCount; ++i)
                cb->LightViewProj[i] = shadow.lightViewProj[i];
            cb->CascadeSplits = XMFLOAT4(
                shadow.splitDepths[0], shadow.splitDepths[1], shadow.splitDepths[2], 0.0f);
            ctx->Unmap(shadowCascadeBuffer.Get(), 0);
        }
    }

    // ── Pipeline state for the shadow pass ────────────────────────────────────
    D3D11_VIEWPORT vp = {};
    vp.Width = static_cast<float>(kShadowMapSize);
    vp.Height = static_cast<float>(kShadowMapSize);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    ctx->RSSetViewports(1, &vp);

    ctx->RSSetState(shadowRastState.Get());
    ctx->IASetInputLayout(inputLayout.Get());

    ctx->VSSetShader(shadowVertexShader.Get(), nullptr, 0);
    ctx->GSSetShader(shadowGeometryShader.Get(), nullptr, 0);   // instanced GS
    ctx->PSSetShader(nullptr, nullptr, 0);                      // depth only

    // b0 = per-draw World matrix, b1 = cascade data for GS
    ctx->VSSetConstantBuffers(0, 1, shadowConstantBuffer.GetAddressOf());
    ctx->GSSetConstantBuffers(1, 1, shadowCascadeBuffer.GetAddressOf());

    // ── Clear the entire array DSV, bind it once ──────────────────────────────
    // Teacher uses one DSV covering all array slices — the GS routes triangles
    // into the correct slice via SV_RenderTargetArrayIndex.
    ctx->ClearDepthStencilView(shadow.dsv.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
    ID3D11RenderTargetView* nullRTV = nullptr;
    ctx->OMSetRenderTargets(0, &nullRTV, shadow.dsv.Get());

    // ── One draw call fills all cascade slices (GS instancing does the rest) ──
    DrawSceneForShadow();

    // Unbind DSV before using the SRV in the lighting pass.
    ctx->OMSetRenderTargets(0, nullptr, nullptr);
    ctx->GSSetShader(nullptr, nullptr, 0);   // clear GS for main pass
}

// ─── Draw all visible geometry for the shadow pass ────────────────────────────
//
// We only need position → just upload World + LightViewProj and draw.

// DrawSceneForShadow — no cascade parameter needed.
// The GS reads all LightViewProj from shadowCascadeBuffer (b1)
// and routes each triangle instance to the correct array slice.
void KatamariComponent::DrawSceneForShadow()
{
    auto* ctx = game->Context.Get();

    // Helper: upload World matrix to b0 and draw.
    auto drawMesh = [&](const XMMATRIX& worldMtx, Mesh* mesh)
        {
            D3D11_MAPPED_SUBRESOURCE mapped;
            if (FAILED(ctx->Map(shadowConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
                return;

            ShadowPassCB* cb = reinterpret_cast<ShadowPassCB*>(mapped.pData);
            XMStoreFloat4x4(&cb->WorldMatrix, XMMatrixTranspose(worldMtx));
            ctx->Unmap(shadowConstantBuffer.Get(), 0);

            mesh->Bind(ctx);
            mesh->Draw(ctx);
        };

    drawMesh(BallWorldMatrix(), ballMesh.get());
    drawMesh(XMMatrixIdentity(), floorMesh.get());

    for (const auto& obj : objects)
    {
        XMMATRIX world = obj.absorbed
            ? StuckObjectWorldMatrix(obj)
            : FreeObjectWorldMatrix(obj);
        drawMesh(world, obj.mesh.get());
    }
}
void KatamariComponent::DestroyResources()
{
    shadowRastState.Reset();
    shadowCascadeBuffer.Reset();
    shadowConstantBuffer.Reset();
    shadowGeometryShader.Reset();
    shadowVertexShader.Reset();
    shadowSamplerState.Reset();
    rastState.Reset();
    depthState.Reset();
    blendState.Reset();
    constantBuffer.Reset();
    inputLayout.Reset();
    pixelShader.Reset();
    vertexShader.Reset();
    vsBytecode.Reset();
    objects.clear();
    meshPool.clear();
    ballMesh.reset();
    floorMesh.reset();
}

float KatamariComponent::RandomFloat(float lo, float hi)
{
    return lo + std::uniform_real_distribution<float>(0.0f, 1.0f)(rng) * (hi - lo);
}

float KatamariComponent::GetTerrainHeight(float x, float z) const
{
    float amplitude = 2.0f;
    float frequency = 0.2f;
    return std::sin(x * frequency) * std::cos(z * frequency) * amplitude;
}

XMVECTOR KatamariComponent::GetTerrainNormal(float x, float z) const
{
    float eps = 0.05f; //offset
    //take 2 points near by x and z
    float hL = GetTerrainHeight(x - eps, z);
    float hR = GetTerrainHeight(x + eps, z);
    float hD = GetTerrainHeight(x, z - eps);
    float hU = GetTerrainHeight(x, z + eps);

    XMVECTOR normal = XMVectorSet(hL - hR, 2.0f * eps, hD - hU, 0.0f);
    return XMVector3Normalize(normal);
}

XMVECTOR KatamariComponent::GetRotationBetweenVectors(XMVECTOR from, XMVECTOR to)
{
    from = XMVector3Normalize(from);
    to = XMVector3Normalize(to);

    float dot = XMVectorGetX(XMVector3Dot(from, to));

    //dont need rotation if vectors near
    if (dot > 0.9999f) return XMQuaternionIdentity();

    //different direction
    if (dot < -0.9999f)
    {
        //rotate to each other
        return XMQuaternionRotationAxis(XMVectorSet(1, 0, 0, 0), XM_PI);
    }

    //axis is vector cross 
    XMVECTOR axis = XMVector3Normalize(XMVector3Cross(from, to));
    //angle of rotation
    float angle = acosf(dot);

    return XMQuaternionRotationAxis(axis, angle);
}