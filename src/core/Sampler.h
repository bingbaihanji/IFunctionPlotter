#pragma once

#include "core/Expression.h"

#include <map>
#include <string>
#include <vector>

namespace core {

struct SampleTrace {
    std::vector<double> x;
    std::vector<double> y;
};

void SampleExpression(const Expr& expression,
                      const std::map<std::string, double>& parameters,
                      double xMin, double xMax, int sampleCount,
                      SampleTrace& output);

} // namespace core
