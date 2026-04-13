#define NOMINMAX

#include "Game.h"
#include "DisplayWin32.h"
#include "InputDevice.h"
#include "GameComponent.h"
#include <stdexcept>
#include <iostream>
#include <vector>

using Microsoft::WRL::ComPtr;

// ─── Construction / Destruction ───────────────────────────────────────────────

Game::Game(std::wstring title, int width, int height)
    : appTitle(std::move(title))
    , screenWidth(width)
    , screenHeight(height)
{
    Initialize();
}

Game::~Game()
{
    DestroyResources();
}

void Game::Initialize()
{
    Display = std::make_unique<DisplayWin32>(appTitle, screenWidth, screenHeight);
    Input   = std::make_unique<InputDevice>(this);

    // Store 'this' in the window user data so StaticWndProc can forward messages.
    SetWindowLongPtr(Display->hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    RegisterRawInput();
    PrepareResources();
    CreateBackBuffer();
}

// ─── Raw Input registration ───────────────────────────────────────────────────

void Game::RegisterRawInput()
{
    RAWINPUTDEVICE rid[2] = {};

    // Keyboard
    rid[0].usUsagePage = 0x01;   // HID_USAGE_PAGE_GENERIC
    rid[0].usUsage     = 0x06;   // HID_USAGE_GENERIC_KEYBOARD
    rid[0].dwFlags     = 0;
    rid[0].hwndTarget  = Display->hWnd;

    // Mouse
    rid[1].usUsagePage = 0x01;   // HID_USAGE_PAGE_GENERIC
    rid[1].usUsage     = 0x02;   // HID_USAGE_GENERIC_MOUSE
    rid[1].dwFlags     = 0;
    rid[1].hwndTarget  = Display->hWnd;

    if (!RegisterRawInputDevices(rid, 2, sizeof(RAWINPUTDEVICE)))
        throw std::runtime_error("RegisterRawInputDevices failed.");
}

// ─── D3D device + swap chain ──────────────────────────────────────────────────

void Game::PrepareResources()
{
    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount                        = 2;
    sd.BufferDesc.Width                   = static_cast<UINT>(screenWidth);
    sd.BufferDesc.Height                  = static_cast<UINT>(screenHeight);
    sd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferDesc.ScanlineOrdering        = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    sd.BufferDesc.Scaling                 = DXGI_MODE_SCALING_UNSPECIFIED;
    sd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow                       = Display->hWnd;
    sd.Windowed                           = TRUE;
    sd.SwapEffect                         = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.Flags                              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.SampleDesc.Count                   = 1;
    sd.SampleDesc.Quality                 = 0;

    UINT createFlags = D3D11_CREATE_DEVICE_DEBUG;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createFlags,
        featureLevels,
        static_cast<UINT>(std::size(featureLevels)),
        D3D11_SDK_VERSION,
        &sd,
        SwapChain.GetAddressOf(),
        Device.GetAddressOf(),
        nullptr,
        Context.GetAddressOf());

    if (FAILED(hr))
        throw std::runtime_error("D3D11CreateDeviceAndSwapChain failed.");
}

// ─── Back buffer + depth buffer ──────────────────────────────────────────────

void Game::CreateBackBuffer()
{
    // Render target view
    ComPtr<ID3D11Texture2D> backBuffer;
    HRESULT hr = SwapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()));
    if (FAILED(hr)) throw std::runtime_error("GetBuffer failed.");

    hr = Device->CreateRenderTargetView(backBuffer.Get(), nullptr, RenderView.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("CreateRenderTargetView failed.");

    // Depth / stencil buffer
    D3D11_TEXTURE2D_DESC dsDesc = {};
    dsDesc.Width              = static_cast<UINT>(screenWidth);
    dsDesc.Height             = static_cast<UINT>(screenHeight);
    dsDesc.MipLevels          = 1;
    dsDesc.ArraySize          = 1;
    dsDesc.Format             = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsDesc.SampleDesc.Count   = 1;
    dsDesc.SampleDesc.Quality = 0;
    dsDesc.Usage              = D3D11_USAGE_DEFAULT;
    dsDesc.BindFlags          = D3D11_BIND_DEPTH_STENCIL;

    ComPtr<ID3D11Texture2D> depthTex;
    hr = Device->CreateTexture2D(&dsDesc, nullptr, depthTex.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("CreateTexture2D (depth) failed.");

    hr = Device->CreateDepthStencilView(depthTex.Get(), nullptr, DepthView.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("CreateDepthStencilView failed.");
}

void Game::RestoreTargets()
{
    ID3D11RenderTargetView* rtv = RenderView.Get();
    Context->OMSetRenderTargets(1, &rtv, DepthView.Get());
}

// ─── Component management ─────────────────────────────────────────────────────

void Game::AddComponent(std::unique_ptr<GameComponent> component)
{
    component->Initialize();
    Components.push_back(std::move(component));
}

// ─── Main loop ────────────────────────────────────────────────────────────────

void Game::Run()
{
    PrevTime = std::chrono::steady_clock::now();
    unsigned int frameCount = 0;
    float        fpsTimer   = 0.0f;

    MSG msg = {};
    while (!isExiting)
    {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (msg.message == WM_QUIT) break;

        auto  now       = std::chrono::steady_clock::now();
        float deltaTime = std::chrono::duration_cast<std::chrono::microseconds>(now - PrevTime).count() / 1'000'000.0f;
        PrevTime = now;

        deltaTime = std::min(deltaTime, 0.05f);

        TotalTime += deltaTime;
        ++frameCount;

        fpsTimer += deltaTime;
        if (fpsTimer >= 1.0f)
        {
            float fps  = static_cast<float>(frameCount) / fpsTimer;
            fpsTimer   = 0.0f;
            frameCount = 0;

            WCHAR text[128];
            swprintf_s(text, L"%s  |  FPS: %.1f", appTitle.c_str(), fps);
            SetWindowText(Display->hWnd, text);
        }

        PrepareFrame();
        UpdateInternal(deltaTime);
        Draw();
        EndFrame();
    }
}

void Game::Exit()
{
    isExiting = true;
}

// ─── Per-frame pipeline ───────────────────────────────────────────────────────

void Game::PrepareFrame()
{
    Context->ClearState();

    D3D11_VIEWPORT vp = {};
    vp.Width    = static_cast<float>(screenWidth);
    vp.Height   = static_cast<float>(screenHeight);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    Context->RSSetViewports(1, &vp);

    float color[4] = { 0.53f, 0.81f, 0.98f, 1.0f };  // sky blue
    Context->OMSetRenderTargets(1, RenderView.GetAddressOf(), DepthView.Get());
    Context->ClearRenderTargetView(RenderView.Get(), color);
    Context->ClearDepthStencilView(DepthView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}

void Game::UpdateInternal(float dt)
{
    for (auto& comp : Components)
        comp->Update(dt);
}

void Game::Draw()
{
    for (auto& comp : Components)
        comp->Draw();
}

void Game::EndFrame()
{
    // Reset per-frame mouse delta accumulator before next frame.
    Input->ResetMouseOffset();

    Context->OMSetRenderTargets(0, nullptr, nullptr);
    SwapChain->Present(1, 0);
}

// ─── Cleanup ──────────────────────────────────────────────────────────────────

void Game::DestroyResources()
{
    for (auto& comp : Components)
        comp->DestroyResources();
    Components.clear();

    RenderView.Reset();
    DepthView.Reset();
    Context.Reset();
    SwapChain.Reset();
    Device.Reset();
}

// ─── Static message handler ───────────────────────────────────────────────────
// Called by DisplayWin32::StaticWndProc.
// Uses Raw Input API (WM_INPUT) — no WM_KEYDOWN/WM_MOUSEMOVE needed.

LRESULT CALLBACK Game::MessageHandler(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    Game* game = reinterpret_cast<Game*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (!game || !game->Input) return 0;

    if (msg != WM_INPUT) return 0;

    // Step 1: query packet size.
    UINT size = 0;
    GetRawInputData(reinterpret_cast<HRAWINPUT>(lp),
                    RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));
    if (size == 0) return 0;

    // Step 2: read the packet.
    std::vector<BYTE> buf(size);
    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lp),
                        RID_INPUT, buf.data(), &size, sizeof(RAWINPUTHEADER)) != size)
        return 0;

    const RAWINPUT* raw = reinterpret_cast<const RAWINPUT*>(buf.data());

    if (raw->header.dwType == RIM_TYPEKEYBOARD)
    {
        const RAWKEYBOARD& kb = raw->data.keyboard;
        // RI_KEY_MAKE  = key pressed
        // RI_KEY_BREAK = key released
        if (kb.Flags == RI_KEY_MAKE)
            game->Input->OnKeyDown(kb);
        else if (kb.Flags == RI_KEY_BREAK)
            game->Input->RemovePressedKey(kb.VKey);
    }
    else if (raw->header.dwType == RIM_TYPEMOUSE)
    {
        game->Input->OnMouseMove(raw->data.mouse);
    }

    return 0;
}
