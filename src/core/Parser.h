#pragma once

#include "core/Expression.h"

#include <string>
#include <vector>

namespace core {

struct ParseResult {
    ExprPtr expression;
    std::vector<std::string> freeVariables;
    std::string error;

    bool Ok() const { return expression != nullptr && error.empty(); }
};

ParseResult ParseExpression(const std::string& source);

} // namespace core
