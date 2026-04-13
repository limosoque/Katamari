#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <WinUser.h>
#include <unordered_set>
#include <SimpleMath.h>

class Game;

class InputDevice
{
public:
    enum class KeyState
    {
        None,
        Pressed,
        Released
    };

    struct RawMouseEventArgs
    {
        int X;
        int Y;
        int ButtonFlags;
    };

private:
    Game* game;
    std::unordered_set<unsigned int> keys;

    DirectX::SimpleMath::Vector2 MousePosition;
    DirectX::SimpleMath::Vector2 MouseOffset;

public:
    InputDevice(Game* inGame);

    void AddPressedKey(unsigned int keyCode);
    void RemovePressedKey(unsigned int keyCode);

    bool IsKeyDown(unsigned int keyCode) const;

    void OnKeyDown(RAWKEYBOARD data);
    void OnMouseMove(RAWMOUSE data);

    DirectX::SimpleMath::Vector2 GetMousePosition() const { return MousePosition; }
    DirectX::SimpleMath::Vector2 GetMouseOffset() const { return MouseOffset; }

    void ResetMouseOffset() {
        MouseOffset = DirectX::SimpleMath::Vector2(0.f, 0.f); 
    }
};
