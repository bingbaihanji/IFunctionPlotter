#pragma once

#include <imgui.h>

namespace ui {

bool ToggleSwitch(const char* id, bool* value,
                  const ImVec2& size = ImVec2(34.0f, 18.0f));
void DrawSettingsIcon(ImDrawList* drawList, const ImVec2& center,
                      float size, ImU32 color);

} // namespace ui
