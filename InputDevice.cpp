#include "InputDevice.h"
#include "Game.h"
#include <iostream>

InputDevice::InputDevice(Game* inGame)
    : game(inGame)
{
    RAWINPUTDEVICE rid[2];

    //keyboard
    rid[0].usUsagePage = 0x01;   //Generic Desktop Controls
    rid[0].usUsage     = 0x06;   //Keyboard
    rid[0].dwFlags     = 0;
    rid[0].hwndTarget  = nullptr;

    //mouse
    rid[1].usUsagePage = 0x01;
    rid[1].usUsage     = 0x02;   //Mouse
    rid[1].dwFlags     = 0;
    rid[1].hwndTarget  = nullptr;

    if (!RegisterRawInputDevices(rid, 2, sizeof(RAWINPUTDEVICE)))
    {
        auto err = GetLastError();
        std::cerr << "Failed to register RawInput devices! Error: " << err << std::endl;
    }
}

void InputDevice::AddPressedKey(unsigned int keyCode)
{
    keys.insert(keyCode);
}

void InputDevice::RemovePressedKey(unsigned int keyCode)
{
    keys.erase(keyCode);
}

bool InputDevice::IsKeyDown(unsigned int keyCode) const
{
    return keys.count(keyCode) > 0;
}

void InputDevice::OnKeyDown(RAWKEYBOARD data)
{
    unsigned int vkey = data.VKey;
    bool isDown = !(data.Flags & RI_KEY_BREAK);

    if (isDown)
        AddPressedKey(vkey);
    else
        RemovePressedKey(vkey);
}

void InputDevice::OnMouseMove(RAWMOUSE data)
{
    if (data.usFlags == MOUSE_MOVE_RELATIVE)
    {
        MouseOffset.x += static_cast<float>(data.lLastX);
        MouseOffset.y += static_cast<float>(data.lLastY);
    }

	//get mouse absolute position
    POINT p;
    GetCursorPos(&p);
    MousePosition.x = static_cast<float>(p.x);
    MousePosition.y = static_cast<float>(p.y);
}
