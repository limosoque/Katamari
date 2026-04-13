#define NOMINMAX

#include "KatamariComponent.h"
#include "Game.h"
#include "InputDevice.h"
#include <stdexcept>
#include <iostream>
#include <algorithm>
#include <cmath>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

// ─── Constructor ─────────────────────────────────────────────────────────────

KatamariComponent::KatamariComponent(
    Game* owner,
    std::vector<ObjectDesc> descs,
    std::wstring            shaderPath,
    float                   sceneRad)
    : GameComponent(owner)
    , objectDescs(std::move(descs))
    , shaderPath(std::move(shaderPath))
    , sceneRadius(sceneRad)
{
    XMStoreFloat4x4(&ballOrientMtx, XMMatrixIdentity());
}

// ─── Initialize ──────────────────────────────────────────────────────────────

void KatamariComponent::Initialize()
{
    CompileShaders();
    CreateInputLayout();
    CreateConstantBuffer();
    CreateRasterizerState();

    BuildBallMesh();
    BuildObjectMeshes();
    BuildFloorMesh();
    ScatterObjects();

    camera.Pitch = 0.4f;
    camera.Distance = ballRadius * 10.0f;
    camera.AspectRatio = static_cast<float>(game->Display->ClientWidth) /
        static_cast<float>(game->Display->ClientHeight);
}

// ─── Shader compilation (from file) ──────────────────────────────────────────

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

// ─── Input layout ────────────────────────────────────────────────────────────

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

// ─── Constant buffer ─────────────────────────────────────────────────────────

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

// ─── Rasterizer ──────────────────────────────────────────────────────────────

void KatamariComponent::CreateRasterizerState()
{
    CD3D11_RASTERIZER_DESC desc(D3D11_DEFAULT);
    desc.CullMode = D3D11_CULL_BACK;
    desc.FillMode = D3D11_FILL_SOLID;
    HRESULT hr = game->Device->CreateRasterizerState(&desc, rastState.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("CreateRasterizerState failed.");
}

// ─── Mesh builders ───────────────────────────────────────────────────────────

void KatamariComponent::BuildBallMesh()
{
    MeshData md = MeshGenerator::CreateSphere(1.0f, 32, 32);
    ballMesh = std::make_shared<Mesh>();
    ballMesh->Upload(game, md);
    ballRadius = 1.0f;
}

void KatamariComponent::BuildObjectMeshes()
{
    meshPool.reserve(objectDescs.size());
    for (const auto& desc : objectDescs)
    {
        MeshData md = ObjLoader::Load(desc.objPath);
        auto mesh = std::make_shared<Mesh>();
        mesh->Upload(game, md);
        meshPool.push_back(std::move(mesh));
    }
}

void KatamariComponent::BuildFloorMesh()
{
    float S = sceneRadius;
    MeshData md;
    md.Vertices =
    {
        { XMFLOAT3(-S, 0,-S), XMFLOAT3(0,1,0), XMFLOAT2(0,0) },
        { XMFLOAT3(S, 0,-S), XMFLOAT3(0,1,0), XMFLOAT2(1,0) },
        { XMFLOAT3(S, 0, S), XMFLOAT3(0,1,0), XMFLOAT2(1,1) },
        { XMFLOAT3(-S, 0, S), XMFLOAT3(0,1,0), XMFLOAT2(0,1) },
    };
    md.Indices = { 0, 2, 1,  0, 3, 2 };
    md.BoundingRadius = S * 1.414f;
    floorMesh = std::make_shared<Mesh>();
    floorMesh->Upload(game, md);
}

// ─── Scatter ─────────────────────────────────────────────────────────────────

void KatamariComponent::ScatterObjects()
{
    static const XMFLOAT4 kPalette[] =
    {
        XMFLOAT4(1.0f, 0.35f, 0.35f, 1.0f),
        XMFLOAT4(0.35f, 1.0f, 0.45f, 1.0f),
        XMFLOAT4(0.35f, 0.55f, 1.0f,  1.0f),
        XMFLOAT4(1.0f, 0.90f, 0.20f,  1.0f),
        XMFLOAT4(1.0f, 0.50f, 0.0f,   1.0f),
        XMFLOAT4(0.80f, 0.20f, 1.0f,  1.0f),
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

            float minDist = 6.0f;
            float r, angle;
            do {
                r = RandomFloat(minDist, sceneRadius - obj.worldRadius);
                angle = RandomFloat(0.0f, XM_2PI);
            } while (r < minDist);

            obj.position = XMFLOAT3(r * std::sin(angle), obj.worldRadius, r * std::cos(angle));
            obj.rotation = XMFLOAT3(RandomFloat(0, XM_2PI), RandomFloat(0, XM_2PI), RandomFloat(0, XM_2PI));

            XMStoreFloat4x4(&obj.localRotation, XMMatrixIdentity());
            objects.push_back(std::move(obj));
        }
    }
}

// ─── Update ──────────────────────────────────────────────────────────────────

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
    const float speed = 7.0f + ballRadius * 0.3f;

    // Вектор Forward: куда смотрит камера в плоскости XZ
    XMVECTOR forward = XMVectorSet(-std::sin(cameraYaw), 0.0f, -std::cos(cameraYaw), 0.0f);
    // ИСПРАВЛЕНИЕ: Вектор Right должен быть перпендикулярен Forward. 
    // Для инверсии движения влево-вправо меняем знаки здесь:
    XMVECTOR right = XMVectorSet(-std::cos(cameraYaw), 0.0f, std::sin(cameraYaw), 0.0f);

    XMVECTOR move = XMVectorZero();
    if (input->IsKeyDown('W') || input->IsKeyDown(VK_UP))    move = XMVectorAdd(move, forward);
    if (input->IsKeyDown('S') || input->IsKeyDown(VK_DOWN))  move = XMVectorAdd(move, -forward);
    if (input->IsKeyDown('D') || input->IsKeyDown(VK_RIGHT)) move = XMVectorAdd(move, right);
    if (input->IsKeyDown('A') || input->IsKeyDown(VK_LEFT))  move = XMVectorAdd(move, -right);

    if (input->IsKeyDown('Q')) cameraYaw -= 1.5f * dt;
    if (input->IsKeyDown('E')) cameraYaw += 1.5f * dt;

    float moveLen = XMVectorGetX(XMVector3Length(move));
    if (moveLen > 0.001f)
    {
        move = XMVectorScale(move, speed * dt / moveLen);

        XMVECTOR up = XMVectorSet(0, 1, 0, 0);
        XMVECTOR rollAxis = XMVector3Normalize(XMVector3Cross(up, move));
        float    angle = (speed * dt) / ballRadius;

        XMVECTOR q = XMQuaternionRotationAxis(rollAxis, angle);
        XMMATRIX rot = XMMatrixRotationQuaternion(q);
        XMMATRIX prev = XMLoadFloat4x4(&ballOrientMtx);
        XMStoreFloat4x4(&ballOrientMtx, prev * rot);

        XMVECTOR pos = XMLoadFloat3(&ballPos);
        pos = XMVectorAdd(pos, move);

        float px = XMVectorGetX(pos);
        float pz = XMVectorGetZ(pos);
        float d = std::sqrt(px * px + pz * pz);
        float lim = sceneRadius - ballRadius;
        if (d > lim) { float s = lim / d; px *= s; pz *= s; }
        pos = XMVectorSetX(pos, px);
        pos = XMVectorSetZ(pos, pz);
        XMStoreFloat3(&ballPos, pos);
    }

    ballPos.y = ballRadius;
}

// ─── Absorption ───────────────────────────────────────────────────────────────

void KatamariComponent::CheckAbsorption()
{
    for (auto& obj : objects)
    {
        if (obj.absorbed) continue;

        // ОБЪЕКТ ДОЛЖЕН БЫТЬ МЕНЬШЕ ШАРА, чтобы его можно было подобрать
        // Если он больше — мы просто катимся мимо (или упираемся, но тут нет коллизий стен)
        if (obj.worldRadius >= ballRadius) continue;

        float dx = obj.position.x - ballPos.x;
        float dy = obj.position.y - ballPos.y;
        float dz = obj.position.z - ballPos.z;
        float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

        // ИСПРАВЛЕНИЕ: Смягчаем условие дистанции. 
        // Объект поглощается, если расстояние меньше суммы радиусов с небольшим запасом.
        if (dist >= ballRadius + obj.worldRadius * 0.8f) continue;

        obj.absorbed = true;
        ++absorbedCount;

        XMMATRIX ballOrient = XMLoadFloat4x4(&ballOrientMtx);
        XMMATRIX ballOrientInv = XMMatrixInverse(nullptr, ballOrient);

        XMVECTOR dirW = (dist > 0.0001f)
            ? XMVectorSet(dx / dist, dy / dist, dz / dist, 0.0f)
            : XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

        XMVECTOR dirL = XMVector3TransformNormal(dirW, ballOrientInv);

        // Позиционируем объект точно на поверхности шара в локальных координатах
        float surfaceOffset = 1.0f + (obj.worldRadius * 0.5f) / ballRadius;
        XMVECTOR offsetL = XMVectorScale(XMVector3Normalize(dirL), surfaceOffset);
        XMStoreFloat3(&obj.localOffset, offsetL);

        XMMATRIX objRot = XMMatrixRotationRollPitchYaw(
            obj.rotation.x, obj.rotation.y, obj.rotation.z);
        XMMATRIX localRot = objRot * ballOrientInv;
        XMStoreFloat4x4(&obj.localRotation, localRot);

        // Рост шара
        float rb3 = ballRadius * ballRadius * ballRadius;
        float ro3 = obj.worldRadius * obj.worldRadius * obj.worldRadius;
        ballRadius = std::cbrt(rb3 + ro3 * 0.3f); // корень кубический из суммы объемов

        std::cout << "[Katamari] Absorbed #" << absorbedCount
            << "  ballRadius=" << ballRadius << '\n';
    }
}

void KatamariComponent::Draw()
{
    auto* ctx = game->Context.Get();
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
    const XMMATRIX& model, const XMMATRIX& view, const XMMATRIX& proj,
    const XMFLOAT4& color, const XMFLOAT3& camPos)
{
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (FAILED(game->Context->Map(constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        return;

    auto* cb = reinterpret_cast<PerObjectCB*>(mapped.pData);
    XMStoreFloat4x4(&cb->Model, XMMatrixTranspose(model));
    XMStoreFloat4x4(&cb->View, XMMatrixTranspose(view));
    XMStoreFloat4x4(&cb->Projection, XMMatrixTranspose(proj));
    cb->Color = color;
    cb->LightDir = XMFLOAT4(0.577f, 0.577f, 0.577f, 0.0f);
    cb->CameraPos = XMFLOAT4(camPos.x, camPos.y, camPos.z, 1.0f);

    game->Context->Unmap(constantBuffer.Get(), 0);
}

void KatamariComponent::DrawBall(const XMMATRIX& v, const XMMATRIX& p, const XMFLOAT3& cam)
{
    SetConstantBuffer(BallWorldMatrix(), v, p, XMFLOAT4(0.9f, 0.85f, 0.1f, 1.0f), cam);
    ballMesh->Bind(game->Context.Get());
    ballMesh->Draw(game->Context.Get());
}

void KatamariComponent::DrawFreeObject(const SceneObject& obj,
    const XMMATRIX& v, const XMMATRIX& p, const XMFLOAT3& cam)
{
    SetConstantBuffer(FreeObjectWorldMatrix(obj), v, p, obj.color, cam);
    obj.mesh->Bind(game->Context.Get());
    obj.mesh->Draw(game->Context.Get());
}

void KatamariComponent::DrawStuckObject(const SceneObject& obj,
    const XMMATRIX& v, const XMMATRIX& p, const XMFLOAT3& cam)
{
    SetConstantBuffer(StuckObjectWorldMatrix(obj), v, p, obj.color, cam);
    obj.mesh->Bind(game->Context.Get());
    obj.mesh->Draw(game->Context.Get());
}

void KatamariComponent::DrawFloor(const XMMATRIX& v, const XMMATRIX& p, const XMFLOAT3& cam)
{
    SetConstantBuffer(XMMatrixIdentity(), v, p, XMFLOAT4(0.25f, 0.55f, 0.25f, 1.0f), cam);
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
    XMMATRIX R = XMMatrixRotationRollPitchYaw(obj.rotation.x, obj.rotation.y, obj.rotation.z);
    XMMATRIX T = XMMatrixTranslation(obj.position.x, obj.position.y, obj.position.z);
    return S * R * T;
}

XMMATRIX KatamariComponent::StuckObjectWorldMatrix(const SceneObject& obj) const
{
    XMMATRIX S = XMMatrixScaling(obj.scale, obj.scale, obj.scale);
    XMMATRIX localRot = XMLoadFloat4x4(&obj.localRotation);
    XMVECTOR offsetV = XMLoadFloat3(&obj.localOffset);
    XMMATRIX T_local = XMMatrixTranslationFromVector(offsetV);

    XMMATRIX objLocal = S * localRot * T_local;
    XMMATRIX ballWorld = BallWorldMatrix();

    return objLocal * ballWorld;
}

void KatamariComponent::DestroyResources()
{
    rastState.Reset();
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