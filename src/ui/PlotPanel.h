#pragma once

#include "app/PlotterApp.h"
#include "ui/UiTheme.h"

namespace ui {

struct PlotPanelState {
    bool fitRequested = false;
    bool resetRequested = false;
};

void DrawPlotPanel(app::PlotterApp& app, PlotPanelState& state,
                   const Fonts& fonts);

} // namespace ui
