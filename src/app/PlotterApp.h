#pragma once

#include "app/FunctionEntry.h"

#include <map>
#include <string>
#include <vector>

namespace app {

class PlotterApp {
public:
    PlotterApp();

    void AddFunction(const std::string& expression = "sin(x)");
    void RemoveFunction(int index);
    void UpdateFunction(int index);
    void SyncParameters();

    void SampleAll(double xMin, double xMax, int pixelWidth);

    std::vector<FunctionEntry>& Functions() { return functions_; }
    const std::vector<FunctionEntry>& Functions() const { return functions_; }

    std::map<std::string, double>& Parameters() { return parameters_; }
    const std::map<std::string, double>& Parameters() const {
        return parameters_;
    }

    const std::vector<std::string>& ActiveParameters() const {
        return activeParameters_;
    }

    int LastSampleCount() const { return lastSampleCount_; }

private:
    std::vector<FunctionEntry> functions_;
    std::map<std::string, double> parameters_;
    std::vector<std::string> activeParameters_;
    int lastSampleCount_ = 0;
    int nextColorIndex_ = 0;
};

} // namespace app
