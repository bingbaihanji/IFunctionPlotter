#pragma once

#include "render/GlassPanel.h"

#include <d3d11.h>
#include <wrl/client.h>

#include <vector>

namespace render {

struct GlassRenderOptions {
    bool gradientEnabled = true;
    ID3D11ShaderResourceView* desktopBackdrop = nullptr;
    float desktopWidth = 0.0f;
    float desktopHeight = 0.0f;
    float desktopOriginX = 0.0f;
    float desktopOriginY = 0.0f;
};

class GlassRenderer {
public:
    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context,
                    UINT width, UINT height);
    void Shutdown();
    void Resize(UINT width, UINT height);
    void Render(ID3D11RenderTargetView* backbuffer, float timeSeconds,
                const std::vector<GlassPanel>& panels,
                const GlassRenderOptions& options = {});

private:
    struct GlassConstants;
    struct BlurConstants;
    struct SceneConstants;
    struct BlurPlan;

    static BlurPlan MakeBlurPlan(float physicalRadius);
    bool CreateShaders();
    bool CreateStates();
    bool CreateConstantBuffers();
    bool CreateSizeDependentResources();
    void ReleaseSizeDependentResources();
    void SetFullViewport();
    void BindFullscreenPass();
    void RenderBlurPass(ID3D11ShaderResourceView* source,
                        ID3D11RenderTargetView* target, UINT targetWidth,
                        UINT targetHeight, UINT sourceWidth, UINT sourceHeight,
                        const BlurPlan& plan, bool boxDownsample,
                        float directionX, float directionY);
    void RenderPanel(ID3D11RenderTargetView* backbuffer,
                     const GlassPanel& panel);

    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    UINT width_ = 0;
    UINT height_ = 0;
    bool ready_ = false;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> fullscreenVS_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> scenePS_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> desktopScenePS_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> copyPS_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> blurPS_;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> glassVS_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> glassPS_;

    Microsoft::WRL::ComPtr<ID3D11Buffer> sceneCB_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> blurCB_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> glassCB_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler_;
    Microsoft::WRL::ComPtr<ID3D11BlendState> glassBlend_;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer_;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> sceneTexture_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> sceneRTV_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> sceneSRV_;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> blurTextureA_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> blurRTVA_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> blurSRVA_;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> blurTextureB_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> blurRTVB_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> blurSRVB_;
};

} // namespace render
