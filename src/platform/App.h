#pragma once

#include "app/PlotterApp.h"
#include "platform/DesktopCapture.h"
#include "render/GlassRenderer.h"
#include "ui/MainWindow.h"

#include <windows.h>

#include <d3d11.h>
#include <dxgi.h>
#include <imgui.h>
#include <wrl/client.h>

#include <vector>

namespace platform {

class App {
public:
    int Run(HINSTANCE instance);

private:
    bool InitializeWindow(HINSTANCE instance);
    bool InitializeD3D();
    bool InitializeImGui();
    void LoadFonts();
    void ApplyStyle();
    void Shutdown();
    void ResizeSwapChain(UINT width, UINT height);
    void CreateRenderTarget();
    void UpdateDesktopCapture();
    bool CaptureStaticDesktopSnapshot();
    void UpdateWindowDrag();
    void UpdateWindowResize();
    bool BeginResize(WPARAM hitTest);

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam,
                                       LPARAM lParam);

    static void MinimizeWindow(void* context);
    static void ToggleMaximizeWindow(void* context);
    static void CloseWindow(void* context);
    static void BeginWindowDrag(void* context);

    HWND hwnd_ = nullptr;
    bool running_ = true;
    bool imguiContextCreated_ = false;
    bool imguiWin32Initialized_ = false;
    bool imguiDx11Initialized_ = false;

    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTarget_;

    render::GlassRenderer glassRenderer_;
    platform::DesktopCapture desktopCapture_;
    app::PlotterApp plotterApp_;
    ui::UiState uiState_;
    ui::WindowActions windowActions_;
    bool displayAffinitySet_ = false;

    bool windowDragActive_ = false;
    POINT windowDragLastCursor_{};
    bool resizeActive_ = false;
    int resizeEdge_ = 0;
    POINT resizeStartCursor_{};
    RECT resizeStartRect_{};

    ImFont* uiFont_ = nullptr;
    ImFont* titleFont_ = nullptr;
    ImFont* monoFont_ = nullptr;

    std::vector<render::GlassPanel> glassPanels_;
    double elapsedSeconds_ = 0.0;
};

} // namespace platform
