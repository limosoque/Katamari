#pragma once
#include "GameComponent.h"
#include "Mesh.h"
#include "Camera.h"
#include "ObjLoader.h"
#include "MeshGenerator.h"
#include "DisplayWin32.h"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl/client.h>

#include <vector>
#include <string>
#include <memory>
#include <random>

struct alignas(16) PerObjectCB
{
    DirectX::XMFLOAT4X4 Model;
    DirectX::XMFLOAT4X4 View;
    DirectX::XMFLOAT4X4 Projection;
    DirectX::XMFLOAT4 Color;
    DirectX::XMFLOAT4 LightDir;
    DirectX::XMFLOAT4 CameraPos;
};

struct SceneObject
{
    std::shared_ptr<Mesh> mesh;

	//Object state while not absorbed
    DirectX::XMFLOAT3 position = { 0, 0, 0 };
	DirectX::XMFLOAT3 rotation = { 0, 0, 0 };   //euler angles in radians
    float scale = 1.0f;
    float worldRadius = 1.0f;
    DirectX::XMFLOAT4 color = { 1, 1, 1, 1 };

    bool absorbed = false;

	//Object state while absorbed (relative to ball)
    DirectX::XMFLOAT3 localOffset = { 0, 0, 0 };
    DirectX::XMFLOAT4X4 localRotation = {};
};

enum class PlacementType {
    Upright,   //ex chair
    Flat,      //ex seashell
    Random     //other
};

struct ObjectDesc
{
    std::string objPath;
    std::wstring texPath;
    PlacementType placement;
    int count = 8;
    float minScale = 0.1f;
    float maxScale = 0.5f;
    float yOffset;
};

class KatamariComponent : public GameComponent
{
public:
    explicit KatamariComponent(
        Game* owner,
        std::vector<ObjectDesc> objectDescs,
        std::wstring ballTexturePath,
        std::wstring floorTexturePath,
        std::wstring shaderPath = L"shaders/Katamari.hlsl",
        float sceneRadius = 60.0f);

    void Initialize() override;
    void Update(float dt) override;
    void Draw() override;
    void DestroyResources() override;

    float BallRadius() const { return ballRadius; }
    int AbsorbedCount() const { return absorbedCount; }

private:
    std::vector<ObjectDesc> objectDescs;
    std::wstring shaderPath;
    float sceneRadius;

    //Ball
    std::shared_ptr<Mesh> ballMesh;
    float ballRadius = 1.0f;
    int ballStacks = 32;
    int ballSlices = 32;
    float ballSpeed = 7.f;
    DirectX::XMFLOAT3 ballPos = { 0, 0, 0 };
    DirectX::XMFLOAT4X4 ballOrientMtx;
    DirectX::XMFLOAT4 ballColor = { 1.f, 1.f, 1.f, 1.f };
    std::wstring ballTexPath;

    //Scene
    std::vector<SceneObject> objects;
    std::vector<std::shared_ptr<Mesh>> meshPool;
    float spawnMinDist = 6.0f;

    //Floor
    std::shared_ptr<Mesh> floorMesh;
    DirectX::XMFLOAT4 floorColor = { 1.f, 1.f, 1.f, 1.0f };
    std::wstring floorTexPath;

    //Camera
    Camera camera;
    float cameraYaw = 0.0f;

    //Stats
    int absorbedCount = 0;

    //D3D
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rastState;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthState;
    Microsoft::WRL::ComPtr<ID3D11BlendState> blendState;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerState;
    Microsoft::WRL::ComPtr<ID3DBlob> vsBytecode;

    std::mt19937 rng{ std::random_device{}() };

    void CompileShaders();
    void CreateInputLayout();
    void CreateConstantBuffer();
    void CreateRasterizerState();
    void CreateDepthStencilState();
    void CreateBlendState();
    void CreateSamplerState();
    void BuildBallMesh();
    void BuildFloorMesh();
    void BuildObjectMeshes();
    void ScatterObjects();

    void UpdateBallPhysics(float dt);
    void CheckAbsorption();

    void DrawBall(const DirectX::XMMATRIX& v, const DirectX::XMMATRIX& p, const DirectX::XMFLOAT3& cam);
    void DrawFreeObject(const SceneObject& obj, const DirectX::XMMATRIX& v, const DirectX::XMMATRIX& p, const DirectX::XMFLOAT3& cam);
    void DrawStuckObject(const SceneObject& obj, const DirectX::XMMATRIX& v, const DirectX::XMMATRIX& p, const DirectX::XMFLOAT3& cam);
    void DrawFloor(const DirectX::XMMATRIX& v, const DirectX::XMMATRIX& p, const DirectX::XMFLOAT3& cam);

    void SetConstantBuffer(const DirectX::XMMATRIX& model, const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj, const DirectX::XMFLOAT4& color, const DirectX::XMFLOAT3& camPos);

    DirectX::XMMATRIX BallWorldMatrix() const;
    DirectX::XMMATRIX FreeObjectWorldMatrix(const SceneObject&) const;
    DirectX::XMMATRIX StuckObjectWorldMatrix(const SceneObject&) const;

    float RandomFloat(float lo, float hi);

    float GetTerrainHeight(float x, float z) const;
    DirectX::XMVECTOR GetTerrainNormal(float x, float z) const;
    DirectX::XMVECTOR GetRotationBetweenVectors(DirectX::XMVECTOR from, DirectX::XMVECTOR to);
};