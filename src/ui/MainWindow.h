#pragma once

#include "app/PlotterApp.h"
#include "render/GlassPanel.h"
#include "ui/AppSettings.h"
#include "ui/PlotPanel.h"
#include "ui/UiTheme.h"
#include "ui/WindowActions.h"

#include <string>
#include <vector>

namespace ui {

struct UiState {
    PlotPanelState plot;
    AppearanceSettings appearance;
    std::string desktopStatus = "桌面采样未开启";
    bool desktopCaptureActive = false;
    bool refreshDesktopSnapshot = false;
};

void DrawMainUi(app::PlotterApp& app, const WindowActions& windowActions,
                UiState& state,
                std::vector<render::GlassPanel>& glassPanels,
                const Fonts& fonts, float timeSeconds, float fps);

} // namespace ui
