#pragma once

class Game;

class GameComponent
{
public:
    Game* game;

    GameComponent(Game* inGame) : game(inGame) {}
    virtual ~GameComponent() {}

    virtual void DestroyResources() = 0;
    virtual void Draw() = 0;
    virtual void Initialize() = 0;
    virtual void Reload() {}
    virtual void Update(float deltaTime) = 0;
};
