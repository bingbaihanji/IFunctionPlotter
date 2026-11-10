#include "render/GlassRenderer.h"

#include "render/Shaders.h"

#include <d3dcompiler.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

namespace render {
namespace {

constexpr int kGaussianPairCapacity = 12;

template <typename T>
bool UpdateConstantBuffer(ID3D11DeviceContext* context, ID3D11Buffer* buffer,
                          const T& data) {
    D3D11_MAPPED_SUBRESOURCE mapped{};
    const HRESULT result =
        context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(result)) {
        return false;
    }
    std::memcpy(mapped.pData, &data, sizeof(T));
    context->Unmap(buffer, 0);
    return true;
}

bool CompileShader(const char* source, const char* entryPoint,
                   const char* target, Microsoft::WRL::ComPtr<ID3DBlob>& blob) {
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    Microsoft::WRL::ComPtr<ID3DBlob> errors;
    const HRESULT result = D3DCompile(
        source, std::strlen(source), nullptr, nullptr, nullptr, entryPoint,
        target, flags, 0, blob.GetAddressOf(), errors.GetAddressOf());
    if (SUCCEEDED(result)) {
        return true;
    }

    std::string message = "Shader compile failed: ";
    message += entryPoint;
    if (errors) {
        message += "\n";
        message.append(static_cast<const char*>(errors->GetBufferPointer()),
                       errors->GetBufferSize());
    }
    OutputDebugStringA(message.c_str());
    MessageBoxA(nullptr, message.c_str(), "IFunctionPlotter",
                MB_OK | MB_ICONERROR);
    blob.Reset();
    return false;
}

float ClampFinite(float value, float minimum, float maximum, float fallback) {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

} // namespace

struct GlassRenderer::SceneConstants {
    float resolution[2];
    float time;
    float gradientEnabled;
    float desktopOrigin[2];
    float desktopSize[2];
};

struct GlassRenderer::BlurConstants {
    float inverseSourceResolution[2];
    float direction[2];
    float centerWeight;
    float pairCount;
    float passKind;
    float padding;
    float pairWeights[kGaussianPairCapacity];
    float pairOffsets[kGaussianPairCapacity];
};

struct GlassRenderer::GlassConstants {
    float bounds[4];
    float radii[4];
    float refraction[4];
    float filter[4];
    float tint[4];
    float framebuffer[4];
    float deformation[4];
    float lighting[4];
    float light[4];
    float framebufferSize[2];
    float padding[2];
};

struct GlassRenderer::BlurPlan {
    float centerWeight = 1.0f;
    int pairCount = 0;
    float pairWeights[kGaussianPairCapacity] = {};
    float pairOffsets[kGaussianPairCapacity] = {};
};

GlassRenderer::BlurPlan GlassRenderer::MakeBlurPlan(float physicalRadius) {
    GlassRenderer::BlurPlan plan;
    if (!std::isfinite(physicalRadius) || physicalRadius <= 0.01f) {
        return plan;
    }

    const float sigma = std::max(0.5f, physicalRadius * 0.25f);
    const int support = std::max(1, static_cast<int>(std::ceil(sigma * 3.0f)));
    plan.pairCount =
        std::min((support + 1) / 2, kGaussianPairCapacity);

    float normalization = 1.0f;
    for (int pair = 0; pair < plan.pairCount; ++pair) {
        const int firstIndex = pair * 2 + 1;
        const int secondIndex = firstIndex + 1;
        const float firstWeight =
            std::exp(-0.5f * static_cast<float>(firstIndex * firstIndex) /
                     (sigma * sigma));
        const float secondWeight =
            secondIndex <= support
                ? std::exp(-0.5f * static_cast<float>(secondIndex * secondIndex) /
                           (sigma * sigma))
                : 0.0f;
        const float combinedWeight = firstWeight + secondWeight;
        plan.pairWeights[pair] = combinedWeight;
        plan.pairOffsets[pair] =
            (static_cast<float>(firstIndex) * firstWeight +
             static_cast<float>(secondIndex) * secondWeight) /
            combinedWeight;
        normalization += combinedWeight * 2.0f;
    }

    plan.centerWeight = 1.0f / normalization;
    for (int pair = 0; pair < plan.pairCount; ++pair) {
        plan.pairWeights[pair] /= normalization;
    }
    return plan;
}

bool GlassRenderer::Initialize(ID3D11Device* device,
                               ID3D11DeviceContext* context, UINT width,
                               UINT height) {
    Shutdown();
    if (device == nullptr || context == nullptr || width == 0 || height == 0) {
        return false;
    }

    device_ = device;
    context_ = context;
    width_ = width;
    height_ = height;

    if (!CreateShaders() || !CreateStates() || !CreateConstantBuffers() ||
        !CreateSizeDependentResources()) {
        Shutdown();
        return false;
    }
    ready_ = true;
    return true;
}

void GlassRenderer::Shutdown() {
    ReleaseSizeDependentResources();
    glassCB_.Reset();
    blurCB_.Reset();
    sceneCB_.Reset();
    rasterizer_.Reset();
    glassBlend_.Reset();
    sampler_.Reset();
    glassPS_.Reset();
    glassVS_.Reset();
    blurPS_.Reset();
    copyPS_.Reset();
    desktopScenePS_.Reset();
    scenePS_.Reset();
    fullscreenVS_.Reset();
    context_.Reset();
    device_.Reset();
    ready_ = false;
    width_ = 0;
    height_ = 0;
}

void GlassRenderer::Resize(UINT width, UINT height) {
    if (!ready_ || width == 0 || height == 0) {
        return;
    }
    if (width == width_ && height == height_) {
        return;
    }

    width_ = width;
    height_ = height;
    ReleaseSizeDependentResources();
    ready_ = CreateSizeDependentResources();
}

bool GlassRenderer::CreateShaders() {
    Microsoft::WRL::ComPtr<ID3DBlob> vertexBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> pixelBlob;

    if (!CompileShader(shaders::kFullscreenVS, "VSFullscreen", "vs_5_0",
                       vertexBlob)) {
        return false;
    }
    if (FAILED(device_->CreateVertexShader(vertexBlob->GetBufferPointer(),
                                           vertexBlob->GetBufferSize(), nullptr,
                                           fullscreenVS_.GetAddressOf()))) {
        return false;
    }

    if (!CompileShader(shaders::kScenePS, "PSBackground", "ps_5_0",
                       pixelBlob)) {
        return false;
    }
    if (FAILED(device_->CreatePixelShader(pixelBlob->GetBufferPointer(),
                                          pixelBlob->GetBufferSize(), nullptr,
                                          scenePS_.GetAddressOf()))) {
        return false;
    }

    if (!CompileShader(shaders::kDesktopScenePS, "PSDesktopScene", "ps_5_0",
                       pixelBlob)) {
        return false;
    }
    if (FAILED(device_->CreatePixelShader(pixelBlob->GetBufferPointer(),
                                          pixelBlob->GetBufferSize(), nullptr,
                                          desktopScenePS_.GetAddressOf()))) {
        return false;
    }

    if (!CompileShader(shaders::kCopyPS, "PSCopy", "ps_5_0", pixelBlob)) {
        return false;
    }
    if (FAILED(device_->CreatePixelShader(pixelBlob->GetBufferPointer(),
                                          pixelBlob->GetBufferSize(), nullptr,
                                          copyPS_.GetAddressOf()))) {
        return false;
    }

    if (!CompileShader(shaders::kBlurPS, "PSBlur", "ps_5_0", pixelBlob)) {
        return false;
    }
    if (FAILED(device_->CreatePixelShader(pixelBlob->GetBufferPointer(),
                                          pixelBlob->GetBufferSize(), nullptr,
                                          blurPS_.GetAddressOf()))) {
        return false;
    }

    if (!CompileShader(shaders::kGlassVS, "VSGlass", "vs_5_0", vertexBlob)) {
        return false;
    }
    if (FAILED(device_->CreateVertexShader(vertexBlob->GetBufferPointer(),
                                           vertexBlob->GetBufferSize(), nullptr,
                                           glassVS_.GetAddressOf()))) {
        return false;
    }

    if (!CompileShader(shaders::kGlassPS, "PSGlass", "ps_5_0", pixelBlob)) {
        return false;
    }
    if (FAILED(device_->CreatePixelShader(pixelBlob->GetBufferPointer(),
                                          pixelBlob->GetBufferSize(), nullptr,
                                          glassPS_.GetAddressOf()))) {
        return false;
    }
    return true;
}

bool GlassRenderer::CreateStates() {
    D3D11_SAMPLER_DESC samplerDesc{};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(device_->CreateSamplerState(&samplerDesc,
                                           sampler_.GetAddressOf()))) {
        return false;
    }

    D3D11_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask =
        D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(device_->CreateBlendState(&blendDesc,
                                         glassBlend_.GetAddressOf()))) {
        return false;
    }

    D3D11_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.FillMode = D3D11_FILL_SOLID;
    rasterizerDesc.CullMode = D3D11_CULL_NONE;
    rasterizerDesc.DepthClipEnable = TRUE;
    rasterizerDesc.ScissorEnable = FALSE;
    if (FAILED(device_->CreateRasterizerState(
            &rasterizerDesc, rasterizer_.GetAddressOf()))) {
        return false;
    }
    return true;
}

bool GlassRenderer::CreateConstantBuffers() {
    auto create = [this](UINT size, Microsoft::WRL::ComPtr<ID3D11Buffer>& buffer) {
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = size;
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        return SUCCEEDED(device_->CreateBuffer(&desc, nullptr,
                                               buffer.GetAddressOf()));
    };

    return create(static_cast<UINT>(sizeof(SceneConstants)),
                  sceneCB_) &&
           create(static_cast<UINT>(sizeof(BlurConstants)), blurCB_) &&
           create(static_cast<UINT>(sizeof(GlassConstants)), glassCB_);
}

bool GlassRenderer::CreateSizeDependentResources() {
    auto createTexture = [this](UINT textureWidth, UINT textureHeight,
                                Microsoft::WRL::ComPtr<ID3D11Texture2D>& texture,
                                Microsoft::WRL::ComPtr<ID3D11RenderTargetView>& rtv,
                                Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv) {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = textureWidth;
        desc.Height = textureHeight;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags =
            D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        if (FAILED(device_->CreateTexture2D(&desc, nullptr,
                                            texture.GetAddressOf()))) {
            return false;
        }
        if (FAILED(device_->CreateRenderTargetView(
                texture.Get(), nullptr, rtv.GetAddressOf()))) {
            return false;
        }
        return SUCCEEDED(device_->CreateShaderResourceView(
            texture.Get(), nullptr, srv.GetAddressOf()));
    };

    if (!createTexture(width_, height_, sceneTexture_, sceneRTV_, sceneSRV_)) {
        return false;
    }

    const UINT blurWidth = std::max(1u, width_ / 2);
    const UINT blurHeight = std::max(1u, height_ / 2);
    if (!createTexture(blurWidth, blurHeight, blurTextureA_, blurRTVA_,
                       blurSRVA_)) {
        return false;
    }
    return createTexture(blurWidth, blurHeight, blurTextureB_, blurRTVB_,
                         blurSRVB_);
}

void GlassRenderer::ReleaseSizeDependentResources() {
    blurSRVB_.Reset();
    blurRTVB_.Reset();
    blurTextureB_.Reset();
    blurSRVA_.Reset();
    blurRTVA_.Reset();
    blurTextureA_.Reset();
    sceneSRV_.Reset();
    sceneRTV_.Reset();
    sceneTexture_.Reset();
}

void GlassRenderer::SetFullViewport() {
    D3D11_VIEWPORT viewport{};
    viewport.Width = static_cast<float>(width_);
    viewport.Height = static_cast<float>(height_);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    context_->RSSetViewports(1, &viewport);
}

void GlassRenderer::BindFullscreenPass() {
    context_->IASetInputLayout(nullptr);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->VSSetShader(fullscreenVS_.Get(), nullptr, 0);
    context_->RSSetState(rasterizer_.Get());
    context_->OMSetBlendState(nullptr, nullptr, 0xffffffff);
}

void GlassRenderer::RenderBlurPass(
    ID3D11ShaderResourceView* source, ID3D11RenderTargetView* target,
    UINT targetWidth, UINT targetHeight, UINT sourceWidth, UINT sourceHeight,
    const BlurPlan& plan, bool boxDownsample, float directionX,
    float directionY) {
    D3D11_VIEWPORT viewport{};
    viewport.Width = static_cast<float>(targetWidth);
    viewport.Height = static_cast<float>(targetHeight);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    context_->RSSetViewports(1, &viewport);
    context_->OMSetRenderTargets(1, &target, nullptr);
    context_->OMSetBlendState(nullptr, nullptr, 0xffffffff);
    context_->IASetInputLayout(nullptr);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->VSSetShader(fullscreenVS_.Get(), nullptr, 0);
    context_->PSSetShader(blurPS_.Get(), nullptr, 0);

    BlurConstants constants{};
    constants.inverseSourceResolution[0] =
        1.0f / static_cast<float>(std::max(1u, sourceWidth));
    constants.inverseSourceResolution[1] =
        1.0f / static_cast<float>(std::max(1u, sourceHeight));
    constants.direction[0] = directionX;
    constants.direction[1] = directionY;
    constants.centerWeight = plan.centerWeight;
    constants.pairCount = static_cast<float>(plan.pairCount);
    constants.passKind = boxDownsample ? 0.0f : 1.0f;
    std::memcpy(constants.pairWeights, plan.pairWeights,
                sizeof(constants.pairWeights));
    std::memcpy(constants.pairOffsets, plan.pairOffsets,
                sizeof(constants.pairOffsets));
    UpdateConstantBuffer(context_.Get(), blurCB_.Get(), constants);

    ID3D11Buffer* constantBuffer = blurCB_.Get();
    ID3D11ShaderResourceView* resource = source;
    context_->PSSetConstantBuffers(0, 1, &constantBuffer);
    context_->PSSetShaderResources(0, 1, &resource);
    context_->PSSetSamplers(0, 1, sampler_.GetAddressOf());
    context_->Draw(3, 0);
}

void GlassRenderer::RenderPanel(ID3D11RenderTargetView* backbuffer,
                                const GlassPanel& panel) {
    const float width = panel.width;
    const float height = panel.height;
    if (!(width > 0.0f) || !(height > 0.0f)) {
        return;
    }

    const float maximumRadius = std::min(width, height) * 0.5f;
    const float shadowExtent = ClampFinite(panel.shadowExtent, 0.0f, 64.0f, 18.0f);

    GlassConstants constants{};
    constants.bounds[0] = panel.x;
    constants.bounds[1] = panel.y;
    constants.bounds[2] = panel.x + width;
    constants.bounds[3] = panel.y + height;

    const float radius = ClampFinite(panel.radius, 0.0f, maximumRadius, 26.0f);
    constants.radii[0] = radius;
    constants.radii[1] = radius;
    constants.radii[2] = radius;
    constants.radii[3] = radius;

    constants.refraction[0] =
        ClampFinite(panel.refractionHeight, 0.001f,
                    std::max(0.001f, maximumRadius), 28.0f);
    constants.refraction[1] =
        ClampFinite(panel.refractionAmount, -960.0f, 960.0f, -80.0f);
    constants.refraction[2] =
        ClampFinite(panel.depthEffect, -2.0f, 2.0f, 0.60f);
    constants.refraction[3] =
        ClampFinite(panel.chromaticAberration, 0.0f, 1.0f, 0.55f);

    constants.filter[0] = ClampFinite(panel.contrast, -1.0f, 1.0f, -0.05f);
    constants.filter[1] = 0.0f;
    constants.filter[2] = 1.0f;
    constants.filter[3] = ClampFinite(panel.opacity, 0.0f, 1.0f, 1.0f);

    for (int index = 0; index < 4; ++index) {
        constants.tint[index] = ClampFinite(panel.tint[index], 0.0f, 1.0f,
                                            index == 3 ? 0.10f : 0.05f);
    }

    constants.framebuffer[0] = static_cast<float>(width_);
    constants.framebuffer[1] = static_cast<float>(height_);
    constants.framebuffer[2] = 0.0f;
    constants.framebuffer[3] = 0.0f;

    constants.deformation[0] = 0.0f;
    constants.deformation[1] = 0.0f;
    constants.deformation[2] = 1.0f;
    constants.deformation[3] = 0.0f;

    constants.lighting[0] = 0.12f;
    constants.lighting[1] = ClampFinite(panel.rimGlow, 0.0f, 1.0f, 0.30f);
    constants.lighting[2] = ClampFinite(panel.specular, 0.0f, 2.0f, 0.45f);
    constants.lighting[3] =
        ClampFinite(panel.outerShadow, 0.0f, 1.0f, 0.35f);

    constants.light[0] = -0.7071f;
    constants.light[1] = -0.7071f;
    constants.light[2] = shadowExtent;
    constants.light[3] = 28.0f;

    constants.framebufferSize[0] = static_cast<float>(width_);
    constants.framebufferSize[1] = static_cast<float>(height_);

    UpdateConstantBuffer(context_.Get(), glassCB_.Get(), constants);

    ID3D11Buffer* constantBuffer = glassCB_.Get();
    context_->VSSetConstantBuffers(0, 1, &constantBuffer);
    context_->PSSetConstantBuffers(0, 1, &constantBuffer);
    context_->OMSetRenderTargets(1, &backbuffer, nullptr);
    context_->Draw(6, 0);
}

void GlassRenderer::Render(ID3D11RenderTargetView* backbuffer,
                           float timeSeconds,
                           const std::vector<GlassPanel>& panels,
                           const GlassRenderOptions& options) {
    if (!ready_ || backbuffer == nullptr || width_ == 0 || height_ == 0) {
        return;
    }

    ID3D11DeviceContext* context = context_.Get();
    ID3D11ShaderResourceView* nullResources[2] = {nullptr, nullptr};

    const bool desktopMode = options.desktopBackdrop != nullptr &&
                             options.desktopWidth > 0.0f &&
                             options.desktopHeight > 0.0f;

    // 1. Scene texture: live desktop behind the window or procedural gradient.
    SetFullViewport();
    ID3D11RenderTargetView* sceneTarget = sceneRTV_.Get();
    context->OMSetRenderTargets(1, &sceneTarget, nullptr);
    const float sceneClear[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    context->ClearRenderTargetView(sceneTarget, sceneClear);
    BindFullscreenPass();
    context->PSSetShader(desktopMode ? desktopScenePS_.Get() : scenePS_.Get(),
                         nullptr, 0);

    SceneConstants sceneConstants{};
    sceneConstants.resolution[0] = static_cast<float>(width_);
    sceneConstants.resolution[1] = static_cast<float>(height_);
    sceneConstants.time = timeSeconds;
    sceneConstants.gradientEnabled = options.gradientEnabled ? 1.0f : 0.0f;
    sceneConstants.desktopOrigin[0] = options.desktopOriginX;
    sceneConstants.desktopOrigin[1] = options.desktopOriginY;
    sceneConstants.desktopSize[0] = options.desktopWidth;
    sceneConstants.desktopSize[1] = options.desktopHeight;
    UpdateConstantBuffer(context, sceneCB_.Get(), sceneConstants);

    ID3D11Buffer* sceneBuffer = sceneCB_.Get();
    context->PSSetConstantBuffers(0, 1, &sceneBuffer);
    if (desktopMode) {
        ID3D11ShaderResourceView* desktop = options.desktopBackdrop;
        context->PSSetShaderResources(0, 1, &desktop);
        context->PSSetSamplers(0, 1, sampler_.GetAddressOf());
    } else {
        context->PSSetShaderResources(0, 2, nullResources);
    }
    context->Draw(3, 0);

    // 2. Downsample and separable Gaussian blur at half resolution.
    const BlurPlan blurPlan = MakeBlurPlan(28.0f);
    const UINT blurWidth = std::max(1u, width_ / 2);
    const UINT blurHeight = std::max(1u, height_ / 2);

    RenderBlurPass(sceneSRV_.Get(), blurRTVA_.Get(), blurWidth, blurHeight,
                   width_, height_, blurPlan, true, 0.0f, 0.0f);
    RenderBlurPass(blurSRVA_.Get(), blurRTVB_.Get(), blurWidth, blurHeight,
                   blurWidth, blurHeight, blurPlan, false, 1.0f, 0.0f);
    RenderBlurPass(blurSRVB_.Get(), blurRTVA_.Get(), blurWidth, blurHeight,
                   blurWidth, blurHeight, blurPlan, false, 0.0f, 1.0f);

    // 3. Scene copy into the back buffer (opaque base layer).
    SetFullViewport();
    context->OMSetRenderTargets(1, &backbuffer, nullptr);
    BindFullscreenPass();
    context->PSSetShader(copyPS_.Get(), nullptr, 0);
    ID3D11ShaderResourceView* sceneResource = sceneSRV_.Get();
    context->PSSetShaderResources(0, 1, &sceneResource);
    context->PSSetSamplers(0, 1, sampler_.GetAddressOf());
    context->Draw(3, 0);

    // 4. Glass panels.
    SetFullViewport();
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(glassVS_.Get(), nullptr, 0);
    context->PSSetShader(glassPS_.Get(), nullptr, 0);
    context->RSSetState(rasterizer_.Get());
    context->OMSetBlendState(glassBlend_.Get(), nullptr, 0xffffffff);

    ID3D11ShaderResourceView* glassResources[2] = {blurSRVA_.Get(),
                                                   sceneSRV_.Get()};
    context->PSSetShaderResources(0, 2, glassResources);
    context->PSSetSamplers(0, 1, sampler_.GetAddressOf());

    for (const GlassPanel& panel : panels) {
        RenderPanel(backbuffer, panel);
    }

    // 5. Restore defaults for the ImGui backend.
    context->OMSetBlendState(nullptr, nullptr, 0xffffffff);
    context->RSSetState(nullptr);
    context->VSSetShader(nullptr, nullptr, 0);
    context->PSSetShader(nullptr, nullptr, 0);
    context->PSSetShaderResources(0, 2, nullResources);
    ID3D11Buffer* nullBuffer = nullptr;
    context->VSSetConstantBuffers(0, 1, &nullBuffer);
    context->PSSetConstantBuffers(0, 1, &nullBuffer);
}

} // namespace render
