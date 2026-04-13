#include "Game.h"
#include "KatamariComponent.h"
#include <stdexcept>
#include <iostream>

int main()
{
    try
    {
        Game game(L"Katamari", 1280, 720);

        // Уменьшаем масштабы объектов, чтобы они были меньше начального радиуса шара (1.0)
        // Теперь объекты будут иметь радиус примерно 0.2 - 0.7
        std::vector<ObjectDesc> objects =
        {
            { "assets/childrens_chair.obj",        10, 0.01f, 0.015f },
            { "assets/Creeper.obj",                10, 0.1f, 0.25f },
            { "assets/obj.obj",                    12, 0.015f, 0.03f },
            { "assets/seashell_rapan-sl-0.obj",    12, 0.1f, 0.3f },
        };

        auto katamari = std::make_unique<KatamariComponent>(
            &game,
            objects,
            L"shaders/Katamari.hlsl",   // путь к шейдеру относительно .exe
            80.0f                        // радиус игровой зоны
        );

        KatamariComponent* kPtr = katamari.get();
        game.AddComponent(std::move(katamari));

        std::cout << "Controls:\n"
            << "  W / S / A / D  or  Arrow keys  —  move the ball\n"
            << "  Q / E                           —  rotate camera left / right\n"
            << "  Esc                             —  quit\n\n";

        game.Run();

        std::cout << "Session ended.\n"
            << "  Absorbed : " << kPtr->AbsorbedCount() << '\n'
            << "  Ball r   : " << kPtr->BallRadius() << '\n';
    }
    catch (const std::exception& e)
    {
        std::cerr << "[FATAL] " << e.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}