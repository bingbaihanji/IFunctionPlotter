#pragma once

// Direct3D 11 port of the liquid glass shader from imgui-liquid-glass-main
// (MIT, adapted from AndroidLiquidGlassView (c) 2025 QmDeve, MIT).
// See third_party/THIRD_PARTY_NOTICES.md.

namespace render::shaders {

inline constexpr const char* kFullscreenVS = R"hlsl(
struct FullscreenVSOut {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

FullscreenVSOut VSFullscreen(uint vertexId : SV_VertexID) {
    float2 corner = float2((vertexId << 1) & 2, vertexId & 2);
    FullscreenVSOut output;
    output.position = float4(corner * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    output.uv = corner;
    return output;
}
)hlsl";

inline constexpr const char* kScenePS = R"hlsl(
cbuffer SceneConstants : register(b0) {
    float2 uResolution;
    float uTime;
    float uGradientEnabled;
    float2 uDesktopOrigin;
    float2 uDesktopSize;
};

struct FullscreenVSOut {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float Hash21(float2 p) {
    float3 p3 = frac(float3(p.x, p.y, p.x) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}

float3 TechBackground(float2 uv) {
    float2 resolution = max(uResolution, float2(1.0, 1.0));
    float2 p = uv * resolution;
    float2 q = (p - 0.5 * resolution) / resolution.y;

    const float3 gradientTop = float3(0.0, 0.3529, 0.2941);     // #005a4b
    const float3 gradientBottom = float3(0.0039, 0.4510, 0.5412); // #01738a
    float gradientMix = saturate(uv.y * 0.82 + uv.x * 0.18);
    float3 color = uGradientEnabled > 0.5
        ? lerp(gradientTop, gradientBottom, gradientMix)
        : float3(0.006, 0.020, 0.024);

    color += float3(0.0, 0.055, 0.065) * (1.0 - saturate(length(q)));

    float t = uTime * 0.05;
    float2 c1 = float2(-0.55 + 0.20 * sin(t * 1.3), -0.20 + 0.15 * cos(t * 1.7));
    float2 c2 = float2(0.62 + 0.16 * cos(t * 1.1), 0.22 + 0.18 * sin(t * 1.4));
    color += float3(0.0, 0.55, 0.45) * 0.13 * exp(-dot(q - c1, q - c1) * 6.0);
    color += float3(0.05, 0.55, 0.65) * 0.11 * exp(-dot(q - c2, q - c2) * 7.0);

    float2 gridUv = p / 48.0;
    float2 gridWidth = max(fwidth(gridUv), float2(1e-5, 1e-5));
    float2 gridDistance = abs(frac(gridUv - 0.5) - 0.5) / gridWidth;
    float gridLine = 1.0 - min(min(gridDistance.x, gridDistance.y), 1.0);
    color += float3(0.10, 0.75, 0.68) * gridLine * 0.045;

    float2 majorUv = p / 192.0;
    float2 majorWidth = max(fwidth(majorUv), float2(1e-5, 1e-5));
    float2 majorDistance = abs(frac(majorUv - 0.5) - 0.5) / majorWidth;
    float majorLine = 1.0 - min(min(majorDistance.x, majorDistance.y), 1.0);
    color += float3(0.10, 0.75, 0.85) * majorLine * 0.035;

    float scan = exp(-abs(frac(uv.y - uTime * 0.02) - 0.5) * 46.0);
    color += float3(0.0, 0.18, 0.20) * scan * 0.035;

    float vignette = smoothstep(1.20, 0.20, length(q));
    color *= lerp(0.78, 1.0, vignette);
    color += (Hash21(p) - 0.5) / 255.0;
    return color;
}

float4 PSBackground(FullscreenVSOut input) : SV_Target {
    return float4(TechBackground(input.uv), 1.0);
}
)hlsl";

inline constexpr const char* kDesktopScenePS = R"hlsl(
cbuffer SceneConstants : register(b0) {
    float2 uResolution;
    float uTime;
    float uGradientEnabled;
    float2 uDesktopOrigin;
    float2 uDesktopSize;
};

Texture2D<float4> uDesktop : register(t0);
SamplerState uLinearClamp : register(s0);

struct FullscreenVSOut {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 PSDesktopScene(FullscreenVSOut input) : SV_Target {
    float2 pixel = input.uv * uResolution;
    float2 desktopSize = max(uDesktopSize, float2(1.0, 1.0));
    float2 uv = (pixel + uDesktopOrigin) / desktopSize;
    if (any(uv < 0.0) || any(uv > 1.0)) {
        return float4(0.006, 0.020, 0.024, 1.0);
    }
    float2 halfTexel = 0.5 / desktopSize;
    uv = clamp(uv, halfTexel, 1.0 - halfTexel);
    return float4(uDesktop.Sample(uLinearClamp, uv).rgb, 1.0);
}
)hlsl";

inline constexpr const char* kCopyPS = R"hlsl(
Texture2D<float4> uScene : register(t0);
SamplerState uLinearClamp : register(s0);

struct FullscreenVSOut {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 PSCopy(FullscreenVSOut input) : SV_Target {
    return float4(uScene.Sample(uLinearClamp, input.uv).rgb, 1.0);
}
)hlsl";

inline constexpr const char* kBlurPS = R"hlsl(
cbuffer BlurConstants : register(b0) {
    float2 uInverseSourceResolution;
    float2 uDirection;
    float uCenterWeight;
    float uPairCount;
    float uPassKind;
    float uPadding;
    float4 uPairWeights[3];
    float4 uPairOffsets[3];
};

Texture2D<float4> uSource : register(t0);
SamplerState uLinearClamp : register(s0);

struct FullscreenVSOut {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 PSBlur(FullscreenVSOut input) : SV_Target {
    if (uPassKind < 0.5) {
        float2 halfTexel = uInverseSourceResolution * 0.5;
        float4 color = uSource.Sample(uLinearClamp, input.uv + float2(-halfTexel.x, -halfTexel.y));
        color += uSource.Sample(uLinearClamp, input.uv + float2( halfTexel.x, -halfTexel.y));
        color += uSource.Sample(uLinearClamp, input.uv + float2(-halfTexel.x,  halfTexel.y));
        color += uSource.Sample(uLinearClamp, input.uv + float2( halfTexel.x,  halfTexel.y));
        return color * 0.25;
    }

    float4 color = uSource.Sample(uLinearClamp, input.uv) * uCenterWeight;
    int count = int(uPairCount + 0.5);
    [unroll]
    for (int pair = 0; pair < 12; ++pair) {
        if (pair >= count) {
            break;
        }
        float weight = uPairWeights[pair >> 2][pair & 3];
        float offset = uPairOffsets[pair >> 2][pair & 3];
        float2 step = uDirection * uInverseSourceResolution * offset;
        color += uSource.Sample(uLinearClamp, input.uv + step) * weight;
        color += uSource.Sample(uLinearClamp, input.uv - step) * weight;
    }
    return color;
}
)hlsl";

inline constexpr const char* kGlassVS = R"hlsl(
cbuffer GlassConstants : register(b0) {
    float4 uBounds;
    float4 uRadii;
    float4 uRefraction;
    float4 uFilter;
    float4 uTint;
    float4 uFramebuffer;
    float4 uDeformation;
    float4 uLighting;
    float4 uLight;
    float2 uFramebufferSize;
    float2 uPadding;
};

struct GlassVSOut {
    float4 position : SV_Position;
    float2 pixel : TEXCOORD0;
};

static const float2 kCorners[6] = {
    float2(0.0, 0.0), float2(1.0, 0.0), float2(1.0, 1.0),
    float2(0.0, 0.0), float2(1.0, 1.0), float2(0.0, 1.0)
};

GlassVSOut VSGlass(uint vertexId : SV_VertexID) {
    float2 local = kCorners[vertexId];
    float drawExtent = max(uLight.z, 1.0);
    float4 drawBounds = uBounds + float4(-drawExtent, -drawExtent, drawExtent, drawExtent);
    float2 position = lerp(drawBounds.xy, drawBounds.zw, local);
    float2 ndc = position / uFramebufferSize * 2.0 - 1.0;
    ndc.y = -ndc.y;

    GlassVSOut output;
    output.position = float4(ndc, 0.0, 1.0);
    output.pixel = position;
    return output;
}
)hlsl";

inline constexpr const char* kGlassPS = R"hlsl(
cbuffer GlassConstants : register(b0) {
    float4 uBounds;
    float4 uRadii;
    float4 uRefraction;
    float4 uFilter;
    float4 uTint;
    float4 uFramebuffer;
    float4 uDeformation;
    float4 uLighting;
    float4 uLight;
    float2 uFramebufferSize;
    float2 uPadding;
};

Texture2D<float4> uBackdrop : register(t0);
Texture2D<float4> uRawBackdrop : register(t1);
SamplerState uLinearClamp : register(s0);

struct GlassVSOut {
    float4 position : SV_Position;
    float2 pixel : TEXCOORD0;
};

float RadiusAt(float2 p, float4 radii) {
    if (p.x >= 0.0) {
        return p.y <= 0.0 ? radii.y : radii.z;
    }
    return p.y <= 0.0 ? radii.x : radii.w;
}

float SdRoundedRect(float2 p, float2 halfSize, float radius) {
    float2 corner = abs(p) - (halfSize - float2(radius, radius));
    float outside = length(max(corner, float2(0.0, 0.0))) - radius;
    float inside = min(max(corner.x, corner.y), 0.0);
    return outside + inside;
}

float SdCircle(float2 p, float radius) {
    return length(p) - radius;
}

float AntialiasedCoverage(float signedDistance) {
    float edgeWidth = max(fwidth(signedDistance), 0.5);
    return 1.0 - smoothstep(-edgeWidth, edgeWidth, signedDistance);
}

float PixelCoverage(float signedDistance, float2 gradient) {
    float gradientLength = length(gradient);
    if (gradientLength < 1e-4) {
        return signedDistance < 0.0 ? 1.0 : 0.0;
    }
    float2 normal = abs(gradient) / gradientLength;
    float major = max(normal.x, normal.y);
    float minor = min(normal.x, normal.y);
    float offset = -signedDistance / gradientLength;
    float extent = 0.5 * (major + minor);
    if (offset <= -extent) {
        return 0.0;
    }
    if (offset >= extent) {
        return 1.0;
    }
    if (minor < 1e-4 || abs(offset) <= 0.5 * (major - minor)) {
        return clamp(0.5 + offset / major, 0.0, 1.0);
    }
    float corner = extent - abs(offset);
    float area = corner * corner / (2.0 * major * minor);
    return offset < 0.0 ? area : 1.0 - area;
}

float SafeSign(float value) {
    return value < 0.0 ? -1.0 : 1.0;
}

float2 SafeNormalize(float2 value, float2 fallback) {
    float valueLength = length(value);
    return valueLength > 0.001 ? value / valueLength : fallback;
}

float2 JellyHalfSize(float2 halfSize, float2 deformation) {
    float2 inset = min(abs(deformation) * 0.46, float2(0.18, 0.18));
    return max(halfSize * (float2(1.0, 1.0) - inset), float2(1.0, 1.0));
}

float2 JellyCoordinate(float2 p, float2 halfSize, float2 deformation) {
    float2 normalized = clamp(p / max(halfSize, float2(1.0, 1.0)),
                              float2(-1.0, -1.0), float2(1.0, 1.0));
    float2 profile = float2(1.0 - normalized.y * normalized.y,
                            1.0 - normalized.x * normalized.x);
    return p - deformation * halfSize * profile * 0.72;
}

float2 GradSdRoundedRect(float2 p, float2 halfSize, float radius) {
    float2 corner = abs(p) - (halfSize - float2(radius, radius));
    if (corner.x >= 0.0 || corner.y >= 0.0) {
        float2 outside = max(corner, float2(0.0, 0.0));
        float outsideLength = length(outside);
        if (outsideLength > 0.001) {
            return sign(p) * (outside / outsideLength);
        }
        float useX = step(corner.y, corner.x);
        return float2(useX * SafeSign(p.x), (1.0 - useX) * SafeSign(p.y));
    }
    float useX = step(corner.y, corner.x);
    return sign(p) * float2(useX, 1.0 - useX);
}

float2 GradSdCircle(float2 p) {
    return SafeNormalize(p, float2(1.0, 0.0));
}

float CircleMap(float value) {
    float mapped = clamp(value, -1.0, 1.0);
    return 1.0 - sqrt(max(0.0, 1.0 - mapped * mapped));
}

float3 ToLinearSrgb(float3 color) {
    float3 low = color / 12.92;
    float3 high = pow(max((color + 0.055) / 1.055, 0.0), 2.4);
    float3 mask = step(color, 0.04045);
    return lerp(high, low, mask);
}

float3 FromLinearSrgb(float3 color) {
    float3 low = color * 12.92;
    float3 high = 1.055 * pow(max(color, 0.0), 1.0 / 2.4) - 0.055;
    float3 mask = step(color, 0.0031308);
    return lerp(high, low, mask);
}

float4 FilterLiquidGlassColor(float4 color, float contrast, float whitePoint,
                              float chromaMultiplier, float4 tint) {
    const float3 rgbToY = float3(0.2126, 0.7152, 0.0722);
    float3 linearColor = ToLinearSrgb(color.rgb);
    float luminance = dot(linearColor, rgbToY);
    color.rgb = FromLinearSrgb(lerp(float3(luminance, luminance, luminance),
                                   linearColor,
                                   float3(chromaMultiplier, chromaMultiplier,
                                          chromaMultiplier)));
    float3 target = whitePoint > 0.0 ? float3(1.0, 1.0, 1.0) : float3(0.0, 0.0, 0.0);
    color.rgb = lerp(color.rgb, target, abs(whitePoint));
    color.rgb = (color.rgb - 0.5) * (1.0 + contrast) + 0.5;
    color.rgb = lerp(color.rgb, tint.rgb, tint.a);
    return color;
}

float2 GlassLightDirection(float4 light) {
    return SafeNormalize(light.xy, float2(-0.7071, -0.7071));
}

float RimIntegral(float t, float h) {
    float u = 1.0 - sqrt(clamp(t / h, 0.0, 1.0));
    float u6 = u * u * u * u * u * u;
    return 2.0 * h * u6 * (u / 7.0 - 1.0 / 6.0);
}

float FilteredRim(float signedDistance, float edgeWidth, float2 distanceGradient) {
    float h = max(edgeWidth, 0.001);
    float halfPixelWidth = 0.5 * max(abs(distanceGradient.x) + abs(distanceGradient.y), 1e-4);
    float t0 = max(-signedDistance - halfPixelWidth, 0.0);
    float t1 = max(-signedDistance + halfPixelWidth, 0.0);
    if (t1 <= 0.0) {
        return 0.0;
    }
    if (t1 - t0 < 1e-4) {
        float u = 1.0 - sqrt(clamp(t0 / h, 0.0, 1.0));
        return u * u * u * u * u;
    }
    return (RimIntegral(t1, h) - RimIntegral(t0, h)) / (t1 - t0);
}

float3 ApplyGlassLighting(float3 color, float signedDistance, float2 normal,
                          float edgeWidth, float2 distanceGradient,
                          float4 lighting, float4 light) {
    float rim = FilteredRim(signedDistance, edgeWidth, distanceGradient);
    float2 lightDirection = GlassLightDirection(light);

    float shadowFacing = max(dot(normal, -lightDirection), 0.0);
    color *= 1.0 - lighting.x * rim * shadowFacing;
    color += lighting.y * rim * (float3(1.0, 1.0, 1.0) - color);

    float specular = pow(max(dot(normal, lightDirection), 0.0), max(light.w, 1.0));
    color += lighting.z * rim * specular * (float3(1.0, 1.0, 1.0) - color);
    return color;
}

float GlassOuterShadow(float signedDistance, float2 normal, float4 lighting, float4 light) {
    float extent = max(light.z, 0.001);
    float falloff = 1.0 - smoothstep(0.0, extent, signedDistance);
    float direction = max(dot(normal, -GlassLightDirection(light)), 0.0);
    return lighting.w * falloff * lerp(0.35, 1.0, direction);
}

float4 SampleContent(float2 pixelCoordinate) {
    float2 halfTexel = 0.5 / uFramebufferSize;
    float2 uv = clamp(pixelCoordinate / uFramebufferSize, halfTexel,
                      float2(1.0, 1.0) - halfTexel);
    float blurMix = clamp(uDeformation.z, 0.0, 1.0);
    if (blurMix >= 0.999) {
        return uBackdrop.Sample(uLinearClamp, uv);
    }
    if (blurMix <= 0.001) {
        return uRawBackdrop.Sample(uLinearClamp, uv);
    }
    return lerp(uRawBackdrop.Sample(uLinearClamp, uv),
                uBackdrop.Sample(uLinearClamp, uv), blurMix);
}

float4 ApplyColorFilter(float4 color) {
    color = FilterLiquidGlassColor(color, uFilter.x, uFilter.y, uFilter.z, uTint);
    color.a = uFilter.w;
    return color;
}

float4 PSGlass(GlassVSOut input) : SV_Target {
    float2 size = max(uBounds.zw - uBounds.xy, float2(1.0, 1.0));
    float2 halfSize = size * 0.5;
    float2 centeredCoordinate = input.pixel - (uBounds.xy + halfSize);
    bool isCircle = uFramebuffer.w > 0.5;
    float2 shapeHalfSize = JellyHalfSize(halfSize, uDeformation.xy);
    float2 shapeCoordinate = JellyCoordinate(centeredCoordinate, halfSize, uDeformation.xy);
    float radius = min(RadiusAt(shapeCoordinate, uRadii), min(shapeHalfSize.x, shapeHalfSize.y));
    float signedDistance = isCircle
        ? SdCircle(shapeCoordinate, min(shapeHalfSize.x, shapeHalfSize.y))
        : SdRoundedRect(shapeCoordinate, shapeHalfSize, radius);
    float smoothRadius = min(max(radius * 1.5, 30.0), min(shapeHalfSize.x, shapeHalfSize.y));
    float2 shapeGradient = isCircle
        ? GradSdCircle(shapeCoordinate)
        : GradSdRoundedRect(shapeCoordinate, shapeHalfSize, smoothRadius);

    float2 distanceGradient = float2(ddx(signedDistance), ddy(signedDistance));
    float coverage = min(size.x, size.y) <= 128.0
        ? PixelCoverage(signedDistance, distanceGradient)
        : AntialiasedCoverage(signedDistance);
    float shadow = (uLighting.w > 0.0 && signedDistance < uLight.z)
        ? GlassOuterShadow(max(signedDistance, 0.0), shapeGradient, uLighting, uLight)
        : 0.0;
    if (coverage <= 0.0) {
        if (shadow <= 0.001) {
            discard;
        }
        return float4(0.0, 0.0, 0.0, shadow * uFilter.w);
    }

    float shadeDistance = min(signedDistance, -0.001);
    float refractionHeight = max(uRefraction.x, 0.001);
    float4 color;
    if (-shadeDistance >= refractionHeight) {
        color = SampleContent(input.pixel);
    } else {
        float depth = 1.0 - (-shadeDistance / refractionHeight);
        float displacement = CircleMap(depth) * uRefraction.y;
        float2 depthGradient = SafeNormalize(shapeCoordinate, shapeGradient);
        float2 gradient = SafeNormalize(shapeGradient + uRefraction.z * depthGradient, shapeGradient);
        float2 refractedCoordinate = input.pixel + displacement * gradient;
        float dispersionIntensity = uRefraction.w *
            ((shapeCoordinate.x * shapeCoordinate.y) /
             max(shapeHalfSize.x * shapeHalfSize.y, 0.001));
        float2 dispersedCoordinate = displacement * gradient * dispersionIntensity;

        color = float4(0.0, 0.0, 0.0, 0.0);
        float4 red = SampleContent(refractedCoordinate + dispersedCoordinate);
        color.r += red.r / 3.5;
        color.a += red.a / 7.0;
        float4 orange = SampleContent(refractedCoordinate + dispersedCoordinate * (2.0 / 3.0));
        color.r += orange.r / 3.5;
        color.g += orange.g / 7.0;
        color.a += orange.a / 7.0;
        float4 yellow = SampleContent(refractedCoordinate + dispersedCoordinate * (1.0 / 3.0));
        color.r += yellow.r / 3.5;
        color.g += yellow.g / 3.5;
        color.a += yellow.a / 7.0;
        float4 green = SampleContent(refractedCoordinate);
        color.g += green.g / 3.5;
        color.a += green.a / 7.0;
        float4 cyan = SampleContent(refractedCoordinate - dispersedCoordinate * (1.0 / 3.0));
        color.g += cyan.g / 3.5;
        color.b += cyan.b / 3.0;
        color.a += cyan.a / 7.0;
        float4 blue = SampleContent(refractedCoordinate - dispersedCoordinate * (2.0 / 3.0));
        color.b += blue.b / 3.0;
        color.a += blue.a / 7.0;
        float4 purple = SampleContent(refractedCoordinate - dispersedCoordinate);
        color.r += purple.r / 7.0;
        color.b += purple.b / 3.0;
        color.a += purple.a / 7.0;
    }

    color = ApplyColorFilter(color);
    color.rgb = ApplyGlassLighting(color.rgb, signedDistance, shapeGradient,
                                   refractionHeight, distanceGradient,
                                   uLighting, uLight);
    float glassAlpha = color.a * coverage;
    float alpha = glassAlpha + shadow * uFilter.w * (1.0 - coverage);
    return float4(color.rgb * (glassAlpha / max(alpha, 1e-6)), alpha);
}
)hlsl";

} // namespace render::shaders
