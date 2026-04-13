#include "DisplayWin32.h"
#include "Game.h"
#include <stdexcept>
#include <iostream>

DisplayWin32::DisplayWin32(std::wstring title, int width, int height)
    : Name(std::move(title)), ClientWidth(width), ClientHeight(height)
{
    hInstance = GetModuleHandle(nullptr);
    RegisterWindowClass();
    CreateAppWindow();
}

DisplayWin32::~DisplayWin32()
{
    if (hWnd)      DestroyWindow(hWnd);
    if (hInstance) UnregisterClass(Name.c_str(), hInstance);
}

void DisplayWin32::RegisterWindowClass()
{
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc   = StaticWndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIcon(nullptr, IDI_WINLOGO);
    wc.hIconSm       = wc.hIcon;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszMenuName  = nullptr;
    wc.lpszClassName = Name.c_str();

    if (!RegisterClassEx(&wc))
        throw std::runtime_error("Failed to register window class.");
}

void DisplayWin32::CreateAppWindow()
{
    RECT rect = { 0, 0, static_cast<LONG>(ClientWidth), static_cast<LONG>(ClientHeight) };
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    int posX = (GetSystemMetrics(SM_CXSCREEN) - ClientWidth)  / 2;
    int posY = (GetSystemMetrics(SM_CYSCREEN) - ClientHeight) / 2;

    DWORD dwStyle = WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX | WS_THICKFRAME;

    hWnd = CreateWindowEx(
        WS_EX_APPWINDOW,
        Name.c_str(), Name.c_str(),
        dwStyle,
        posX, posY,
        rect.right  - rect.left,
        rect.bottom - rect.top,
        nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
        throw std::runtime_error("Failed to create window.");

    ShowWindow(hWnd, SW_SHOW);
    SetForegroundWindow(hWnd);
    SetFocus(hWnd);
    ShowCursor(TRUE);
}

LRESULT CALLBACK DisplayWin32::StaticWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    // Forward all messages to Game::MessageHandler for input processing.
    // Game stores itself via SetWindowLongPtr in Game::Initialize (see Game.cpp).
    Game* game = reinterpret_cast<Game*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (game)
    {
        LRESULT res = Game::MessageHandler(hwnd, msg, wp, lp);
        if (res) return res;
    }

    switch (msg)
    {
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) { PostQuitMessage(0); return 0; }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(hwnd, msg, wp, lp);
    }
}
