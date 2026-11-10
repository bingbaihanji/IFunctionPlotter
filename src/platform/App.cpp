#include "platform/App.h"

#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "implot.h"

#include <dwmapi.h>
#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <vector>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd,
                                                            UINT msg,
                                                            WPARAM wParam,
                                                            LPARAM lParam);

namespace platform {
namespace {

constexpr wchar_t kWindowClass[] = L"IFunctionPlotterWindow";
constexpr wchar_t kWindowTitle[] = L"IFunctionPlotter";

constexpr DWORD kDwmUseImmersiveDarkMode = 20;
constexpr DWORD kDwmWindowCornerPreference = 33;
constexpr int kDwmWindowCornerRound = 2;

constexpr int kMinimumWindowWidth = 1080;
constexpr int kMinimumWindowHeight = 720;

constexpr DWORD kExcludeFromCapture = 0x00000011; // WDA_EXCLUDEFROMCAPTURE
constexpr DWORD kAffinityNone = 0x00000000;        // WDA_NONE

} // namespace

int App::Run(HINSTANCE instance) {
    ImGui_ImplWin32_EnableDpiAwareness();

    if (!InitializeWindow(instance)) {
        MessageBoxW(nullptr, L"窗口创建失败", kWindowTitle,
                    MB_OK | MB_ICONERROR);
        return 1;
    }
    if (!InitializeD3D()) {
        MessageBoxW(nullptr, L"Direct3D 11 初始化失败", kWindowTitle,
                    MB_OK | MB_ICONERROR);
        Shutdown();
        return 1;
    }
    if (!InitializeImGui()) {
        MessageBoxW(nullptr, L"ImGui 初始化失败", kWindowTitle,
                    MB_OK | MB_ICONERROR);
        Shutdown();
        return 1;
    }

    LARGE_INTEGER frequency{};
    LARGE_INTEGER previous{};
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&previous);

    MSG message{};
    while (running_) {
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                running_ = false;
                break;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (!running_) {
            break;
        }

        // Custom move/resize keeps the render loop alive so the glass can
        // resample the desktop every frame while the window is manipulated.
        UpdateWindowDrag();
        UpdateWindowResize();

        if (IsIconic(hwnd_) || renderTarget_ == nullptr) {
            Sleep(16);
            previous.QuadPart = 0;
            continue;
        }

        LARGE_INTEGER current{};
        QueryPerformanceCounter(&current);
        double delta = 0.0;
        if (previous.QuadPart != 0) {
            delta = static_cast<double>(current.QuadPart - previous.QuadPart) /
                    static_cast<double>(frequency.QuadPart);
        }
        previous = current;
        delta = std::min(delta, 0.05);
        elapsedSeconds_ += delta;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        glassPanels_.clear();
        ui::Fonts fonts{uiFont_, titleFont_, monoFont_};
        ui::DrawMainUi(plotterApp_, windowActions_, uiState_, glassPanels_,
                       fonts, static_cast<float>(elapsedSeconds_),
                       ImGui::GetIO().Framerate);

        UpdateDesktopCapture();

        render::GlassRenderOptions renderOptions;
        renderOptions.gradientEnabled = uiState_.appearance.gradientEnabled;
        if (desktopCapture_.Active() &&
            desktopCapture_.Snapshot() != nullptr) {
            POINT clientOrigin{0, 0};
            ClientToScreen(hwnd_, &clientOrigin);
            const RECT outputRect = desktopCapture_.OutputRect();
            renderOptions.desktopBackdrop = desktopCapture_.Snapshot();
            renderOptions.desktopWidth =
                static_cast<float>(desktopCapture_.Width());
            renderOptions.desktopHeight =
                static_cast<float>(desktopCapture_.Height());
            renderOptions.desktopOriginX =
                static_cast<float>(clientOrigin.x - outputRect.left);
            renderOptions.desktopOriginY =
                static_cast<float>(clientOrigin.y - outputRect.top);
        }

        ImGui::Render();

        const float clearColor[4] = {0.012f, 0.018f, 0.034f, 1.0f};
        context_->ClearRenderTargetView(renderTarget_.Get(), clearColor);
        glassRenderer_.Render(renderTarget_.Get(),
                              static_cast<float>(elapsedSeconds_),
                              glassPanels_, renderOptions);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        const HRESULT presentResult = swapChain_->Present(1, 0);
        if (presentResult == DXGI_ERROR_DEVICE_REMOVED ||
            presentResult == DXGI_ERROR_DEVICE_RESET) {
            break;
        }
    }

    Shutdown();
    return 0;
}

bool App::InitializeWindow(HINSTANCE instance) {
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = nullptr;
    windowClass.lpszClassName = kWindowClass;
    if (RegisterClassExW(&windowClass) == 0) {
        return false;
    }

    RECT workArea{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    int width = 1440;
    int height = 900;
    const int availableWidth = workArea.right - workArea.left;
    const int availableHeight = workArea.bottom - workArea.top;
    width = std::min(width, std::max(kMinimumWindowWidth, availableWidth - 80));
    height =
        std::min(height, std::max(kMinimumWindowHeight, availableHeight - 80));
    const int x = workArea.left + (availableWidth - width) / 2;
    const int y = workArea.top + (availableHeight - height) / 2;

    hwnd_ = CreateWindowExW(
        WS_EX_APPWINDOW, kWindowClass, kWindowTitle,
        WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU,
        x, y, width, height, nullptr, nullptr, instance, nullptr);
    if (hwnd_ == nullptr) {
        return false;
    }

    SetWindowLongPtrW(hwnd_, GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(this));

    const BOOL darkMode = TRUE;
    DwmSetWindowAttribute(hwnd_, kDwmUseImmersiveDarkMode, &darkMode,
                          sizeof(darkMode));
    const int cornerPreference = kDwmWindowCornerRound;
    DwmSetWindowAttribute(hwnd_, kDwmWindowCornerPreference, &cornerPreference,
                          sizeof(cornerPreference));

    windowActions_.context = hwnd_;
    windowActions_.minimize = MinimizeWindow;
    windowActions_.toggleMaximize = ToggleMaximizeWindow;
    windowActions_.close = CloseWindow;
    windowActions_.beginDrag = BeginWindowDrag;

    ShowWindow(hwnd_, SW_SHOWDEFAULT);
    UpdateWindow(hwnd_);
    return true;
}

bool App::InitializeD3D() {
    DXGI_SWAP_CHAIN_DESC swapChainDesc{};
    swapChainDesc.BufferCount = 2;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = hwnd_;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.Windowed = TRUE;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    const D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };

    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    HRESULT result = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, featureLevels,
        static_cast<UINT>(std::size(featureLevels)), D3D11_SDK_VERSION,
        &swapChainDesc, swapChain_.GetAddressOf(),
        device_.GetAddressOf(), nullptr, context_.GetAddressOf());
    if (FAILED(result) && (flags & D3D11_CREATE_DEVICE_DEBUG) != 0) {
        flags &= ~static_cast<UINT>(D3D11_CREATE_DEVICE_DEBUG);
        result = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, featureLevels,
            static_cast<UINT>(std::size(featureLevels)), D3D11_SDK_VERSION,
            &swapChainDesc, swapChain_.GetAddressOf(),
            device_.GetAddressOf(), nullptr, context_.GetAddressOf());
    }
    if (FAILED(result)) {
        return false;
    }

    CreateRenderTarget();
    if (renderTarget_ == nullptr) {
        return false;
    }

    RECT clientRect{};
    GetClientRect(hwnd_, &clientRect);
    glassRenderer_.Initialize(
        device_.Get(), context_.Get(),
        static_cast<UINT>(std::max<LONG>(1, clientRect.right - clientRect.left)),
        static_cast<UINT>(
            std::max<LONG>(1, clientRect.bottom - clientRect.top)));
    return true;
}

bool App::InitializeImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    imguiContextCreated_ = true;

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    ApplyStyle();
    LoadFonts();

    if (!ImGui_ImplWin32_Init(hwnd_)) {
        return false;
    }
    imguiWin32Initialized_ = true;
    if (!ImGui_ImplDX11_Init(device_.Get(), context_.Get())) {
        return false;
    }
    imguiDx11Initialized_ = true;

    ImPlotStyle& plotStyle = ImPlot::GetStyle();
    plotStyle.PlotBorderSize = 0.0f;
    plotStyle.MinorAlpha = 0.22f;
    plotStyle.PlotPadding = ImVec2(14.0f, 14.0f);
    plotStyle.LabelPadding = ImVec2(6.0f, 6.0f);
    plotStyle.LegendPadding = ImVec2(12.0f, 10.0f);
    plotStyle.LegendInnerPadding = ImVec2(8.0f, 6.0f);
    plotStyle.LegendSpacing = ImVec2(10.0f, 4.0f);
    return true;
}

void App::LoadFonts() {
    ImGuiIO& io = ImGui::GetIO();
    const ImWchar* chineseRanges =
        io.Fonts->GetGlyphRangesChineseSimplifiedCommon();

    ImFontConfig uiConfig;
    uiConfig.OversampleH = 2;
    uiConfig.OversampleV = 2;
    uiConfig.PixelSnapH = true;
    uiFont_ = io.Fonts->AddFontFromFileTTF(
        "C:\\Windows\\Fonts\\msyh.ttc", 19.0f, &uiConfig, chineseRanges);

    ImFontConfig titleConfig;
    titleConfig.OversampleH = 2;
    titleConfig.OversampleV = 2;
    titleConfig.PixelSnapH = true;
    titleFont_ = io.Fonts->AddFontFromFileTTF(
        "C:\\Windows\\Fonts\\msyhbd.ttc", 26.0f, &titleConfig, chineseRanges);

    ImFontConfig monoConfig;
    monoConfig.OversampleH = 1;
    monoConfig.OversampleV = 1;
    monoConfig.PixelSnapH = true;
    monoFont_ = io.Fonts->AddFontFromFileTTF(
        "C:\\Windows\\Fonts\\consola.ttf", 18.0f, &monoConfig);

    if (uiFont_ == nullptr) {
        uiFont_ = io.Fonts->AddFontDefault();
    }
    if (titleFont_ == nullptr) {
        titleFont_ = uiFont_;
    }
    if (monoFont_ == nullptr) {
        monoFont_ = uiFont_;
    }
    io.FontDefault = uiFont_;
}

void App::ApplyStyle() {
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowRounding = 0.0f;
    style.ChildRounding = 18.0f;
    style.FrameRounding = 10.0f;
    style.PopupRounding = 14.0f;
    style.ScrollbarRounding = 10.0f;
    style.GrabRounding = 8.0f;
    style.TabRounding = 10.0f;
    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.WindowPadding = ImVec2(18.0f, 16.0f);
    style.FramePadding = ImVec2(12.0f, 8.0f);
    style.ItemSpacing = ImVec2(10.0f, 10.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
    style.ScrollbarSize = 10.0f;
    style.GrabMinSize = 12.0f;
    style.WindowTitleAlign = ImVec2(0.0f, 0.5f);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(0.90f, 0.95f, 1.00f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.46f, 0.56f, 0.66f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.02f, 0.03f, 0.06f, 0.90f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.03f, 0.05f, 0.09f, 0.96f);
    colors[ImGuiCol_Border] = ImVec4(0.30f, 0.72f, 1.00f, 0.16f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_FrameBg] = ImVec4(1.0f, 1.0f, 1.0f, 0.055f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.35f, 0.85f, 1.00f, 0.10f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.35f, 0.85f, 1.00f, 0.14f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(1.0f, 1.0f, 1.0f, 0.16f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.30f, 0.90f, 1.00f, 0.28f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.30f, 0.90f, 1.00f, 0.40f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.22f, 0.88f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.22f, 0.88f, 1.00f, 0.90f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.65f, 0.98f, 1.00f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.10f, 0.20f, 0.30f, 0.55f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.18f, 0.62f, 0.90f, 0.45f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.20f, 0.78f, 1.00f, 0.60f);
    colors[ImGuiCol_Header] = ImVec4(0.20f, 0.70f, 1.00f, 0.16f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.20f, 0.70f, 1.00f, 0.25f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.20f, 0.70f, 1.00f, 0.35f);
    colors[ImGuiCol_Separator] = ImVec4(0.40f, 0.80f, 1.00f, 0.12f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.30f, 0.85f, 1.00f, 0.30f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.30f, 0.85f, 1.00f, 0.45f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.30f, 0.85f, 1.00f, 0.10f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.30f, 0.85f, 1.00f, 0.30f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.30f, 0.85f, 1.00f, 0.45f);
    colors[ImGuiCol_PlotLines] = ImVec4(0.22f, 0.88f, 1.00f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.55f, 0.85f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.22f, 0.88f, 1.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.55f, 0.85f, 1.00f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.20f, 0.80f, 1.00f, 0.25f);
    colors[ImGuiCol_NavHighlight] = ImVec4(0.30f, 0.90f, 1.00f, 0.60f);
}

void App::UpdateDesktopCapture() {
    const bool requested = uiState_.appearance.desktopBackdrop;

    if (requested) {
        if (!desktopCapture_.Active()) {
            if (!displayAffinitySet_) {
                if (SetWindowDisplayAffinity(hwnd_, kExcludeFromCapture) ==
                    FALSE) {
                    uiState_.appearance.desktopBackdrop = false;
                    uiState_.desktopCaptureActive = false;
                    uiState_.desktopStatus =
                        "当前系统不支持窗口捕获排除，桌面采样不可用";
                    return;
                }
                displayAffinitySet_ = true;
            }

            if (!desktopCapture_.Initialize(device_.Get(), context_.Get(),
                                            hwnd_)) {
                desktopCapture_.Shutdown();
                SetWindowDisplayAffinity(hwnd_, kAffinityNone);
                displayAffinitySet_ = false;
                uiState_.appearance.desktopBackdrop = false;
                uiState_.desktopCaptureActive = false;
                uiState_.desktopStatus =
                    "桌面采样初始化失败（可能被远程桌面或显卡占用）";
                return;
            }
            uiState_.desktopStatus = "桌面采样已开启";
        }

        const HMONITOR currentMonitor =
            MonitorFromWindow(hwnd_, MONITOR_DEFAULTTOPRIMARY);
        if (desktopCapture_.Monitor() != currentMonitor) {
            desktopCapture_.Shutdown();
            SetWindowDisplayAffinity(hwnd_, kAffinityNone);
            displayAffinitySet_ = false;
            uiState_.desktopCaptureActive = false;
            uiState_.desktopStatus = "窗口已切换显示器，正在重新初始化…";
            return;
        }

        desktopCapture_.Update();
        uiState_.desktopCaptureActive = desktopCapture_.Active();
        if (!desktopCapture_.Active()) {
            uiState_.desktopStatus = "桌面采样中断，正在重试…";
        }
    } else {
        if (desktopCapture_.Active() ||
            desktopCapture_.Monitor() != nullptr) {
            desktopCapture_.Shutdown();
        }
        if (displayAffinitySet_) {
            SetWindowDisplayAffinity(hwnd_, kAffinityNone);
            displayAffinitySet_ = false;
        }
        uiState_.desktopCaptureActive = false;
        uiState_.desktopStatus = "桌面采样未开启";
    }
}

void App::Shutdown() {
    desktopCapture_.Shutdown();
    if (displayAffinitySet_ && hwnd_ != nullptr) {
        SetWindowDisplayAffinity(hwnd_, kAffinityNone);
        displayAffinitySet_ = false;
    }

    if (imguiDx11Initialized_) {
        ImGui_ImplDX11_Shutdown();
        imguiDx11Initialized_ = false;
    }
    if (imguiWin32Initialized_) {
        ImGui_ImplWin32_Shutdown();
        imguiWin32Initialized_ = false;
    }
    if (imguiContextCreated_) {
        ImPlot::DestroyContext();
        ImGui::DestroyContext();
        imguiContextCreated_ = false;
    }

    glassRenderer_.Shutdown();
    renderTarget_.Reset();
    swapChain_.Reset();
    context_.Reset();
    device_.Reset();

    if (hwnd_ != nullptr) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

void App::ResizeSwapChain(UINT width, UINT height) {
    if (swapChain_ == nullptr || width == 0 || height == 0) {
        return;
    }

    context_->OMSetRenderTargets(0, nullptr, nullptr);
    renderTarget_.Reset();

    if (FAILED(swapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN,
                                         0))) {
        return;
    }
    CreateRenderTarget();
    glassRenderer_.Resize(width, height);
}

void App::CreateRenderTarget() {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
    if (FAILED(swapChain_->GetBuffer(0, IID_PPV_ARGS(
                                            backBuffer.GetAddressOf())))) {
        return;
    }
    device_->CreateRenderTargetView(backBuffer.Get(), nullptr,
                                    renderTarget_.GetAddressOf());
}

void App::MinimizeWindow(void* context) {
    HWND hwnd = static_cast<HWND>(context);
    if (App* app = reinterpret_cast<App*>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA))) {
        app->windowDragActive_ = false;
        app->resizeActive_ = false;
    }
    ShowWindow(hwnd, SW_MINIMIZE);
}

void App::ToggleMaximizeWindow(void* context) {
    HWND hwnd = static_cast<HWND>(context);
    if (App* app = reinterpret_cast<App*>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA))) {
        app->windowDragActive_ = false;
        app->resizeActive_ = false;
    }
    ShowWindow(hwnd, IsZoomed(hwnd) ? SW_RESTORE : SW_MAXIMIZE);
}

void App::CloseWindow(void* context) {
    HWND hwnd = static_cast<HWND>(context);
    if (App* app = reinterpret_cast<App*>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA))) {
        app->windowDragActive_ = false;
        app->resizeActive_ = false;
    }
    PostMessageW(hwnd, WM_CLOSE, 0, 0);
}

void App::BeginWindowDrag(void* context) {
    HWND hwnd = static_cast<HWND>(context);
    App* app = reinterpret_cast<App*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (app == nullptr) {
        return;
    }

    app->resizeActive_ = false;
    if (IsZoomed(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
    }
    app->windowDragActive_ = true;
    GetCursorPos(&app->windowDragLastCursor_);
}

void App::UpdateWindowDrag() {
    if (!windowDragActive_) {
        return;
    }
    if (GetForegroundWindow() != hwnd_ ||
        (GetAsyncKeyState(VK_LBUTTON) & 0x8000) == 0) {
        windowDragActive_ = false;
        return;
    }

    POINT cursor{};
    if (!GetCursorPos(&cursor)) {
        return;
    }
    const int deltaX = cursor.x - windowDragLastCursor_.x;
    const int deltaY = cursor.y - windowDragLastCursor_.y;
    windowDragLastCursor_ = cursor;
    if (deltaX == 0 && deltaY == 0) {
        return;
    }

    RECT windowRect{};
    if (!GetWindowRect(hwnd_, &windowRect)) {
        return;
    }
    SetWindowPos(hwnd_, nullptr, windowRect.left + deltaX,
                 windowRect.top + deltaY, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

bool App::BeginResize(WPARAM hitTest) {
    if (IsZoomed(hwnd_)) {
        return false;
    }

    int edge = 0;
    switch (hitTest) {
    case HTLEFT:
        edge = 1;
        break;
    case HTRIGHT:
        edge = 2;
        break;
    case HTTOP:
        edge = 4;
        break;
    case HTBOTTOM:
        edge = 8;
        break;
    case HTTOPLEFT:
        edge = 1 | 4;
        break;
    case HTTOPRIGHT:
        edge = 2 | 4;
        break;
    case HTBOTTOMLEFT:
        edge = 1 | 8;
        break;
    case HTBOTTOMRIGHT:
        edge = 2 | 8;
        break;
    default:
        return false;
    }

    windowDragActive_ = false;
    resizeActive_ = true;
    resizeEdge_ = edge;
    GetCursorPos(&resizeStartCursor_);
    GetWindowRect(hwnd_, &resizeStartRect_);
    return true;
}

void App::UpdateWindowResize() {
    if (!resizeActive_) {
        return;
    }
    if (GetForegroundWindow() != hwnd_ ||
        (GetAsyncKeyState(VK_LBUTTON) & 0x8000) == 0) {
        resizeActive_ = false;
        return;
    }

    POINT cursor{};
    if (!GetCursorPos(&cursor)) {
        return;
    }
    const int deltaX = cursor.x - resizeStartCursor_.x;
    const int deltaY = cursor.y - resizeStartCursor_.y;
    RECT rect = resizeStartRect_;
    if ((resizeEdge_ & 1) != 0) {
        rect.left += deltaX;
    }
    if ((resizeEdge_ & 2) != 0) {
        rect.right += deltaX;
    }
    if ((resizeEdge_ & 4) != 0) {
        rect.top += deltaY;
    }
    if ((resizeEdge_ & 8) != 0) {
        rect.bottom += deltaY;
    }

    if (rect.right - rect.left < kMinimumWindowWidth) {
        if ((resizeEdge_ & 1) != 0) {
            rect.left = rect.right - kMinimumWindowWidth;
        } else {
            rect.right = rect.left + kMinimumWindowWidth;
        }
    }
    if (rect.bottom - rect.top < kMinimumWindowHeight) {
        if ((resizeEdge_ & 4) != 0) {
            rect.top = rect.bottom - kMinimumWindowHeight;
        } else {
            rect.bottom = rect.top + kMinimumWindowHeight;
        }
    }
    if (rect.right <= rect.left || rect.bottom <= rect.top) {
        return;
    }

    SetWindowPos(hwnd_, nullptr, rect.left, rect.top, rect.right - rect.left,
                 rect.bottom - rect.top, SWP_NOZORDER | SWP_NOACTIVATE);
}

LRESULT CALLBACK App::WindowProc(HWND hwnd, UINT message, WPARAM wParam,
                                 LPARAM lParam) {
    App* app = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (message == WM_NCLBUTTONDOWN && app != nullptr) {
        if (app->BeginResize(wParam)) {
            return 0;
        }
    }

    if (ImGui_ImplWin32_WndProcHandler(hwnd, message, wParam, lParam) != 0) {
        return 1;
    }

    switch (message) {
    case WM_NCCALCSIZE:
        if (wParam != 0) {
            return 0;
        }
        break;
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize.x = kMinimumWindowWidth;
        info->ptMinTrackSize.y = kMinimumWindowHeight;

        HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO monitorInfo{};
        monitorInfo.cbSize = sizeof(monitorInfo);
        if (GetMonitorInfoW(monitor, &monitorInfo) != 0) {
            info->ptMaxPosition.x =
                monitorInfo.rcWork.left - monitorInfo.rcMonitor.left;
            info->ptMaxPosition.y =
                monitorInfo.rcWork.top - monitorInfo.rcMonitor.top;
            info->ptMaxSize.x =
                monitorInfo.rcWork.right - monitorInfo.rcWork.left;
            info->ptMaxSize.y =
                monitorInfo.rcWork.bottom - monitorInfo.rcWork.top;
        }
        return 0;
    }
    case WM_NCHITTEST: {
        if (IsZoomed(hwnd) != 0) {
            return HTCLIENT;
        }
        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        RECT rect{};
        GetWindowRect(hwnd, &rect);
        constexpr int border = 7;
        const bool left = point.x >= rect.left && point.x < rect.left + border;
        const bool right =
            point.x < rect.right && point.x >= rect.right - border;
        const bool top = point.y >= rect.top && point.y < rect.top + border;
        const bool bottom =
            point.y < rect.bottom && point.y >= rect.bottom - border;
        if (top && left) {
            return HTTOPLEFT;
        }
        if (top && right) {
            return HTTOPRIGHT;
        }
        if (bottom && left) {
            return HTBOTTOMLEFT;
        }
        if (bottom && right) {
            return HTBOTTOMRIGHT;
        }
        if (left) {
            return HTLEFT;
        }
        if (right) {
            return HTRIGHT;
        }
        if (top) {
            return HTTOP;
        }
        if (bottom) {
            return HTBOTTOM;
        }
        return HTCLIENT;
    }
    case WM_SIZE:
        if (app != nullptr && wParam != SIZE_MINIMIZED) {
            app->ResizeSwapChain(LOWORD(lParam), HIWORD(lParam));
        }
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        ValidateRect(hwnd, nullptr);
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (app != nullptr) {
            app->hwnd_ = nullptr;
        }
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

} // namespace platform
