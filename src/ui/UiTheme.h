#pragma once

#include <imgui.h>

namespace ui {

struct Fonts {
    ImFont* ui = nullptr;
    ImFont* title = nullptr;
    ImFont* mono = nullptr;
};

namespace theme {

inline const ImVec4 kAccent = ImVec4(0.24f, 0.88f, 1.00f, 1.00f);
inline const ImVec4 kAccentSoft = ImVec4(0.24f, 0.88f, 1.00f, 0.55f);
inline const ImVec4 kAccentMagenta = ImVec4(1.00f, 0.40f, 0.85f, 1.00f);
inline const ImVec4 kText = ImVec4(0.90f, 0.95f, 1.00f, 1.00f);
inline const ImVec4 kTextDim = ImVec4(0.53f, 0.64f, 0.75f, 1.00f);
inline const ImVec4 kDanger = ImVec4(1.00f, 0.38f, 0.44f, 1.00f);

inline ImU32 Color(const ImVec4& color, float alphaMultiplier = 1.0f) {
    return ImGui::GetColorU32(ImVec4(color.x, color.y, color.z,
                                     color.w * alphaMultiplier));
}

inline ImU32 Rgba(int red, int green, int blue, int alpha) {
    return IM_COL32(red, green, blue, alpha);
}

} // namespace theme
} // namespace ui
