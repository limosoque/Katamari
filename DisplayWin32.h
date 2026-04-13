#pragma once
#include <windows.h>
#include <string>

class Game;

/// Encapsulates Win32 window creation and message routing.
class DisplayWin32
{
public:
    int         ClientWidth = 800;
    int         ClientHeight = 800;
    HINSTANCE   hInstance = nullptr;
    HWND        hWnd = nullptr;
    WNDCLASSEX  wc = {};
    std::wstring Name;

    explicit DisplayWin32(std::wstring title, int width = 800, int height = 800);
    ~DisplayWin32();

    // Non-copyable
    DisplayWin32(const DisplayWin32&) = delete;
    DisplayWin32& operator=(const DisplayWin32&) = delete;

    static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

private:
    void RegisterWindowClass();
    void CreateAppWindow();
};
