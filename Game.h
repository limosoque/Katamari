#pragma once
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <memory>
#include <chrono>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")

class GameComponent;
class DisplayWin32;
class InputDevice;

/// Central application class: owns the D3D device, swap chain, and component list.
class Game
{
public:
    // D3D resources — public so components can access them directly.
    Microsoft::WRL::ComPtr<ID3D11Device>            Device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext>     Context;
    Microsoft::WRL::ComPtr<IDXGISwapChain>          SwapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>  RenderView;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView>  DepthView;

    bool ScreenResized = false;

    // Sub-systems
    std::unique_ptr<DisplayWin32> Display;
    std::unique_ptr<InputDevice>  Input;

    // Timing
    float TotalTime = 0.0f;
    float StartTime = 0.0f;

    explicit Game(std::wstring title = L"My3DApp", int width = 800, int height = 800);
    ~Game();

    Game(const Game&)            = delete;
    Game& operator=(const Game&) = delete;

    /// Register a component — Game takes ownership.
    void AddComponent(std::unique_ptr<GameComponent> component);

    /// Main loop entry point.
    void Run();

    /// Request orderly shutdown.
    void Exit();

    /// Called by DisplayWin32::StaticWndProc — handles WM_INPUT (Raw Input API).
    static LRESULT CALLBACK MessageHandler(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

private:
    std::vector<std::unique_ptr<GameComponent>> Components;
    bool isExiting = false;

    std::wstring appTitle;
    int          screenWidth;
    int          screenHeight;

    std::chrono::time_point<std::chrono::steady_clock> PrevTime;

    void Initialize();
    void RegisterRawInput();   // registers keyboard + mouse as Raw Input devices
    void PrepareResources();
    void CreateBackBuffer();
    void RestoreTargets();
    void PrepareFrame();
    void UpdateInternal(float dt);
    void Draw();
    void EndFrame();
    void DestroyResources();
};
