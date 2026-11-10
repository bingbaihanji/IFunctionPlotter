#include "ui/MainWindow.h"

#include "ui/FunctionPanel.h"
#include "ui/UiWidgets.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>

namespace ui {
namespace {

constexpr const char* kSettingsPopupId = "##settings_popup";

constexpr float kMargin = 18.0f;
constexpr float kGap = 14.0f;
constexpr float kTitleHeight = 66.0f;
constexpr float kSidebarWidth = 410.0f;
constexpr float kPanelRadius = 28.0f;
constexpr float kTitleRadius = 24.0f;
constexpr float kWindowButtonWidth = 38.0f;
constexpr float kWindowButtonHeight = 30.0f;
constexpr float kWindowButtonGap = 4.0f;
constexpr int kWindowButtonCount = 3;

void AddGlassPanel(std::vector<render::GlassPanel>& panels, const ImVec2& min,
                   const ImVec2& max, float radius, const float tint[4],
                   float refractionAmount, float opacity) {
    render::GlassPanel panel;
    panel.x = min.x;
    panel.y = min.y;
    panel.width = max.x - min.x;
    panel.height = max.y - min.y;
    panel.radius = radius;
    panel.refractionAmount = refractionAmount;
    panel.opacity = opacity;
    for (int index = 0; index < 4; ++index) {
        panel.tint[index] = tint[index];
    }
    panels.push_back(panel);
}

void DrawSineBadge(ImDrawList* drawList, const ImVec2& min, float size) {
    const ImVec2 max(min.x + size, min.y + size);
    drawList->AddRectFilled(min, max, theme::Rgba(10, 32, 52, 215), 10.0f);
    drawList->AddRectFilled(min, ImVec2(max.x, min.y + size * 0.5f),
                            theme::Rgba(40, 170, 210, 45), 10.0f,
                            ImDrawFlags_RoundCornersTop);
    drawList->AddRect(min, max, theme::Rgba(120, 235, 255, 110), 10.0f, 0,
                      1.0f);

    constexpr int kPointCount = 24;
    ImVec2 points[kPointCount];
    const float centerY = min.y + size * 0.5f;
    const float amplitude = size * 0.22f;
    const float inset = size * 0.20f;
    for (int index = 0; index < kPointCount; ++index) {
        const float t = static_cast<float>(index) /
                        static_cast<float>(kPointCount - 1);
        points[index].x = min.x + inset + t * (size - inset * 2.0f);
        points[index].y =
            centerY - std::sin(t * 6.2831853f * 1.25f) * amplitude;
    }
    drawList->AddPolyline(points, kPointCount,
                          theme::Rgba(245, 252, 255, 235),
                          ImDrawFlags_None, 2.0f);
}

void DrawChip(ImDrawList* drawList, const Fonts& fonts, const ImVec2& min,
              const ImVec2& size, const char* text, const ImVec4& color,
              bool pulsing, float timeSeconds) {
    const ImVec2 max(min.x + size.x, min.y + size.y);
    drawList->AddRectFilled(min, max, theme::Rgba(8, 24, 38, 120),
                            size.y * 0.5f);
    drawList->AddRect(min, max, theme::Color(color, 0.25f), size.y * 0.5f, 0,
                      1.0f);

    float textOffset = 12.0f;
    if (pulsing) {
        const float pulse = 0.55f + 0.45f *
                                        std::sin(timeSeconds * 4.0f);
        drawList->AddCircleFilled(
            ImVec2(min.x + 12.0f, (min.y + max.y) * 0.5f), 3.5f,
            theme::Color(color, pulse));
        textOffset = 21.0f;
    }

    const ImVec2 textSize =
        fonts.mono->CalcTextSizeA(13.0f, FLT_MAX, 0.0f, text);
    drawList->AddText(
        fonts.mono, 13.0f,
        ImVec2(min.x + textOffset,
               (min.y + max.y - textSize.y) * 0.5f),
        theme::Color(color, 0.95f), text);
}

bool DrawWindowButton(const char* id, const ImVec2& min, int kind,
                      bool danger) {
    const ImVec2 size(kWindowButtonWidth, kWindowButtonHeight);
    ImGui::SetCursorScreenPos(min);
    ImGui::InvisibleButton(id, size);
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();
    const bool clicked = ImGui::IsItemClicked();

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 max(min.x + size.x, min.y + size.y);
    if (hovered || held) {
        const ImU32 background =
            danger ? theme::Rgba(220, 48, 74, held ? 220 : 150)
                   : theme::Rgba(255, 255, 255, held ? 46 : 26);
        drawList->AddRectFilled(min, max, background, 8.0f);
    }

    const ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
    const ImU32 glyphColor =
        danger ? theme::Rgba(255, 235, 240, 235)
               : theme::Rgba(210, 232, 245, 220);
    if (kind == 0) {
        drawList->AddLine(ImVec2(center.x - 5.0f, center.y),
                          ImVec2(center.x + 5.0f, center.y), glyphColor, 1.4f);
    } else if (kind == 1) {
        drawList->AddRect(ImVec2(center.x - 5.0f, center.y - 5.0f),
                          ImVec2(center.x + 5.0f, center.y + 5.0f), glyphColor,
                          1.0f, 0, 1.4f);
    } else {
        drawList->AddLine(ImVec2(center.x - 4.5f, center.y - 4.5f),
                          ImVec2(center.x + 4.5f, center.y + 4.5f), glyphColor,
                          1.5f);
        drawList->AddLine(ImVec2(center.x + 4.5f, center.y - 4.5f),
                          ImVec2(center.x - 4.5f, center.y + 4.5f), glyphColor,
                          1.5f);
    }
    if (hovered) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    return clicked;
}

void DrawTitleBar(const WindowActions& actions, const Fonts& fonts,
                  const ImVec2& min, const ImVec2& max, float timeSeconds,
                  float fps) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    DrawSineBadge(drawList, ImVec2(min.x + 22.0f, min.y + 15.0f), 36.0f);

    const float textX = min.x + 70.0f;
    drawList->AddText(fonts.title, fonts.title->LegacySize,
                      ImVec2(textX, min.y + 9.0f), theme::Color(theme::kText),
                      "IFunctionPlotter");
    drawList->AddText(fonts.ui, 16.0f, ImVec2(textX + 1.0f, min.y + 40.0f),
                      theme::Color(theme::kTextDim),
                      "函数绘制器 · 实时表达式可视化");

    const float buttonsWidth =
        kWindowButtonCount * kWindowButtonWidth +
        (kWindowButtonCount - 1) * kWindowButtonGap;
    const float buttonsX = max.x - 20.0f - buttonsWidth;
    const float buttonsY = min.y + (kTitleHeight - kWindowButtonHeight) * 0.5f;

    const ImVec2 fpsChipSize(94.0f, 26.0f);
    const ImVec2 liveChipSize(72.0f, 26.0f);
    const float chipY = min.y + (kTitleHeight - 26.0f) * 0.5f;

    char fpsText[32];
    std::snprintf(fpsText, sizeof(fpsText), "%.0f FPS", fps);
    const float fpsX = buttonsX - 14.0f - fpsChipSize.x;
    DrawChip(drawList, fonts, ImVec2(fpsX, chipY), fpsChipSize, fpsText,
             theme::kTextDim, false, timeSeconds);
    DrawChip(drawList, fonts,
             ImVec2(fpsX - 8.0f - liveChipSize.x, chipY), liveChipSize, "LIVE",
             theme::kAccent, true, timeSeconds);

    const ImVec2 settingsChipSize(34.0f, 26.0f);
    const float settingsX =
        fpsX - 8.0f - liveChipSize.x - 8.0f - settingsChipSize.x;
    const ImVec2 settingsMin(settingsX, chipY);
    ImGui::SetCursorScreenPos(settingsMin);
    ImGui::InvisibleButton("##settings_button", settingsChipSize);
    const bool settingsHovered = ImGui::IsItemHovered();
    if (settingsHovered || ImGui::IsItemActive()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        drawList->AddRectFilled(
            settingsMin,
            ImVec2(settingsMin.x + settingsChipSize.x,
                   settingsMin.y + settingsChipSize.y),
            theme::Rgba(255, 255, 255, ImGui::IsItemActive() ? 46 : 26),
            8.0f);
    }
    DrawSettingsIcon(
        drawList,
        ImVec2(settingsMin.x + settingsChipSize.x * 0.5f,
               settingsMin.y + settingsChipSize.y * 0.5f),
        12.0f, theme::Color(theme::kText, 0.88f));
    if (settingsHovered) {
        ImGui::SetTooltip("界面设置");
    }
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        ImGui::OpenPopup(kSettingsPopupId);
    }

    const float dragWidth =
        settingsX - 8.0f - (min.x + 12.0f) - 10.0f;
    if (dragWidth > 0.0f) {
        ImGui::SetCursorScreenPos(ImVec2(min.x + 12.0f, min.y + 8.0f));
        ImGui::InvisibleButton(
            "##title_drag",
            ImVec2(dragWidth, kTitleHeight - 16.0f));
        const bool dragHovered = ImGui::IsItemHovered();
        if (dragHovered &&
            ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
            actions.toggleMaximize != nullptr) {
            actions.toggleMaximize(actions.context);
        } else if (dragHovered &&
                   ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
                   actions.beginDrag != nullptr) {
            actions.beginDrag(actions.context);
        }
    }

    float buttonX = buttonsX;
    if (DrawWindowButton("##minimize", ImVec2(buttonX, buttonsY), 0, false) &&
        actions.minimize != nullptr) {
        actions.minimize(actions.context);
    }
    buttonX += kWindowButtonWidth + kWindowButtonGap;
    if (DrawWindowButton("##maximize", ImVec2(buttonX, buttonsY), 1, false) &&
        actions.toggleMaximize != nullptr) {
        actions.toggleMaximize(actions.context);
    }
    buttonX += kWindowButtonWidth + kWindowButtonGap;
    if (DrawWindowButton("##close", ImVec2(buttonX, buttonsY), 2, true) &&
        actions.close != nullptr) {
        actions.close(actions.context);
    }
}

void DrawToggleRow(const char* label, const char* id, bool* value) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - 36.0f);
    ToggleSwitch(id, value);
}

void DrawSettingsPopup(UiState& state, const Fonts& fonts,
                       std::vector<render::GlassPanel>& glassPanels) {
    const ImVec2 popupMin = ImGui::GetWindowPos();
    const ImVec2 popupSize = ImGui::GetWindowSize();
    static const float kSettingsTint[4] = {0.005f, 0.025f, 0.025f, 0.58f};
    AddGlassPanel(glassPanels, popupMin,
                  ImVec2(popupMin.x + popupSize.x,
                         popupMin.y + popupSize.y),
                  18.0f, kSettingsTint, -80.0f, 0.97f);

    ImGui::PushFont(fonts.ui);
    ImGui::TextColored(theme::kAccent, "界面设置");
    ImGui::SameLine();
    ImGui::TextDisabled("APPEARANCE");
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    ImGui::BeginDisabled(state.desktopCaptureActive);
    DrawToggleRow("渐变背景", "##gradient",
                  &state.appearance.gradientEnabled);
    ImGui::TextWrapped(
        state.desktopCaptureActive
            ? "桌面采样开启时，渐变背景由桌面画面替代。"
            : "关闭后使用深色纯色背景。");
    ImGui::EndDisabled();
    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    DrawToggleRow("桌面背景采样", "##desktop_backdrop",
                  &state.appearance.desktopBackdrop);
    ImGui::TextWrapped(
        "无色液体玻璃：采样窗口后方的桌面，"
        "并应用高斯模糊、边缘折射与色散。");
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    DrawToggleRow("录屏兼容模式", "##recording_compatible",
                  &state.appearance.recordingCompatible);
    ImGui::TextWrapped(
        "使用静态桌面快照作为玻璃背景，窗口可正常截图/录屏。"
        "实时模式会排除本窗口，因此无法被录屏捕获。");

    if (state.appearance.desktopBackdrop &&
        state.appearance.recordingCompatible) {
        if (ImGui::Button("刷新背景", ImVec2(-1.0f, 34.0f))) {
            state.refreshDesktopSnapshot = true;
        }
        ImGui::TextDisabled(
            "刷新时窗口会短暂从录屏中消失（用于排除自身）。");
    }

    const ImVec4 statusColor =
        state.desktopCaptureActive
            ? ImVec4(0.35f, 0.95f, 0.75f, 1.0f)
            : (state.appearance.desktopBackdrop
                   ? ImVec4(1.0f, 0.72f, 0.32f, 1.0f)
                   : theme::kTextDim);
    ImGui::PushStyleColor(ImGuiCol_Text, statusColor);
    ImGui::TextWrapped("%s", state.desktopStatus.c_str());
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    ImGui::TextUnformatted("玻璃不透明度");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderFloat("##glass_opacity", &state.appearance.glassOpacity,
                       0.35f, 1.0f, "%.2f");
    ImGui::TextDisabled("数值越低，越接近完全透明的无色玻璃。");
    ImGui::Dummy(ImVec2(0.0f, 14.0f));

    if (ImGui::Button("完成", ImVec2(-1.0f, 38.0f))) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::PopFont();
}

} // namespace

void DrawMainUi(app::PlotterApp& app, const WindowActions& windowActions,
                UiState& state,
                std::vector<render::GlassPanel>& glassPanels,
                const Fonts& fonts, float timeSeconds, float fps) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 origin = viewport->Pos;
    const ImVec2 size = viewport->Size;

    ImGui::SetNextWindowPos(origin);
    ImGui::SetNextWindowSize(size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::Begin("##IFunctionPlotterRoot", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoScrollWithMouse |
                     ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoNavFocus);

    const ImVec2 titleMin(origin.x + kMargin, origin.y + kMargin);
    const ImVec2 titleMax(origin.x + size.x - kMargin,
                          titleMin.y + kTitleHeight);
    const ImVec2 sidebarMin(titleMin.x, titleMax.y + kGap);
    const ImVec2 sidebarMax(sidebarMin.x + kSidebarWidth,
                            origin.y + size.y - kMargin);
    const ImVec2 plotMin(sidebarMax.x + kGap, sidebarMin.y);
    const ImVec2 plotMax(origin.x + size.x - kMargin,
                         origin.y + size.y - kMargin);

    const bool desktopMode =
        state.appearance.desktopBackdrop && state.desktopCaptureActive;
    static const float kTitleTintNormal[4] = {0.02f, 0.06f, 0.06f, 0.18f};
    static const float kSidebarTintNormal[4] = {0.01f, 0.05f, 0.05f, 0.22f};
    static const float kPlotTintNormal[4] = {0.01f, 0.04f, 0.05f, 0.20f};
    static const float kTitleTintDesktop[4] = {0.02f, 0.02f, 0.02f, 0.03f};
    static const float kSidebarTintDesktop[4] = {0.02f, 0.02f, 0.02f, 0.03f};
    static const float kPlotTintDesktop[4] = {0.02f, 0.02f, 0.02f, 0.03f};

    const float* titleTint =
        desktopMode ? kTitleTintDesktop : kTitleTintNormal;
    const float* sidebarTint =
        desktopMode ? kSidebarTintDesktop : kSidebarTintNormal;
    const float* plotTint =
        desktopMode ? kPlotTintDesktop : kPlotTintNormal;
    const float opacity =
        std::clamp(state.appearance.glassOpacity, 0.0f, 1.0f);

    AddGlassPanel(glassPanels, titleMin, titleMax, kTitleRadius, titleTint,
                  -90.0f, opacity);
    AddGlassPanel(glassPanels, sidebarMin, sidebarMax, kPanelRadius,
                  sidebarTint, -120.0f, opacity);
    AddGlassPanel(glassPanels, plotMin, plotMax, kPanelRadius, plotTint,
                  -130.0f, opacity);

    DrawTitleBar(windowActions, fonts, titleMin, titleMax, timeSeconds, fps);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(22.0f, 20.0f));
    ImGui::SetCursorScreenPos(sidebarMin);
    ImGui::BeginChild("##sidebar", ImVec2(sidebarMax.x - sidebarMin.x,
                                          sidebarMax.y - sidebarMin.y),
                      ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);
    DrawFunctionPanel(app, fonts, fps);
    ImGui::EndChild();
    ImGui::PopStyleVar();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 18.0f));
    ImGui::SetCursorScreenPos(plotMin);
    ImGui::BeginChild("##plot_panel",
                      ImVec2(plotMax.x - plotMin.x, plotMax.y - plotMin.y),
                      ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoBackground |
                          ImGuiWindowFlags_NoScrollbar |
                          ImGuiWindowFlags_NoScrollWithMouse);
    DrawPlotPanel(app, state.plot, fonts);
    ImGui::EndChild();
    ImGui::PopStyleVar();

    ImGui::SetNextWindowSize(ImVec2(460.0f, 0.0f),
                             ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(
        ImVec2(origin.x + size.x * 0.5f, origin.y + size.y * 0.5f),
        ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(22.0f, 20.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 18.0f);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    if (ImGui::BeginPopup(kSettingsPopupId)) {
        DrawSettingsPopup(state, fonts, glassPanels);
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);

    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
}

} // namespace ui
