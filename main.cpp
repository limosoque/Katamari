#include "Game.h"
#include "KatamariComponent.h"
#include <stdexcept>
#include <iostream>
#include <random>
#include <string>
#include <cmath>
#include <utility>
#include <chrono>
#include <cstddef>
#include "Material.h"

#include <ShellScalingApi.h>
#pragma comment(lib, "Shcore.lib")

float RandomFloat(std::mt19937& rng, float minValue, float maxValue)
{
    return std::uniform_real_distribution<float>(minValue, maxValue)(rng);
}

int AutomaticMaxEmitPerFrame(float emissionRate)
{
    float maxCatchUpSeconds = 0.25f;
    int cap = static_cast<int>(std::ceil(emissionRate * maxCatchUpSeconds));
    return cap > 1 ? cap : 1;
}

size_t AutomaticMaxParticles(float emissionRate, float lifetimeMax)
{
    float particleBufferHeadroom = 1.25f;
    size_t minimumParticleBufferSize = 1024;
    size_t needed = static_cast<size_t>(std::ceil(emissionRate * lifetimeMax * particleBufferHeadroom));
    return needed > minimumParticleBufferSize ? needed : minimumParticleBufferSize;
}

DirectX::XMFLOAT4 RandomParticleColor(std::mt19937& rng)
{
    return DirectX::XMFLOAT4(
        RandomFloat(rng, 0.0f, 1.0f),
        RandomFloat(rng, 0.0f, 1.0f),
        RandomFloat(rng, 0.0f, 1.0f),
        RandomFloat(rng, 0.0f, 1.0f));
}

std::pair<float, float> RandomParticleBrightnessRange(std::mt19937& rng)
{
    float a = RandomFloat(rng, 0.0f, 1.0f);
    float b = RandomFloat(rng, 0.0f, 1.0f);
    float minBrightness = a < b ? a : b;
    float maxBrightness = a < b ? b : a;
    return std::pair<float, float>(minBrightness, maxBrightness);
}

int main()
{
    SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);


    try
    {
        float sceneRadius = 120.0f;
        Game game(L"Katamari", 1920, 1080);

        //scale objects
        std::vector<ObjectDesc> objects =
        {
            ObjectDesc("assets/childrens_chair/childrens_chair.obj", L"assets/childrens_chair/childrens_chair_Albedo.png", PlacementType::Upright, 50, 0.02f, 0.025f, 0.02f, Material::Wood()),
            ObjectDesc("assets/creeper/CreeperZ.obj", L"assets/creeper/creeper.png", PlacementType::Upright, 50, 0.3f, 0.5f, 0.02f, Material::Organic()),
            ObjectDesc("assets/woman/obj.obj", L"assets/childrens_chair/childrens_chair_Normal.png", PlacementType::Upright, 52, 0.015f, 0.03f, 0.02f, Material::Metal()),
            ObjectDesc("assets/seashell/seashell_rapan-sl-0.obj", L"assets/seashell/rapana_diffuse.png", PlacementType::Flat, 52, 0.05f, 0.2f, -0.2f, Material::Ceramic()),
            ObjectDesc("assets/mouse/W_hlmaus.obj", L"assets/mouse/Feldmaus_Diffuse.png", PlacementType::Flat, 52, 0.001f, 0.003f, 0.02f, Material::Organic())
        };

        long long particleSeedTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        unsigned int randomWaterfallSeed = static_cast<unsigned int>(particleSeedTime);
        unsigned int randomFountainSeed = randomWaterfallSeed + 1u;


        DirectX::XMFLOAT3 gravity = DirectX::XMFLOAT3(0.0f, -9.81f, 0.0f);
        DirectX::XMFLOAT3 wind = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);


        //Waterfall count
        int randomWaterfallCount = 20; 

		//size and shape of the waterfall emitter area
        float waterfallWidthMin = 4.f;
        float waterfallWidthMax = 10.f;
        float waterfallDepthMin = 0.5f;
        float waterfallDepthMax = 1.5f;
        float waterfallHeightMin = 10.f;
        float waterfallHeightMax = 15.f;

        /*particle settings*/
        float waterfallStartSizeMin = 0.1f;
        float waterfallStartSizeMax = 0.3f;
        float waterfallEndSizeMin = 0.05f;
        float waterfallEndSizeMax = 0.2f;

        //spawned particels count
        float waterfallEmissionRateMin = 500.f;
        float waterfallEmissionRateMax = 1500.f;

        float waterfallLifetimeMin = 2.f;
        float waterfallLifetimeMax = 5.f;

        float waterfallSpeedMin = 0.5f;
        float waterfallSpeedMax = 5.f;

        //spread relative to flow direction
        float waterfallSpreadMin = 0.1f;
        float waterfallSpreadMax = 1.5f;

		/*physics parameters*/
		//air resistance applied to velocity
        float waterfallDragMin = 0.01f;
        float waterfallDragMax = 2.f; 

        //bounce from ground (0, 1)
        float waterfallGroundRestitutionMin = 0.0f;
        float waterfallGroundRestitutionMax = 1.0f;
        
		//friction from ground (0, 1)
        float waterfallGroundFrictionMin = 0.0f;
        float waterfallGroundFrictionMax = 1.0f;

		//SDF push from ball power
        float waterfallSdfRepelMin = 25.f;
        float waterfallSdfRepelMax = 50.f;
        
		//SDF velocity transfer from ball (0, 1)
        float waterfallSdfVelocityTransferMin = 0.1f;
        float waterfallSdfVelocityTransferMax = 1.f;

        std::mt19937 waterfallRng(randomWaterfallSeed);

        std::vector<WaterfallDesc> waterfalls;
        waterfalls.reserve(randomWaterfallCount);

        for (int i = 0; i < randomWaterfallCount; ++i)
        {
            ParticleEmitterSettings randomParticles;
            float emissionRate = RandomFloat(waterfallRng, waterfallEmissionRateMin, waterfallEmissionRateMax);
            randomParticles.emissionRate = emissionRate;
            randomParticles.maxParticles = AutomaticMaxParticles(emissionRate, waterfallLifetimeMax);
            randomParticles.lifetimeMin = waterfallLifetimeMin;
            randomParticles.lifetimeMax = waterfallLifetimeMax;
            randomParticles.speedMin = waterfallSpeedMin;
            randomParticles.speedMax = waterfallSpeedMax;
            randomParticles.spreadRadians = RandomFloat(waterfallRng, waterfallSpreadMin, waterfallSpreadMax);
            randomParticles.startSizeMin = waterfallStartSizeMin;
            randomParticles.startSizeMax = waterfallStartSizeMax;
            randomParticles.endSizeMin = waterfallEndSizeMin;
            randomParticles.endSizeMax = waterfallEndSizeMax;
            randomParticles.drag = RandomFloat(waterfallRng, waterfallDragMin, waterfallDragMax);
            randomParticles.gravity = gravity;
            randomParticles.wind = wind;
            randomParticles.groundCollision.surfaceOffset = randomParticles.startSizeMin;
            randomParticles.groundCollision.restitution = RandomFloat(waterfallRng, waterfallGroundRestitutionMin, waterfallGroundRestitutionMax);
            randomParticles.groundCollision.friction = RandomFloat(waterfallRng, waterfallGroundFrictionMin, waterfallGroundFrictionMax);
            std::pair<float, float> brightness = RandomParticleBrightnessRange(waterfallRng);
            randomParticles.brightnessMin = brightness.first;
            randomParticles.brightnessMax = brightness.second;
            randomParticles.startColor = RandomParticleColor(waterfallRng);
            randomParticles.endColor = RandomParticleColor(waterfallRng);

            DirectX::XMFLOAT3 position(
                RandomFloat(waterfallRng, -sceneRadius, sceneRadius),
                0.0f,
                RandomFloat(waterfallRng, -sceneRadius, sceneRadius));
            float width = RandomFloat(waterfallRng, waterfallWidthMin, waterfallWidthMax);
            float depth = RandomFloat(waterfallRng, waterfallDepthMin, waterfallDepthMax);
            float height = RandomFloat(waterfallRng, waterfallHeightMin, waterfallHeightMax);

            WaterfallDesc desc(
                "Random waterfall " + std::to_string(i + 1),
                position,
                width,
                depth,
                height,
                emissionRate,
                randomParticles);

            desc.emitterYaw = RandomFloat(waterfallRng, 0.0f, DirectX::XM_2PI);
            desc.maxEmitPerFrame = AutomaticMaxEmitPerFrame(emissionRate);
            desc.sdfSettings.influenceDistance = randomParticles.startSizeMax;
            desc.sdfSettings.repelStrength = RandomFloat(waterfallRng, waterfallSdfRepelMin, waterfallSdfRepelMax);
            desc.sdfSettings.velocityTransfer = RandomFloat(waterfallRng, waterfallSdfVelocityTransferMin, waterfallSdfVelocityTransferMax);
            desc.sdfSettings.surfaceOffset = randomParticles.startSizeMax;

            waterfalls.push_back(desc);
        }

        //Fountain count
        int randomFountainCount = 40;

        //size and shape of the fountain emitter area
        float fountainEmitterWidthMin = 0.1f; 
        float fountainEmitterWidthMax = 1.f;
        float fountainEmitterDepthMin = 0.1f;
        float fountainEmitterDepthMax = 1.f;

        /*particle settings*/
        float fountainStartSizeMin = 0.03f;
        float fountainStartSizeMax = 0.1f;
        float fountainEndSizeMin = 0.1f;
        float fountainEndSizeMax = 0.3f;

        //spawned particles count
        float fountainEmissionRateMin = 500.f;
        float fountainEmissionRateMax = 1500.f;

        float fountainLifetimeMin = 2.f;
        float fountainLifetimeMax = 5.f;

        float fountainSpeedMin = 5.f;
        float fountainSpeedMax = 10.f;

        //spread relative to flow direction
        float fountainSpreadMin = 2.f;
        float fountainSpreadMax = 3.5f; 
        
        /*physics parameters*/
        //air resistance applied to velocity
        float fountainDragMin = 0.01f;
        float fountainDragMax = 2.f;

		//bounce from ground (0, 1)
        float fountainGroundRestitutionMin = 0.f;
        float fountainGroundRestitutionMax = 1.f;

		//friction from ground (0, 1)
        float fountainGroundFrictionMin = 0.f;
        float fountainGroundFrictionMax = 1.f;

        //SDF push from ball power
        float fountainSdfRepelMin = 25.0f;
        float fountainSdfRepelMax = 50.0f;

        //SDF velocity transfer from ball (0, 1)
        float fountainSdfVelocityTransferMin = 0.1f;
        float fountainSdfVelocityTransferMax = 1.f;

        std::mt19937 fountainRng(randomFountainSeed);
        std::vector<FountainDesc> fountains;
        fountains.reserve(randomFountainCount);

        for (int i = 0; i < randomFountainCount; ++i)
        {
            ParticleEmitterSettings randomParticles;
            float emissionRate = RandomFloat(fountainRng, fountainEmissionRateMin, fountainEmissionRateMax);
            randomParticles.emissionRate = emissionRate;
            randomParticles.maxParticles = AutomaticMaxParticles(emissionRate, fountainLifetimeMax);
            randomParticles.lifetimeMin = fountainLifetimeMin;
            randomParticles.lifetimeMax = fountainLifetimeMax;
            randomParticles.speedMin = fountainSpeedMin;
            randomParticles.speedMax = fountainSpeedMax;
            randomParticles.spreadRadians = RandomFloat(fountainRng, fountainSpreadMin, fountainSpreadMax);
            randomParticles.startSizeMin = fountainStartSizeMin;
            randomParticles.startSizeMax = fountainStartSizeMax;
            randomParticles.endSizeMin = fountainEndSizeMin;
            randomParticles.endSizeMax = fountainEndSizeMax;
            randomParticles.drag = RandomFloat(fountainRng, fountainDragMin, fountainDragMax);
            randomParticles.gravity = gravity;
            randomParticles.wind = wind;
            randomParticles.groundCollision.surfaceOffset = randomParticles.startSizeMin;
            randomParticles.groundCollision.restitution = RandomFloat(fountainRng, fountainGroundRestitutionMin, fountainGroundRestitutionMax);
            randomParticles.groundCollision.friction = RandomFloat(fountainRng, fountainGroundFrictionMin, fountainGroundFrictionMax);
            std::pair<float, float> brightness = RandomParticleBrightnessRange(fountainRng);
            randomParticles.brightnessMin = brightness.first;
            randomParticles.brightnessMax = brightness.second;
            randomParticles.startColor = RandomParticleColor(fountainRng);
            randomParticles.endColor = RandomParticleColor(fountainRng);

            DirectX::XMFLOAT3 position(
                RandomFloat(fountainRng, -sceneRadius, sceneRadius),
                0.0f,
                RandomFloat(fountainRng, -sceneRadius, sceneRadius));

            FountainDesc desc(
                "Random fountain " + std::to_string(i + 1),
                position,
                RandomFloat(fountainRng, fountainEmitterWidthMin, fountainEmitterWidthMax),
                RandomFloat(fountainRng, fountainEmitterDepthMin, fountainEmitterDepthMax),
                0,
                emissionRate,
                randomParticles);

            desc.emitterYaw = RandomFloat(fountainRng, 0.0f, DirectX::XM_2PI);
            desc.flowDirection = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
            desc.maxEmitPerFrame = AutomaticMaxEmitPerFrame(emissionRate);
            desc.sdfSettings.influenceDistance = randomParticles.startSizeMax;
            desc.sdfSettings.repelStrength = RandomFloat(fountainRng, fountainSdfRepelMin, fountainSdfRepelMax);
            desc.sdfSettings.velocityTransfer = RandomFloat(fountainRng, fountainSdfVelocityTransferMin, fountainSdfVelocityTransferMax);
            desc.sdfSettings.surfaceOffset = randomParticles.startSizeMax;

            fountains.push_back(desc);
        }

        auto katamari = std::make_unique<KatamariComponent>(
            &game,
            objects,
            waterfalls,
            fountains,
            L"assets/ball/basketball/ball_basecolor.png",
            L"assets/ground/forrest_ground_01_diff_4k.jpg",
            L"shaders/LightShotVisual.hlsl",
            sceneRadius
        );

        KatamariComponent* kPtr = katamari.get();
        game.AddComponent(std::move(katamari));

        std::cout << "Controls:\n"
            << "W/A/S/D - move the ball\n"
            << "Space - shoot a terrain-following point light\n"
            << "P - toggle pause\n"
            << "Right mouse drag - orbit camera\n"
            << "Left mouse click - GPU GBuffer pick\n"
            << "F1 - toggle shadow map HUD\n"
            //<< "F2 - toggle shadow HUD contour preview\n"
            << "C - toggle cascade color debug view\n"
            //<< "</> - adjust shadow HUD exposure\n"
            //<< "Q/E - rotate camera left/right\n"
            << "Esc - quit\n\n";

        game.Run();

        std::cout << "Session ended.\n"
            << "Absorbed: " << kPtr->AbsorbedCount() << '\n'
            << "Ball radius: " << kPtr->BallRadius() << '\n';
    }
    catch (const std::exception& e)
    {
        std::cerr << "[FATAL] " << e.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
