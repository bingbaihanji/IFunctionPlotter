#include "ui/UiWidgets.h"

#include "ui/UiTheme.h"

namespace ui {

bool ToggleSwitch(const char* id, bool* value, const ImVec2& size) {
    ImGui::InvisibleButton(id, size);
    const bool changed =
        ImGui::IsItemClicked(ImGuiMouseButton_Left) && value != nullptr;
    if (changed) {
        *value = !*value;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    const float radius = size.y * 0.5f;
    const bool enabled = value != nullptr && *value;

    drawList->AddRectFilled(
        min, max,
        enabled ? theme::Rgba(34, 200, 238, 150)
                : theme::Rgba(255, 255, 255, 24),
        radius);
    drawList->AddRect(min, max,
                      enabled ? theme::Rgba(160, 245, 255, 140)
                              : theme::Rgba(255, 255, 255, 35),
                      radius, 0, 1.0f);

    const ImVec2 knobCenter(enabled ? max.x - radius : min.x + radius,
                            (min.y + max.y) * 0.5f);
    drawList->AddCircleFilled(knobCenter, radius - 3.0f,
                              theme::Rgba(242, 252, 255, 245));
    if (enabled) {
        drawList->AddCircle(knobCenter, radius - 1.5f,
                            theme::Rgba(180, 248, 255, 100), 0, 1.0f);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    return changed;
}

void DrawSettingsIcon(ImDrawList* drawList, const ImVec2& center, float size,
                      ImU32 color) {
    const float half = size * 0.5f;
    const float spacing = size * 0.28f;
    for (int index = 0; index < 3; ++index) {
        const float y = center.y + (static_cast<float>(index) - 1.0f) * spacing;
        drawList->AddLine(ImVec2(center.x - half, y),
                          ImVec2(center.x + half, y), color, 1.2f);
        const float knobOffset =
            index == 0 ? -half * 0.42f : (index == 1 ? half * 0.46f : 0.0f);
        drawList->AddCircleFilled(ImVec2(center.x + knobOffset, y), 2.0f,
                                  color);
    }
}

} // namespace ui
