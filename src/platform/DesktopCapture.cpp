#include "platform/DesktopCapture.h"

#include <algorithm>

namespace platform {

bool DesktopCapture::Initialize(ID3D11Device* device,
                                ID3D11DeviceContext* context, HWND window) {
    Shutdown();
    if (device == nullptr || context == nullptr || window == nullptr) {
        return false;
    }

    device_ = device;
    context_ = context;

    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    if (FAILED(device->QueryInterface(IID_PPV_ARGS(
            dxgiDevice.GetAddressOf())))) {
        Shutdown();
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    if (FAILED(dxgiDevice->GetAdapter(adapter.GetAddressOf()))) {
        Shutdown();
        return false;
    }

    monitor_ = MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY);

    Microsoft::WRL::ComPtr<IDXGIOutput> selectedOutput;
    RECT selectedRect{};
    for (UINT index = 0;; ++index) {
        Microsoft::WRL::ComPtr<IDXGIOutput> output;
        const HRESULT result =
            adapter->EnumOutputs(index, output.GetAddressOf());
        if (result == DXGI_ERROR_NOT_FOUND) {
            break;
        }
        if (FAILED(result)) {
            break;
        }

        DXGI_OUTPUT_DESC description{};
        if (FAILED(output->GetDesc(&description))) {
            continue;
        }

        if (selectedOutput.Get() == nullptr) {
            selectedOutput = output;
            selectedRect = description.DesktopCoordinates;
        }
        if (description.Monitor == monitor_) {
            selectedOutput = output;
            selectedRect = description.DesktopCoordinates;
            break;
        }
    }

    if (selectedOutput.Get() == nullptr) {
        Shutdown();
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGIOutput1> output1;
    if (FAILED(selectedOutput.As(&output1))) {
        Shutdown();
        return false;
    }
    if (FAILED(output1->DuplicateOutput(
            device, duplication_.GetAddressOf()))) {
        Shutdown();
        return false;
    }

    DXGI_OUTDUPL_DESC duplicationDescription{};
    duplication_->GetDesc(&duplicationDescription);
    width_ = duplicationDescription.ModeDesc.Width;
    height_ = duplicationDescription.ModeDesc.Height;
    if (width_ == 0 || height_ == 0) {
        Shutdown();
        return false;
    }

    DXGI_FORMAT format = duplicationDescription.ModeDesc.Format;
    if (format == DXGI_FORMAT_UNKNOWN) {
        format = DXGI_FORMAT_B8G8R8A8_UNORM;
    }
    if (!CreateSnapshot(format, width_, height_)) {
        Shutdown();
        return false;
    }

    outputRect_ = selectedRect;
    active_ = true;
    return true;
}

void DesktopCapture::Shutdown() {
    snapshotSrv_.Reset();
    snapshot_.Reset();
    duplication_.Reset();
    context_.Reset();
    device_.Reset();
    outputRect_ = RECT{};
    monitor_ = nullptr;
    width_ = 0;
    height_ = 0;
    active_ = false;
}

bool DesktopCapture::CreateSnapshot(DXGI_FORMAT format, UINT width,
                                    UINT height) {
    D3D11_TEXTURE2D_DESC description{};
    description.Width = width;
    description.Height = height;
    description.MipLevels = 1;
    description.ArraySize = 1;
    description.Format = format;
    description.SampleDesc.Count = 1;
    description.Usage = D3D11_USAGE_DEFAULT;
    description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(device_->CreateTexture2D(&description, nullptr,
                                        snapshot_.GetAddressOf()))) {
        return false;
    }
    return SUCCEEDED(device_->CreateShaderResourceView(
        snapshot_.Get(), nullptr, snapshotSrv_.GetAddressOf()));
}

bool DesktopCapture::Update() {
    if (!Active()) {
        return false;
    }

    DXGI_OUTDUPL_FRAME_INFO frameInfo{};
    Microsoft::WRL::ComPtr<IDXGIResource> resource;
    const HRESULT result =
        duplication_->AcquireNextFrame(0, &frameInfo, resource.GetAddressOf());
    if (result == DXGI_ERROR_WAIT_TIMEOUT) {
        return true;
    }
    if (FAILED(result)) {
        active_ = false;
        return false;
    }

    Microsoft::WRL::ComPtr<ID3D11Texture2D> desktopTexture;
    if (SUCCEEDED(resource.As(&desktopTexture))) {
        context_->CopyResource(snapshot_.Get(), desktopTexture.Get());
    }
    duplication_->ReleaseFrame();
    return true;
}

} // namespace platform
