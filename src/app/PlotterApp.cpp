#include "app/PlotterApp.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <set>
#include <utility>

namespace app {
namespace {

constexpr float kPalette[][4] = {
    {0.22f, 0.88f, 1.00f, 1.0f}, // cyan
    {1.00f, 0.38f, 0.82f, 1.0f}, // magenta
    {0.63f, 1.00f, 0.42f, 1.0f}, // lime
    {1.00f, 0.78f, 0.32f, 1.0f}, // amber
    {0.70f, 0.56f, 1.00f, 1.0f}, // violet
    {1.00f, 0.46f, 0.40f, 1.0f}, // coral
};

constexpr int kPaletteSize =
    static_cast<int>(sizeof(kPalette) / sizeof(kPalette[0]));

void CopyColor(const float source[4], float destination[4]) {
    for (int index = 0; index < 4; ++index) {
        destination[index] = source[index];
    }
}

} // namespace

PlotterApp::PlotterApp() {
    AddFunction("a*sin(b*x)");
    AddFunction("0.35*cos(3*x)");
}

void PlotterApp::AddFunction(const std::string& expression) {
    FunctionEntry entry;
    const int length = std::snprintf(entry.expression,
                                     FunctionEntry::kExpressionCapacity, "%s",
                                     expression.c_str());
    if (length < 0) {
        entry.expression[0] = '\0';
    }
    CopyColor(kPalette[nextColorIndex_ % kPaletteSize], entry.color);
    ++nextColorIndex_;

    functions_.push_back(std::move(entry));
    UpdateFunction(static_cast<int>(functions_.size()) - 1);
}

void PlotterApp::RemoveFunction(int index) {
    if (index < 0 || index >= static_cast<int>(functions_.size())) {
        return;
    }
    functions_.erase(functions_.begin() + index);
    SyncParameters();
}

void PlotterApp::UpdateFunction(int index) {
    if (index < 0 || index >= static_cast<int>(functions_.size())) {
        return;
    }
    FunctionEntry& entry = functions_[static_cast<size_t>(index)];
    entry.parsed = core::ParseExpression(entry.expression);
    SyncParameters();
}

void PlotterApp::SyncParameters() {
    std::vector<std::string> active;
    std::set<std::string> seen;

    for (const FunctionEntry& entry : functions_) {
        if (!entry.Valid()) {
            continue;
        }
        for (const std::string& name : entry.parsed.freeVariables) {
            if (seen.insert(name).second) {
                active.push_back(name);
            }
        }
    }

    for (const std::string& name : active) {
        parameters_.emplace(name, 1.0);
    }
    activeParameters_ = std::move(active);
}

void PlotterApp::SampleAll(double xMin, double xMax, int pixelWidth) {
    if (!(xMax > xMin)) {
        return;
    }

    const int sampleCount = std::clamp(pixelWidth * 2, 512, 8192);
    lastSampleCount_ = sampleCount;

    for (FunctionEntry& entry : functions_) {
        if (!entry.visible || !entry.Valid()) {
            continue;
        }
        core::SampleExpression(*entry.parsed.expression, parameters_, xMin,
                               xMax, sampleCount, entry.trace);
    }
}

} // namespace app
