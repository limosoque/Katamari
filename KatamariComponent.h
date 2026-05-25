#pragma once
#include "GameComponent.h"
#include "Mesh.h"
#include "Camera.h"
#include "ObjLoader.h"
#include "MeshGenerator.h"
#include "Material.h"
#include "DisplayWin32.h"
#include "ShadowMap.h"
#include "ShadowMapHud.h"
#include "RenderingSystem.h"
#include "ParticleSystem.h"
#include "ParticleRenderer.h"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl/client.h>

#include <vector>
#include <string>
#include <memory>
#include <random>

static constexpr int kMaxShotLights = 8; //Max active shot lights sent to the shader
static constexpr int kMaxShotTrailStamps = 64; //Max active trail stamps sent to the shader

struct alignas(16) PerObjectCB
{
    DirectX::XMFLOAT4X4 WorldMatrix;
    DirectX::XMFLOAT4X4 ViewMatrix;
    DirectX::XMFLOAT4X4 ProjectionMatrix;

    DirectX::XMFLOAT4 MaterialAmbientColor;
    DirectX::XMFLOAT4 MaterialDiffuseColor;
	DirectX::XMFLOAT4 MaterialSpecularColor;
	
    float MaterialShininess;
    float Padding[3];

    DirectX::XMFLOAT4 SunlightColor;
    DirectX::XMFLOAT4 SunlightDirection;
    DirectX::XMFLOAT4 CameraPosition;

    DirectX::XMFLOAT4X4 LightViewProj[kCascadeCount];
    DirectX::XMFLOAT4   CascadeSplits;

    DirectX::XMFLOAT4 ShotLightColorAndRange; // rgb = shot color, a = point-light range
    DirectX::XMFLOAT4 ShotLightCountsAndFlags;// x = light count, y = trail count, z = ground mask, w = emissive
    DirectX::XMFLOAT4 ShotLights[kMaxShotLights];// xyz = light position, w = faded intensity
    DirectX::XMFLOAT4 ShotTrailStamps[kMaxShotTrailStamps];// xy = ground XZ center, z = radius, w = faded intensity
};

struct SceneObject
{
    std::shared_ptr<Mesh> mesh;

	//Object state while not absorbed
    DirectX::XMFLOAT3 position = { 0, 0, 0 };
	DirectX::XMFLOAT3 rotation = { 0, 0, 0 }; //euler angles in radians
    float scale = 1.0f;
    float worldRadius = 1.0f;
    DirectX::XMFLOAT4 color = { 1, 1, 1, 1 };
    Material material = Material::Plastic();

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
    float yOffset = 0.0f;
    Material material = Material::Plastic();

    ObjectDesc(std::string obj, std::wstring tex, PlacementType p, int c, float minS, float maxS, float yOff, Material m)
        : objPath(obj), texPath(tex), placement(p), count(c), minScale(minS), maxScale(maxS), yOffset(yOff), material(m) {}
};

struct LightShotSettings
{
    float intensity = 2.f;                               // Point-light power used by scene shading
    float range = 6.0f;                                  // Point-light radius in world units
    float speed = 14.0f;                                 
    float lifetime = 3.f;                                
    float fireCooldown = 0.5f;                           
    float hoverHeight = 1.5f;                            // Height above terrain height
    float spawnForwardOffset = 0.f;                      // Extra spawn distance in front of the ball
    float visualRadius = 0.1f;                           // Rendered emissive sphere radius
    float visualIntensity = 2.f;                         // Brightness of the rendered sphere
    float trailEmitInterval = 0.02f;                     // Seconds between trail stamps
    float trailLifetime = 0.1f;                          // Seconds before a trail stamp fades out
    float trailRadius = 0.05f;                           // Base radius of each ground glow stamp
    float trailIntensity = 0.05f;                        // Base brightness of each ground glow stamp
    DirectX::XMFLOAT3 color = { 1.0f, 0.75f, 0.35f };    // Shared color for light, sphere, and trail
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
    LightShotSettings& ShotSettings() { return lightShotSettings; }
    const LightShotSettings& ShotSettings() const { return lightShotSettings; }

private:
    struct ActiveLightShot
    {
        DirectX::XMFLOAT3 position = { 0, 0, 0 };     // Current world-space center
        DirectX::XMFLOAT3 direction = { 0, 0, -1 };   // Fixed horizontal travel direction
        float age = 0.0f;                             //in secs
        float trailClock = 0.0f;                      // time until next trail stamp
    };

    struct LightTrailStamp
    {
        DirectX::XMFLOAT3 position = { 0, 0, 0 };     // Ground-space stamp center
        float age = 0.0f;                             //in secs
    };

    std::vector<ObjectDesc> objectDescs;
    std::wstring shaderPath;
	std::wstring shadowShaderPath = L"shaders/ShadowPass.hlsl";
    float sceneRadius;

    //Sun
    DirectX::XMFLOAT4 SunlightDirection = { 0.577f, 0.577f, 0.577f, 0.0f };
    DirectX::XMFLOAT4 SunlightColor = { 1.0f, 1.0f, 1.0f, 1.0f };

    //Ball
    std::shared_ptr<Mesh> ballMesh;
    float ballRadius = 1.0f;
    int ballStacks = 32;
    int ballSlices = 32;
    float ballSpeed = 7.f;
    DirectX::XMFLOAT3 ballPos = { 0, 0, 0 };
    DirectX::XMFLOAT4X4 ballOrientMtx;
    DirectX::XMFLOAT4 ballColor = { 1.f, 1.f, 1.f, 1.f };
    Material ballMaterial = Material::Rubber();
    std::wstring ballTexPath;

    //Scene
    std::vector<SceneObject> objects;
    std::vector<std::shared_ptr<Mesh>> meshPool;
    float spawnMinDist = 6.0f;

    //Floor
    std::shared_ptr<Mesh> floorMesh;
    DirectX::XMFLOAT4 floorColor = { 1.f, 1.f, 1.f, 1.0f };
    Material floorMaterial = Material::Ground();
    std::wstring floorTexPath;

    //Camera
    Camera camera;
    float cameraYaw = 0.0f;

    //Stats
    int absorbedCount = 0;

    // Light shots
    LightShotSettings lightShotSettings;               //Tunable shooting and lighting parameters
    std::vector<ActiveLightShot> lightShots;           //Active moving light projectiles
    std::vector<LightTrailStamp> lightTrailStamps;     //Active fading ground glow stamps
    float lightShotCooldown = 0.0f;                    //Remaining time until the next shot can spawn
    Material lightShotGlowMaterial = Material::Light();

    // Particles
    ParticleEmitterSettings lightShotParticleSettings;
    ParticleSystem lightShotParticles;
    ParticleRenderer particleRenderer;
    std::vector<ParticleVertex> particleDrawVertices;

    // Shadow
    ShadowData shadow;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> shadowVertexShader;
    Microsoft::WRL::ComPtr<ID3D11GeometryShader> shadowGeometryShader;
    Microsoft::WRL::ComPtr<ID3D11Buffer> shadowConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> shadowCascadeBuffer;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> shadowRastState;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> shadowSamplerState;
    ShadowMapHud shadowMapHud;
    bool shadowHudToggleHeld = false;
    bool shadowHudInvertToggleHeld = false;

    // Deferred rendering
    RenderingSystem renderingSystem;
    bool demoSpotLightEnabled = true;
    DirectX::XMFLOAT3 demoSpotLightColor = { 0.55f, 0.65f, 1.0f };
    float demoSpotLightIntensity = 0.8f;
    float demoSpotLightRange = 18.0f;

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

    void CompileShadowShader();
    void CreateShadowConstantBuffer();
    void CreateShadowCascadeBuffer();
    void CreateShadowRasterizerState();
    void CreateShadowSamplerState();
    void RenderShadowPass();
    void DrawSceneForShadow();
    void UpdateShadowHudInput(float dt);

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
    void UpdateLightShots(float dt);
    void SpawnLightShot();
    void EmitLightTrail(const DirectX::XMFLOAT3& shotPosition);
    void EmitShotParticles(const DirectX::XMFLOAT3& shotPosition, const DirectX::XMFLOAT3& shotDirection, int burstCount);
    DirectX::XMVECTOR LightShotSpawnDirection() const;
    float ComputeLightShotFade(float age, float lifetime) const;

    void DrawBall(const DirectX::XMMATRIX& v, const DirectX::XMMATRIX& p, const DirectX::XMFLOAT3& cam);
    void DrawFreeObject(const SceneObject& obj, const DirectX::XMMATRIX& v, const DirectX::XMMATRIX& p, const DirectX::XMFLOAT3& cam);
    void DrawStuckObject(const SceneObject& obj, const DirectX::XMMATRIX& v, const DirectX::XMMATRIX& p, const DirectX::XMFLOAT3& cam);
    void DrawFloor(const DirectX::XMMATRIX& v, const DirectX::XMMATRIX& p, const DirectX::XMFLOAT3& cam);
    void DrawLightShots(const DirectX::XMMATRIX& v, const DirectX::XMMATRIX& p, const DirectX::XMFLOAT3& cam);
    void DrawParticles(const DirectX::XMMATRIX& v, const DirectX::XMMATRIX& p, const DirectX::XMFLOAT3& cam, int width, int height);
    void ApplyForwardPipeline();
    DeferredLightingData BuildDeferredLightingData(const DirectX::XMFLOAT3& camPos) const;

    void SetConstantBuffer(const DirectX::XMMATRIX& world, const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& projection, const Material& material, const DirectX::XMFLOAT3& camPos, float groundTrailMask = 0.0f, float lightShotEmissive = 0.0f);
    void FillLightShotConstants(PerObjectCB* cb, float groundTrailMask, float lightShotEmissive) const;

    DirectX::XMMATRIX BallWorldMatrix() const;
    DirectX::XMMATRIX FreeObjectWorldMatrix(const SceneObject&) const;
    DirectX::XMMATRIX StuckObjectWorldMatrix(const SceneObject&) const;

    float RandomFloat(float lo, float hi);

    float GetTerrainHeight(float x, float z) const;
    DirectX::XMVECTOR GetTerrainNormal(float x, float z) const;
    DirectX::XMVECTOR GetRotationBetweenVectors(DirectX::XMVECTOR from, DirectX::XMVECTOR to);
};
