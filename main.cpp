#include "Game.h"
#include "KatamariComponent.h"
#include <stdexcept>
#include <iostream>
#include "Material.h"

#include <ShellScalingApi.h>
#pragma comment(lib, "Shcore.lib")

int main()
{
    SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);


    try
    {
        Game game(L"Katamari", 1920, 1080);

        //scale objects
        std::vector<ObjectDesc> objects =
        {
            ObjectDesc("assets/childrens_chair/childrens_chair.obj", L"assets/childrens_chair/childrens_chair_Albedo.png", PlacementType::Upright, 10, 0.02f, 0.025f, 0.02f, Material::Wood()),
            ObjectDesc("assets/creeper/CreeperZ.obj", L"assets/creeper/creeper.png", PlacementType::Upright, 10, 0.3f, 0.5f, 0.02f, Material::Organic()),
            ObjectDesc("assets/woman/obj.obj", L"assets/childrens_chair/childrens_chair_Normal.png", PlacementType::Upright, 12, 0.015f, 0.03f, 0.02f, Material::Metal()),
            ObjectDesc("assets/seashell/seashell_rapan-sl-0.obj", L"assets/seashell/rapana_diffuse.png", PlacementType::Flat, 12, 0.05f, 0.2f, -0.2f, Material::Ceramic()),
            ObjectDesc("assets/mouse/W_hlmaus.obj", L"assets/mouse/Feldmaus_Diffuse.png", PlacementType::Flat, 12, 0.001f, 0.003f, 0.02f, Material::Organic())
        };

        auto katamari = std::make_unique<KatamariComponent>(
            &game,
            objects,
            L"assets/ball/basketball/ball_basecolor.png",
            L"assets/ground/forrest_ground_01_diff_4k.jpg",
            L"shaders/Katamari.hlsl",
            20.0f                    
        );

        KatamariComponent* kPtr = katamari.get();
        game.AddComponent(std::move(katamari));

        std::cout << "Controls:\n"
            << "W/A/S/D - move the ball\n"
            << "Space - shoot a terrain-following point light\n"
            << "F1 - toggle shadow map HUD\n"
            << "F2 - toggle shadow HUD contour preview\n"
            << "</> - adjust shadow HUD exposure\n"
            << "Q/E - rotate camera left/right\n"
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
