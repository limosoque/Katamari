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
#include "GBufferPicker.h"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl/client.h>

#include <vector>
#include <string>
#include <memory>
#include <random>
#include <cstdint>
#include <utility>

const int kMaxShotLights = 8; //Max active shot lights sent to the shader
const int kMaxShotTrailStamps = 64; //Max active trail stamps sent to the shader
const uint32_t kInvalidObjectId = 0;
const uint32_t kFloorObjectId = 1;
const uint32_t kBallObjectId = 2;
const uint32_t kFirstSceneObjectId = 100;

struct alignas(16) PerObjectCB
{
    DirectX::XMFLOAT4X4 WorldMatrix;
    DirectX::XMFLOAT4X4 ViewMatrix;
    DirectX::XMFLOAT4X4 ProjectionMatrix;

    DirectX::XMFLOAT4 MaterialAmbientColor;
    DirectX::XMFLOAT4 MaterialDiffuseColor;
	DirectX::XMFLOAT4 MaterialSpecularColor;
	
    float MaterialShininess;
    uint32_t ObjectId;
    float Padding[2];

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
    uint32_t objectId = kInvalidObjectId;
    std::string debugName;
    std::string sourceMeshPath;

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
    bool particleSdfEnabled = true;                       // Makes light-shot particles react to active shot spheres
    float particleSdfRadius = 0.24f;                      // Collision radius around the visible shot sphere
    float particleSdfInfluenceDistance = 0.42f;           // Extra distance where particles start bending around the shot
    float particleSdfRepelStrength = 5.0f;                // Outward force applied to particles near the shot sphere
    float particleSdfVelocityTransfer = 0.18f;            // Fraction of shot velocity inherited by nearby particles
    float particleSdfSurfaceOffset = 0.03f;               // Keeps particles just outside the shot sphere
    float trailEmitInterval = 0.02f;                     // Seconds between trail stamps
    float trailLifetime = 0.1f;                          // Seconds before a trail stamp fades out
    float trailRadius = 0.05f;                           // Base radius of each ground glow stamp
    float trailIntensity = 0.05f;                        // Base brightness of each ground glow stamp
    DirectX::XMFLOAT3 color = { 1.0f, 0.75f, 0.35f };    // Shared color for light, sphere, and trail
};

struct ParticleSdfSettings
{
    bool enabled = true;                                  // Enables ball/particle SDF interaction for this emitter
    float influenceDistance = 2.2f;                       // Extra distance around the ball where particles start reacting
    float repelStrength = 28.0f;                          // Outward acceleration applied near the ball surface
    float velocityTransfer = 0.45f;                       // Fraction of ball velocity transferred into nearby particles
    float surfaceOffset = 0.08f;                          // Keeps particles slightly outside the ball SDF surface
};

struct WaterfallDesc
{
    std::string debugName = "Waterfall";
    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };       // XZ anchor; Y is used only when anchorToTerrain is false
    float emitterYaw = 0.0f;                                 // Horizontal rotation of the rectangular emitter in radians
    DirectX::XMFLOAT3 flowDirection = { 0.0f, -1.0f, 0.0f }; // Initial particle flow direction
    float width = 5.0f;                                      // Emitter rectangle width in world units
    float depth = 0.8f;                                      // Emitter rectangle thickness in world units
    float heightAboveTerrain = 13.0f;                        // Spawn height over terrain when anchorToTerrain is true
    float emissionRate = 900.0f;                             // Particles spawned per second
    int maxEmitPerFrame = 384;                               // Safety cap for low-FPS frames
    bool anchorToTerrain = true;                             // Recomputes top center from terrain height at position.xz
    ParticleEmitterSettings particleSettings;                // Per-waterfall GPU particle visuals and motion
    ParticleSdfSettings sdfSettings;                         // Per-waterfall reaction to the katamari ball SDF

    WaterfallDesc() = default;
    WaterfallDesc(
        std::string name,
        const DirectX::XMFLOAT3& pos,
        float emitterWidth,
        float emitterDepth,
        float emitterHeightAboveTerrain,
        float rate,
        const ParticleEmitterSettings& particles)
        : debugName(std::move(name)),
          position(pos),
          width(emitterWidth),
          depth(emitterDepth),
          heightAboveTerrain(emitterHeightAboveTerrain),
          emissionRate(rate),
          particleSettings(particles)
    {
    }
};

struct FountainDesc
{
    std::string debugName = "Fountain";
    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };       // XZ anchor; Y is used only when anchorToTerrain is false
    float emitterYaw = 0.0f;                                 // Horizontal rotation of the ground emitter in radians
    DirectX::XMFLOAT3 flowDirection = { 0.0f, 1.0f, 0.0f };  // Initial jet direction
    float width = 0.8f;                                      // Ground emitter width in world units
    float depth = 0.8f;                                      // Ground emitter depth in world units
    float heightOffset = 0.05f;                              // Spawn height over terrain when anchorToTerrain is true
    float emissionRate = 700.0f;                             // Particles spawned per second
    int maxEmitPerFrame = 256;                               // Safety cap for low-FPS frames
    bool anchorToTerrain = true;                             // Recomputes emitter center from terrain height at position.xz
    ParticleEmitterSettings particleSettings;                // Per-fountain GPU particle visuals and motion
    ParticleSdfSettings sdfSettings;                         // Per-fountain reaction to the katamari ball SDF

    FountainDesc() = default;
    FountainDesc(
        std::string name,
        const DirectX::XMFLOAT3& pos,
        float emitterWidth,
        float emitterDepth,
        float emitterHeightOffset,
        float rate,
        const ParticleEmitterSettings& particles)
        : debugName(std::move(name)),
          position(pos),
          width(emitterWidth),
          depth(emitterDepth),
          heightOffset(emitterHeightOffset),
          emissionRate(rate),
          particleSettings(particles)
    {
    }
};

class KatamariComponent : public GameComponent
{
public:
    explicit KatamariComponent(
        Game* owner,
        std::vector<ObjectDesc> objectDescs,
        std::vector<WaterfallDesc> waterfallDescs,
        std::vector<FountainDesc> fountainDescs,
        std::wstring ballTexturePath,
        std::wstring floorTexturePath,
        std::wstring lightShotVisualShaderPath = L"shaders/LightShotVisual.hlsl",
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

    struct ActiveWaterfall
    {
        WaterfallDesc desc;
        ParticleSystem particles;
        DirectX::XMFLOAT3 topCenter = { 0.0f, 10.0f, 0.0f };
        DirectX::XMFLOAT3 widthAxis = { 1.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 depthAxis = { 0.0f, 0.0f, 1.0f };
        float emissionAccumulator = 0.0f;
    };

    struct ActiveFountain
    {
        FountainDesc desc;
        ParticleSystem particles;
        DirectX::XMFLOAT3 emitterCenter = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 widthAxis = { 1.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 depthAxis = { 0.0f, 0.0f, 1.0f };
        float emissionAccumulator = 0.0f;
    };

    std::vector<ObjectDesc> objectDescs;
    std::vector<WaterfallDesc> waterfallDescs;
    std::vector<FountainDesc> fountainDescs;
    std::wstring lightShotVisualShaderPath;
	std::wstring shadowShaderPath = L"shaders/ShadowPass.hlsl";
    float sceneRadius;

    //Sun
    DirectX::XMFLOAT4 SunlightDirection = { 0.577f, 0.577f, 0.577f, 0.0f };
    DirectX::XMFLOAT4 SunlightColor = { 1.0f, 1.0f, 1.0f, 1.5f };

    //Ball
    std::shared_ptr<Mesh> ballMesh;
    float ballRadius = 1.0f;
    int ballStacks = 32;
    int ballSlices = 32;
    float ballSpeed = 7.f;
    DirectX::XMFLOAT3 ballPos = { 0, 0, 0 };
    DirectX::XMFLOAT3 previousBallPos = { 0, 0, 0 };
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
    bool gamePaused = false;
    bool pauseToggleHeld = false;

    // Light shots
    LightShotSettings lightShotSettings;               //Tunable shooting and lighting parameters
    std::vector<ActiveLightShot> lightShots;           //Active moving light projectiles
    std::vector<LightTrailStamp> lightTrailStamps;     //Active fading ground glow stamps
    float lightShotCooldown = 0.0f;                    //Remaining time until the next shot can spawn
    Material lightShotGlowMaterial = Material::Light();

    // Particles
    ParticleEmitterSettings lightShotParticleSettings;
    ParticleSystem lightShotParticles;
    std::vector<ActiveWaterfall> waterfalls;
    std::vector<ActiveFountain> fountains;
    ParticleRenderer particleRenderer;

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
    bool cascadeColorDebugEnabled = false;
    bool cascadeColorDebugToggleHeld = false;

    // Deferred rendering
    RenderingSystem renderingSystem;
    GBufferPicker gBufferPicker;
    bool pendingGBufferPick = false;
    uint32_t pendingPickX = 0;
    uint32_t pendingPickY = 0;
    bool demoSpotLightEnabled = true;
    DirectX::XMFLOAT3 demoSpotLightColor = { 0.55f, 0.65f, 1.0f };
    float demoSpotLightIntensity = 0.8f;
    float demoSpotLightRange = 18.0f;

    //D3D
    Microsoft::WRL::ComPtr<ID3D11VertexShader> lightShotVisualVertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> lightShotVisualPixelShader;
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

    void CompileLightShotVisualShaders();
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
    void CreateWaterfalls();
    void CreateFountains();

    void UpdateBallPhysics(float dt);
    void CheckAbsorption();
    void UpdateLightShots(float dt);
    void UpdateLightShotParticles(float dt);
    void UpdateWaterfalls(float dt, const DirectX::XMFLOAT3& ballVelocity);
    void UpdateFountains(float dt, const DirectX::XMFLOAT3& ballVelocity);
    void UpdateMouseCameraAndPicking(float dt);
    void UpdatePauseInput();
    void ApplyCameraFrameState();
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
    void ApplyLightShotVisualPipeline();
    DeferredLightingData BuildDeferredLightingData(const DirectX::XMFLOAT3& camPos) const;
    void ExecutePendingGBufferPick(uint32_t width, uint32_t height, const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& projection);
    std::string DescribeObjectId(uint32_t objectId) const;

    void SetConstantBuffer(const DirectX::XMMATRIX& world, const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& projection, const Material& material, const DirectX::XMFLOAT3& camPos, uint32_t objectId = kInvalidObjectId, float groundTrailMask = 0.0f, float lightShotEmissive = 0.0f);
    void FillLightShotConstants(PerObjectCB* cb, float groundTrailMask, float lightShotEmissive) const;

    DirectX::XMMATRIX BallWorldMatrix() const;
    DirectX::XMMATRIX FreeObjectWorldMatrix(const SceneObject&) const;
    DirectX::XMMATRIX StuckObjectWorldMatrix(const SceneObject&) const;

    float RandomFloat(float lo, float hi);

    float GetTerrainHeight(float x, float z) const;
    DirectX::XMVECTOR GetTerrainNormal(float x, float z) const;
    DirectX::XMVECTOR GetRotationBetweenVectors(DirectX::XMVECTOR from, DirectX::XMVECTOR to);
};
