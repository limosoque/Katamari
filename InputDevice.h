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
    bool leftMouseDown = false;
    bool rightMouseDown = false;
    bool leftMousePressed = false;
    bool rightMousePressed = false;

public:
    InputDevice(Game* inGame);

    void AddPressedKey(unsigned int keyCode);
    void RemovePressedKey(unsigned int keyCode);

    bool IsKeyDown(unsigned int keyCode) const;
    bool IsLeftMouseDown() const { return leftMouseDown; }
    bool IsRightMouseDown() const { return rightMouseDown; }
    bool WasLeftMousePressed() const { return leftMousePressed; }
    bool WasRightMousePressed() const { return rightMousePressed; }

    void OnKeyDown(RAWKEYBOARD data);
    void OnMouseMove(RAWMOUSE data);

    DirectX::SimpleMath::Vector2 GetMousePosition() const { return MousePosition; }
    DirectX::SimpleMath::Vector2 GetMouseOffset() const { return MouseOffset; }

    void ResetMouseOffset() {
        MouseOffset = DirectX::SimpleMath::Vector2(0.f, 0.f); 
        leftMousePressed = false;
        rightMousePressed = false;
    }
};
