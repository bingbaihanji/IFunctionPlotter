#pragma once

#include "core/Sampler.h"
#include "core/Parser.h"

namespace app {

struct FunctionEntry {
    static constexpr int kExpressionCapacity = 256;

    char expression[kExpressionCapacity] = "";
    core::ParseResult parsed;
    core::SampleTrace trace;
    float color[4] = {0.22f, 0.88f, 1.0f, 1.0f};
    bool visible = true;

    bool Valid() const { return parsed.Ok(); }
};

} // namespace app
