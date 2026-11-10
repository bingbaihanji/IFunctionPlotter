#pragma once

namespace render {

struct GlassPanel {
    float x = 0.0f;
    float y = 0.0f;
    float width = 100.0f;
    float height = 100.0f;
    float radius = 26.0f;
    float opacity = 1.0f;
    float tint[4] = {0.03f, 0.08f, 0.10f, 0.08f};
    float refractionHeight = 32.0f;
    float refractionAmount = -90.0f;
    float depthEffect = 0.70f;
    float chromaticAberration = 0.65f;
    float contrast = 0.0f;
    float rimGlow = 0.38f;
    float specular = 0.55f;
    float outerShadow = 0.42f;
    float shadowExtent = 22.0f;
};

} // namespace render
