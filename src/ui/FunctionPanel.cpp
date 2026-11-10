#include "ui/FunctionPanel.h"

#include "ui/UiWidgets.h"

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <string>

namespace ui {
namespace {

void DrawSectionTitle(const char* title, const char* tag,
                      const Fonts& fonts) {
    const ImVec2 position = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    constexpr float height = 22.0f;

    drawList->AddRectFilled(
        ImVec2(position.x, position.y + 2.0f),
        ImVec2(position.x + 3.0f, position.y + height - 2.0f),
        theme::Color(theme::kAccent), 1.5f);
    drawList->AddText(fonts.ui, 19.0f,
                      ImVec2(position.x + 12.0f, position.y),
                      theme::Color(theme::kText), title);

    const ImVec2 titleSize =
        fonts.ui->CalcTextSizeA(19.0f, FLT_MAX, 0.0f, title);
    drawList->AddText(fonts.mono, 11.0f,
                      ImVec2(position.x + 22.0f + titleSize.x,
                             position.y + 6.0f),
                      theme::Color(theme::kTextDim, 0.75f), tag);

    ImGui::Dummy(ImVec2(0.0f, height + 8.0f));
}

void DrawDeleteButton(const char* id, const ImVec2& min, bool* clicked) {
    ImGui::SetCursorScreenPos(min);
    ImGui::InvisibleButton(id, ImVec2(22.0f, 22.0f));
    const bool hovered = ImGui::IsItemHovered();
    if (hovered) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && clicked != nullptr) {
        *clicked = true;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImU32 color = hovered ? theme::Rgba(255, 92, 112, 255)
                                : theme::Rgba(188, 208, 224, 130);
    const ImVec2 center(min.x + 11.0f, min.y + 11.0f);
    drawList->AddLine(ImVec2(center.x - 4.5f, center.y - 4.5f),
                      ImVec2(center.x + 4.5f, center.y + 4.5f), color, 1.5f);
    drawList->AddLine(ImVec2(center.x + 4.5f, center.y - 4.5f),
                      ImVec2(center.x - 4.5f, center.y + 4.5f), color, 1.5f);
}

} // namespace

void DrawFunctionPanel(app::PlotterApp& app, const Fonts& fonts, float fps) {
    DrawSectionTitle("函数", "EXPRESSIONS", fonts);

    int removeIndex = -1;
    std::vector<app::FunctionEntry>& functions = app.Functions();

    for (int index = 0; index < static_cast<int>(functions.size()); ++index) {
        app::FunctionEntry& entry = functions[static_cast<size_t>(index)];
        const bool hasError =
            !entry.parsed.error.empty() && entry.expression[0] != '\0';
        const float rowHeight = hasError ? 96.0f : 54.0f;
        const ImVec2 rowMin = ImGui::GetCursorScreenPos();
        const float rowWidth = ImGui::GetContentRegionAvail().x;
        const ImVec2 rowMax(rowMin.x + rowWidth, rowMin.y + rowHeight);
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        ImGui::PushID(index);
        drawList->AddRectFilled(rowMin, rowMax, theme::Rgba(255, 255, 255, 7),
                                14.0f);
        drawList->AddRect(
            rowMin, rowMax,
            hasError ? theme::Rgba(255, 92, 112, 90)
                     : theme::Rgba(120, 220, 255, 32),
            14.0f, 0, 1.0f);

        const ImU32 accentColor = theme::Rgba(
            static_cast<int>(entry.color[0] * 255.0f),
            static_cast<int>(entry.color[1] * 255.0f),
            static_cast<int>(entry.color[2] * 255.0f), 220);
        drawList->AddRectFilled(
            ImVec2(rowMin.x + 1.0f, rowMin.y + 13.0f),
            ImVec2(rowMin.x + 4.0f, rowMax.y - 13.0f), accentColor, 1.5f);

        ImGui::SetCursorScreenPos(ImVec2(rowMin.x + 14.0f, rowMin.y + 10.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.0f, 2.0f));
        ImGui::ColorEdit4(
            "##color", entry.color,
            ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoAlpha |
                ImGuiColorEditFlags_NoLabel |
                ImGuiColorEditFlags_NoDragDrop);
        ImGui::PopStyleVar();

        ImGui::SameLine(0.0f, 6.0f);
        ImVec2 togglePosition = ImGui::GetCursorScreenPos();
        togglePosition.y = rowMin.y + 16.0f;
        ImGui::SetCursorScreenPos(togglePosition);
        ToggleSwitch("##visible", &entry.visible);

        ImGui::SameLine(0.0f, 10.0f);
        ImVec2 inputPosition = ImGui::GetCursorScreenPos();
        inputPosition.y = rowMin.y + 9.0f;
        ImGui::SetCursorScreenPos(inputPosition);
        const float rightEdge = rowMin.x + rowWidth - 46.0f;
        const float inputWidth =
            std::max(60.0f, rightEdge - inputPosition.x);
        ImGui::SetNextItemWidth(inputWidth);

        ImGui::PushFont(fonts.mono);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, theme::Rgba(0, 0, 0, 42));
        ImGui::PushStyleColor(ImGuiCol_Text, theme::Color(theme::kText));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 9.0f);
        const bool edited = ImGui::InputTextWithHint(
            "##expression", "a*sin(b*x)", entry.expression,
            app::FunctionEntry::kExpressionCapacity);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);
        ImGui::PopFont();
        if (edited) {
            app.UpdateFunction(index);
        }

        bool deleteClicked = false;
        DrawDeleteButton("##delete",
                         ImVec2(rowMin.x + rowWidth - 36.0f,
                                rowMin.y + 16.0f),
                         &deleteClicked);
        if (deleteClicked) {
            removeIndex = index;
        }

        if (hasError) {
            ImGui::SetCursorScreenPos(ImVec2(rowMin.x + 14.0f,
                                             rowMin.y + 50.0f));
            ImGui::PushTextWrapPos(ImGui::GetWindowContentRegionMax().x -
                                   14.0f);
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  theme::Color(theme::kDanger, 0.95f));
            ImGui::TextWrapped("%s", entry.parsed.error.c_str());
            ImGui::PopStyleColor();
            ImGui::PopTextWrapPos();
        }

        ImGui::SetCursorScreenPos(
            ImVec2(rowMin.x, rowMax.y + 8.0f));
        ImGui::PopID();
    }

    if (removeIndex >= 0) {
        app.RemoveFunction(removeIndex);
    }

    ImGui::Dummy(ImVec2(0.0f, 2.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, theme::Rgba(24, 108, 138, 95));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          theme::Rgba(34, 188, 226, 130));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                          theme::Rgba(64, 218, 255, 160));
    ImGui::PushStyleColor(ImGuiCol_Text, theme::Color(theme::kText));
    if (ImGui::Button("＋  添加函数", ImVec2(-1.0f, 40.0f))) {
        app.AddFunction("");
    }
    ImGui::PopStyleColor(4);

    ImGui::Dummy(ImVec2(0.0f, 16.0f));
    DrawSectionTitle("参数", "PARAMETERS", fonts);

    const std::vector<std::string>& parameters = app.ActiveParameters();
    if (parameters.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text,
                              theme::Color(theme::kTextDim, 0.9f));
        ImGui::TextWrapped("表达式中出现的字母会自动生成参数滑条。");
        ImGui::PopStyleColor();
    }

    for (const std::string& name : parameters) {
        ImGui::PushID(name.c_str());
        double& value = app.Parameters()[name];

        ImGui::PushFont(fonts.mono);
        ImGui::TextColored(theme::kAccent, "%s", name.c_str());
        char valueText[32];
        std::snprintf(valueText, sizeof(valueText), "%.3f", value);
        const float valueWidth = ImGui::CalcTextSize(valueText).x;
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - valueWidth);
        ImGui::TextColored(ImVec4(0.86f, 0.93f, 1.0f, 0.92f), "%s",
                           valueText);
        ImGui::PopFont();

        double minimum = -10.0;
        double maximum = 10.0;
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, theme::Rgba(255, 255, 255, 10));
        ImGui::SliderScalar("##value", ImGuiDataType_Double, &value, &minimum,
                            &maximum, "##");
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        ImGui::Dummy(ImVec2(0.0f, 2.0f));
        ImGui::PopID();
    }

    ImGui::Dummy(ImVec2(0.0f, 10.0f));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,
                          ImVec4(0.82f, 0.90f, 0.98f, 0.92f));
    ImGui::Text("采样 %d 点/曲线 · %.0f FPS", app.LastSampleCount(), fps);
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    if (ImGui::CollapsingHeader("表达式语法")) {
        ImGui::PushStyleColor(ImGuiCol_Text,
                              theme::Color(theme::kTextDim, 0.95f));
        ImGui::TextWrapped(
            "常量：pi、e    自变量：x\n"
            "隐式乘法：2x、2(x+1)、(x+1)(x-1)\n"
            "函数：sin cos tan asin acos atan sinh cosh tanh exp ln log10 "
            "log2 sqrt cbrt abs floor ceil round sign\n"
            "双参：min max pow atan2 mod");
        ImGui::PopStyleColor();
    }
}

} // namespace ui
