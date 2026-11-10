#include "core/Expression.h"

#include <cmath>
#include <limits>
#include <utility>

namespace core {
namespace {

class NumberExpr final : public Expr {
public:
    explicit NumberExpr(double value) : value_(value) {}

    double Eval(const EvalContext&) const override { return value_; }

private:
    double value_;
};

class VariableExpr final : public Expr {
public:
    explicit VariableExpr(std::string name) : name_(std::move(name)) {}

    double Eval(const EvalContext& context) const override {
        if (name_ == "x") {
            return context.x;
        }
        const auto found = context.parameters.find(name_);
        return found == context.parameters.end()
                   ? std::numeric_limits<double>::quiet_NaN()
                   : found->second;
    }

private:
    std::string name_;
};

class UnaryExpr final : public Expr {
public:
    UnaryExpr(char op, ExprPtr operand) : op_(op), operand_(std::move(operand)) {}

    double Eval(const EvalContext& context) const override {
        const double value = operand_->Eval(context);
        return op_ == '-' ? -value : value;
    }

private:
    char op_;
    ExprPtr operand_;
};

class BinaryExpr final : public Expr {
public:
    BinaryExpr(char op, ExprPtr lhs, ExprPtr rhs)
        : op_(op), lhs_(std::move(lhs)), rhs_(std::move(rhs)) {}

    double Eval(const EvalContext& context) const override {
        const double left = lhs_->Eval(context);
        const double right = rhs_->Eval(context);
        switch (op_) {
        case '+':
            return left + right;
        case '-':
            return left - right;
        case '*':
            return left * right;
        case '/':
            return left / right;
        case '%':
            return std::fmod(left, right);
        case '^':
            return std::pow(left, right);
        default:
            return std::numeric_limits<double>::quiet_NaN();
        }
    }

private:
    char op_;
    ExprPtr lhs_;
    ExprPtr rhs_;
};

class CallExpr final : public Expr {
public:
    CallExpr(BuiltinFunction function, std::vector<ExprPtr> arguments)
        : function_(function), arguments_(std::move(arguments)) {}

    double Eval(const EvalContext& context) const override {
        double first = 0.0;
        double second = 0.0;
        if (!arguments_.empty()) {
            first = arguments_[0]->Eval(context);
        }
        if (arguments_.size() > 1) {
            second = arguments_[1]->Eval(context);
        }

        switch (function_) {
        case BuiltinFunction::Sin:
            return std::sin(first);
        case BuiltinFunction::Cos:
            return std::cos(first);
        case BuiltinFunction::Tan:
            return std::tan(first);
        case BuiltinFunction::Asin:
            return std::asin(first);
        case BuiltinFunction::Acos:
            return std::acos(first);
        case BuiltinFunction::Atan:
            return std::atan(first);
        case BuiltinFunction::Sinh:
            return std::sinh(first);
        case BuiltinFunction::Cosh:
            return std::cosh(first);
        case BuiltinFunction::Tanh:
            return std::tanh(first);
        case BuiltinFunction::Exp:
            return std::exp(first);
        case BuiltinFunction::Log:
            return std::log(first);
        case BuiltinFunction::Log10:
            return std::log10(first);
        case BuiltinFunction::Log2:
            return std::log2(first);
        case BuiltinFunction::Sqrt:
            return std::sqrt(first);
        case BuiltinFunction::Cbrt:
            return std::cbrt(first);
        case BuiltinFunction::Abs:
            return std::abs(first);
        case BuiltinFunction::Floor:
            return std::floor(first);
        case BuiltinFunction::Ceil:
            return std::ceil(first);
        case BuiltinFunction::Round:
            return std::round(first);
        case BuiltinFunction::Sign:
            return first > 0.0 ? 1.0 : (first < 0.0 ? -1.0 : 0.0);
        case BuiltinFunction::Min:
            return std::fmin(first, second);
        case BuiltinFunction::Max:
            return std::fmax(first, second);
        case BuiltinFunction::Pow:
            return std::pow(first, second);
        case BuiltinFunction::Atan2:
            return std::atan2(first, second);
        case BuiltinFunction::Mod:
            return std::fmod(first, second);
        }
        return std::numeric_limits<double>::quiet_NaN();
    }

private:
    BuiltinFunction function_;
    std::vector<ExprPtr> arguments_;
};

struct FunctionName {
    const char* name;
    BuiltinFunction function;
    int arity;
};

constexpr FunctionName kFunctions[] = {
    {"sin", BuiltinFunction::Sin, 1},     {"cos", BuiltinFunction::Cos, 1},
    {"tan", BuiltinFunction::Tan, 1},     {"asin", BuiltinFunction::Asin, 1},
    {"acos", BuiltinFunction::Acos, 1},   {"atan", BuiltinFunction::Atan, 1},
    {"sinh", BuiltinFunction::Sinh, 1},   {"cosh", BuiltinFunction::Cosh, 1},
    {"tanh", BuiltinFunction::Tanh, 1},   {"exp", BuiltinFunction::Exp, 1},
    {"ln", BuiltinFunction::Log, 1},      {"log", BuiltinFunction::Log, 1},
    {"log10", BuiltinFunction::Log10, 1}, {"log2", BuiltinFunction::Log2, 1},
    {"sqrt", BuiltinFunction::Sqrt, 1},   {"cbrt", BuiltinFunction::Cbrt, 1},
    {"abs", BuiltinFunction::Abs, 1},     {"floor", BuiltinFunction::Floor, 1},
    {"ceil", BuiltinFunction::Ceil, 1},   {"round", BuiltinFunction::Round, 1},
    {"sign", BuiltinFunction::Sign, 1},   {"min", BuiltinFunction::Min, 2},
    {"max", BuiltinFunction::Max, 2},     {"pow", BuiltinFunction::Pow, 2},
    {"atan2", BuiltinFunction::Atan2, 2}, {"mod", BuiltinFunction::Mod, 2},
};

} // namespace

ExprPtr MakeNumberExpr(double value) { return std::make_unique<NumberExpr>(value); }

ExprPtr MakeVariableExpr(std::string name) {
    return std::make_unique<VariableExpr>(std::move(name));
}

ExprPtr MakeUnaryExpr(char op, ExprPtr operand) {
    return std::make_unique<UnaryExpr>(op, std::move(operand));
}

ExprPtr MakeBinaryExpr(char op, ExprPtr lhs, ExprPtr rhs) {
    return std::make_unique<BinaryExpr>(op, std::move(lhs), std::move(rhs));
}

ExprPtr MakeCallExpr(BuiltinFunction function, std::vector<ExprPtr> arguments) {
    return std::make_unique<CallExpr>(function, std::move(arguments));
}

bool FindBuiltinFunction(const std::string& name, BuiltinFunction& function) {
    for (const FunctionName& entry : kFunctions) {
        if (name == entry.name) {
            function = entry.function;
            return true;
        }
    }
    return false;
}

int BuiltinArity(BuiltinFunction function) {
    for (const FunctionName& entry : kFunctions) {
        if (entry.function == function) {
            return entry.arity;
        }
    }
    return 0;
}

} // namespace core
