#include "ui/PlotPanel.h"

#include "implot.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <string>

namespace ui {
namespace {

void DrawHeader(app::PlotterApp& app, PlotPanelState& state,
                const Fonts& fonts) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float headerY = ImGui::GetCursorPosY();

    drawList->AddRectFilled(
        ImVec2(start.x, start.y + 3.0f),
        ImVec2(start.x + 3.0f, start.y + 22.0f),
        theme::Color(theme::kAccent), 1.5f);

    ImGui::SetCursorScreenPos(ImVec2(start.x + 12.0f, start.y));
    ImGui::PushFont(fonts.ui);
    ImGui::TextColored(theme::kAccent, "实时曲线");
    ImGui::SameLine(0.0f, 10.0f);
    ImGui::TextDisabled("REALTIME PLOT");
    ImGui::SameLine(0.0f, 12.0f);

    int visibleCount = 0;
    for (const app::FunctionEntry& entry : app.Functions()) {
        if (entry.visible && entry.Valid()) {
            ++visibleCount;
        }
    }
    ImGui::TextDisabled("· %d 条可见", visibleCount);

    constexpr float buttonWidth = 96.0f;
    constexpr float buttonHeight = 30.0f;
    constexpr float buttonGap = 8.0f;
    const float buttonsStartX = ImGui::GetWindowContentRegionMax().x -
                                buttonWidth * 2.0f - buttonGap;
    ImGui::SetCursorPos(ImVec2(buttonsStartX, headerY));

    ImGui::PushStyleColor(ImGuiCol_Button, theme::Rgba(18, 66, 92, 110));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          theme::Rgba(28, 150, 190, 150));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                          theme::Rgba(64, 216, 255, 180));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
    if (ImGui::Button("自动适应", ImVec2(buttonWidth, buttonHeight))) {
        state.fitRequested = true;
    }
    ImGui::SameLine(0.0f, buttonGap);
    if (ImGui::Button("重置视图", ImVec2(buttonWidth, buttonHeight))) {
        state.resetRequested = true;
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    ImGui::PopFont();

    ImGui::SetCursorPos(ImVec2(0.0f, headerY + 36.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,
                          ImVec4(0.78f, 0.88f, 0.96f, 0.92f));
    ImGui::TextUnformatted("滚轮缩放 · 拖拽平移 · 每帧按像素密度实时采样");
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0.0f, 6.0f));
}

} // namespace

void DrawPlotPanel(app::PlotterApp& app, PlotPanelState& state,
                   const Fonts& fonts) {
    DrawHeader(app, state, fonts);

    const bool fitThisFrame = state.fitRequested;
    if (fitThisFrame) {
        ImPlot::SetNextAxesToFit();
    }

    ImPlot::PushStyleColor(ImPlotCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImPlot::PushStyleColor(ImPlotCol_PlotBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImPlot::PushStyleColor(ImPlotCol_PlotBorder,
                           ImVec4(0.28f, 0.84f, 1.0f, 0.16f));
    ImPlot::PushStyleColor(ImPlotCol_LegendBg,
                           ImVec4(0.02f, 0.05f, 0.09f, 0.62f));
    ImPlot::PushStyleColor(ImPlotCol_LegendBorder,
                           ImVec4(0.40f, 0.86f, 1.0f, 0.18f));
    ImPlot::PushStyleColor(ImPlotCol_LegendText,
                           ImVec4(0.82f, 0.92f, 1.0f, 0.92f));
    ImPlot::PushStyleColor(ImPlotCol_AxisText,
                           ImVec4(0.82f, 0.90f, 0.98f, 0.96f));
    ImPlot::PushStyleColor(ImPlotCol_AxisGrid,
                           ImVec4(0.34f, 0.74f, 1.0f, 0.14f));
    ImPlot::PushStyleColor(ImPlotCol_AxisTick,
                           ImVec4(0.55f, 0.84f, 1.0f, 0.60f));
    ImPlot::PushStyleColor(ImPlotCol_Crosshairs,
                           ImVec4(0.30f, 0.90f, 1.0f, 0.55f));
    ImPlot::PushStyleVar(ImPlotStyleVar_PlotBorderSize, 1.0f);
    ImPlot::PushStyleVar(ImPlotStyleVar_MinorAlpha, 0.22f);
    ImPlot::PushStyleVar(ImPlotStyleVar_MajorGridSize, ImVec2(1.0f, 1.0f));
    ImPlot::PushStyleVar(ImPlotStyleVar_MinorGridSize, ImVec2(1.0f, 1.0f));

    if (ImPlot::BeginPlot("##plot", ImVec2(-1.0f, -1.0f),
                          ImPlotFlags_NoTitle | ImPlotFlags_Crosshairs)) {
        ImPlot::SetupAxes("x", "y", ImPlotAxisFlags_NoInitialFit,
                          ImPlotAxisFlags_NoInitialFit);
        if (!fitThisFrame) {
            const ImPlotCond condition = state.resetRequested
                                             ? ImPlotCond_Always
                                             : ImPlotCond_Once;
            ImPlot::SetupAxesLimits(-10.0, 10.0, -5.0, 5.0, condition);
        }
        state.fitRequested = false;
        state.resetRequested = false;

        const ImPlotRect limits = ImPlot::GetPlotLimits(ImAxis_X1, ImAxis_Y1);
        const ImVec2 plotSize = ImPlot::GetPlotSize();
        app.SampleAll(limits.X.Min, limits.X.Max,
                      static_cast<int>(std::max(64.0f, plotSize.x)));

        int visibleCount = 0;
        for (int index = 0;
             index < static_cast<int>(app.Functions().size()); ++index) {
            app::FunctionEntry& entry =
                app.Functions()[static_cast<size_t>(index)];
            if (!entry.visible || !entry.Valid() || entry.trace.x.empty()) {
                continue;
            }

            const int count = static_cast<int>(entry.trace.x.size());
            char label[512];
            std::snprintf(label, sizeof(label), "f%d  %s###f%d", index + 1,
                          entry.expression, index + 1);
            char glowLabel[64];
            std::snprintf(glowLabel, sizeof(glowLabel), "##glow%d", index);

            const ImVec4 color(entry.color[0], entry.color[1],
                               entry.color[2], 1.0f);

            ImPlotSpec glowSpec;
            glowSpec.LineColor =
                ImVec4(color.x, color.y, color.z, 0.22f);
            glowSpec.LineWeight = 9.0f;
            glowSpec.Flags = static_cast<ImPlotItemFlags>(
                ImPlotItemFlags_NoLegend | ImPlotLineFlags_SkipNaN);
            ImPlot::PlotLine(glowLabel, entry.trace.x.data(),
                             entry.trace.y.data(), count, glowSpec);

            ImPlotSpec lineSpec;
            lineSpec.LineColor = color;
            lineSpec.LineWeight = 2.6f;
            lineSpec.Flags =
                static_cast<ImPlotItemFlags>(ImPlotLineFlags_SkipNaN);
            ImPlot::PlotLine(label, entry.trace.x.data(),
                             entry.trace.y.data(), count, lineSpec);
            ++visibleCount;
        }

        if (visibleCount == 0) {
            const ImVec2 plotPosition = ImPlot::GetPlotPos();
            const ImVec2 plotAreaSize = ImPlot::GetPlotSize();
            const char* hint = "输入表达式，曲线将实时出现";
            const ImVec2 textSize = ImGui::CalcTextSize(hint);
            ImGui::GetWindowDrawList()->AddText(
                ImVec2(plotPosition.x +
                           (plotAreaSize.x - textSize.x) * 0.5f,
                       plotPosition.y +
                           (plotAreaSize.y - textSize.y) * 0.5f),
                theme::Color(theme::kTextDim, 0.75f), hint);
        }

        ImPlot::EndPlot();
    }

    ImPlot::PopStyleVar(4);
    ImPlot::PopStyleColor(10);
}

} // namespace ui
