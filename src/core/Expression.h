#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace core {

struct EvalContext {
    double x = 0.0;
    const std::map<std::string, double>& parameters;
};

class Expr {
public:
    virtual ~Expr() = default;
    virtual double Eval(const EvalContext& context) const = 0;
};

using ExprPtr = std::unique_ptr<Expr>;

enum class BuiltinFunction {
    Sin,
    Cos,
    Tan,
    Asin,
    Acos,
    Atan,
    Sinh,
    Cosh,
    Tanh,
    Exp,
    Log,
    Log10,
    Log2,
    Sqrt,
    Cbrt,
    Abs,
    Floor,
    Ceil,
    Round,
    Sign,
    Min,
    Max,
    Pow,
    Atan2,
    Mod,
};

ExprPtr MakeNumberExpr(double value);
ExprPtr MakeVariableExpr(std::string name);
ExprPtr MakeUnaryExpr(char op, ExprPtr operand);
ExprPtr MakeBinaryExpr(char op, ExprPtr lhs, ExprPtr rhs);
ExprPtr MakeCallExpr(BuiltinFunction function, std::vector<ExprPtr> arguments);

bool FindBuiltinFunction(const std::string& name, BuiltinFunction& function);
int BuiltinArity(BuiltinFunction function);

} // namespace core
