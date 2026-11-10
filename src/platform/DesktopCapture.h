#pragma once

#include <windows.h>

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

namespace platform {

// Captures the desktop output that contains the application window. The
// application window itself must be excluded from capture by the caller so
// that the liquid glass samples what is actually behind the window.
class DesktopCapture {
public:
    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context,
                    HWND window);
    void Shutdown();

    // Returns false only when the duplication was lost. A timeout (no desktop
    // change) is not a failure: the previous snapshot stays valid.
    bool Update();

    // Waits up to timeoutMilliseconds for one fresh desktop frame. Used by the
    // recording-compatible mode to grab a single static snapshot.
    bool CaptureFrame(UINT timeoutMilliseconds);

    bool Active() const {
        return active_ && duplication_ != nullptr && snapshotSrv_ != nullptr;
    }

    bool HasSnapshot() const { return hasSnapshot_; }
    ID3D11ShaderResourceView* Snapshot() const { return snapshotSrv_.Get(); }
    UINT Width() const { return width_; }
    UINT Height() const { return height_; }
    const RECT& OutputRect() const { return outputRect_; }
    HMONITOR Monitor() const { return monitor_; }

private:
    bool CreateSnapshot(DXGI_FORMAT format, UINT width, UINT height);

    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<IDXGIOutputDuplication> duplication_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> snapshot_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> snapshotSrv_;

    RECT outputRect_{};
    HMONITOR monitor_ = nullptr;
    UINT width_ = 0;
    UINT height_ = 0;
    bool active_ = false;
    bool hasSnapshot_ = false;
};

} // namespace platform
