#include "core/Sampler.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace core {

void SampleExpression(const Expr& expression,
                      const std::map<std::string, double>& parameters,
                      double xMin, double xMax, int sampleCount,
                      SampleTrace& output) {
    sampleCount = std::clamp(sampleCount, 2, 16384);
    output.x.resize(static_cast<size_t>(sampleCount));
    output.y.resize(static_cast<size_t>(sampleCount));

    const double step = (xMax - xMin) / static_cast<double>(sampleCount - 1);
    EvalContext context{0.0, parameters};
    constexpr double kNotANumber = std::numeric_limits<double>::quiet_NaN();

    for (int index = 0; index < sampleCount; ++index) {
        const double x = xMin + step * static_cast<double>(index);
        context.x = x;
        const double y = expression.Eval(context);
        output.x[static_cast<size_t>(index)] = x;
        output.y[static_cast<size_t>(index)] =
            std::isfinite(y) ? y : kNotANumber;
    }
}

} // namespace core
