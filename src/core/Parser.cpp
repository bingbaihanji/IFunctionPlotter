#include "core/Parser.h"

#include <cctype>
#include <cstdlib>
#include <set>
#include <stdexcept>
#include <utility>

namespace core {
namespace {

constexpr int kMaximumDepth = 128;

class ParseException final : public std::runtime_error {
public:
    ParseException(int column, const std::string& message)
        : std::runtime_error(message), column_(column) {}

    int column() const { return column_; }

private:
    int column_;
};

enum class TokenKind {
    Number,
    Identifier,
    Plus,
    Minus,
    Star,
    Slash,
    Percent,
    Caret,
    LeftParen,
    RightParen,
    Comma,
    End,
};

struct Token {
    TokenKind kind = TokenKind::End;
    double number = 0.0;
    std::string text;
    int column = 1;
};

std::string Trim(const std::string& value) {
    size_t begin = 0;
    while (begin < value.size() &&
           std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
        ++begin;
    }
    size_t end = value.size();
    while (end > begin &&
           std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }
    return value.substr(begin, end - begin);
}

class Lexer {
public:
    explicit Lexer(const std::string& source) : source_(source) {}

    Token Next() {
        SkipWhitespace();
        Token token;
        token.column = static_cast<int>(position_) + 1;
        if (position_ >= source_.size()) {
            return token;
        }

        const char current = source_[position_];
        switch (current) {
        case '+':
            ++position_;
            token.kind = TokenKind::Plus;
            return token;
        case '-':
            ++position_;
            token.kind = TokenKind::Minus;
            return token;
        case '*':
            ++position_;
            token.kind = TokenKind::Star;
            return token;
        case '/':
            ++position_;
            token.kind = TokenKind::Slash;
            return token;
        case '%':
            ++position_;
            token.kind = TokenKind::Percent;
            return token;
        case '^':
            ++position_;
            token.kind = TokenKind::Caret;
            return token;
        case '(':
            ++position_;
            token.kind = TokenKind::LeftParen;
            return token;
        case ')':
            ++position_;
            token.kind = TokenKind::RightParen;
            return token;
        case ',':
            ++position_;
            token.kind = TokenKind::Comma;
            return token;
        default:
            break;
        }

        const unsigned char unsignedCurrent = static_cast<unsigned char>(current);
        if (std::isdigit(unsignedCurrent) != 0 || current == '.') {
            return ReadNumber();
        }
        if (std::isalpha(unsignedCurrent) != 0 || current == '_') {
            return ReadIdentifier();
        }
        throw ParseException(token.column,
                             std::string("无法识别的字符 '") + current + "'");
    }

private:
    void SkipWhitespace() {
        while (position_ < source_.size() &&
               std::isspace(static_cast<unsigned char>(source_[position_])) != 0) {
            ++position_;
        }
    }

    Token ReadNumber() {
        const size_t begin = position_;
        int digitCount = 0;
        while (position_ < source_.size() &&
               std::isdigit(static_cast<unsigned char>(source_[position_])) != 0) {
            ++position_;
            ++digitCount;
        }
        if (position_ < source_.size() && source_[position_] == '.') {
            ++position_;
            while (position_ < source_.size() &&
                   std::isdigit(static_cast<unsigned char>(source_[position_])) != 0) {
                ++position_;
                ++digitCount;
            }
        }
        if (digitCount == 0) {
            throw ParseException(static_cast<int>(begin) + 1, "数字格式错误");
        }
        if (position_ < source_.size() &&
            (source_[position_] == 'e' || source_[position_] == 'E')) {
            const size_t exponentBegin = position_;
            ++position_;
            if (position_ < source_.size() &&
                (source_[position_] == '+' || source_[position_] == '-')) {
                ++position_;
            }
            const size_t exponentDigitsBegin = position_;
            while (position_ < source_.size() &&
                   std::isdigit(static_cast<unsigned char>(source_[position_])) != 0) {
                ++position_;
            }
            if (position_ == exponentDigitsBegin) {
                position_ = exponentBegin;
            }
        }

        Token token;
        token.kind = TokenKind::Number;
        token.column = static_cast<int>(begin) + 1;
        const std::string literal = source_.substr(begin, position_ - begin);
        token.number = std::strtod(literal.c_str(), nullptr);
        return token;
    }

    Token ReadIdentifier() {
        const size_t begin = position_;
        while (position_ < source_.size()) {
            const unsigned char current =
                static_cast<unsigned char>(source_[position_]);
            if (std::isalnum(current) == 0 && current != '_') {
                break;
            }
            ++position_;
        }
        Token token;
        token.kind = TokenKind::Identifier;
        token.column = static_cast<int>(begin) + 1;
        token.text = source_.substr(begin, position_ - begin);
        for (char& character : token.text) {
            character = static_cast<char>(
                std::tolower(static_cast<unsigned char>(character)));
        }
        return token;
    }

    const std::string& source_;
    size_t position_ = 0;
};

class Parser {
public:
    Parser(const std::string& source, std::set<std::string>& freeVariables)
        : lexer_(source), freeVariables_(freeVariables) {
        Advance();
    }

    ExprPtr Parse() {
        ExprPtr expression = ParseExpression();
        if (token_.kind != TokenKind::End) {
            throw ParseException(token_.column, "表达式末尾存在多余内容");
        }
        return expression;
    }

private:
    class DepthGuard {
    public:
        explicit DepthGuard(Parser& parser) : parser_(parser) {
            if (++parser_.depth_ > kMaximumDepth) {
                throw ParseException(parser_.token_.column,
                                     "表达式嵌套过深");
            }
        }

        ~DepthGuard() { --parser_.depth_; }

    private:
        Parser& parser_;
    };

    void Advance() { token_ = lexer_.Next(); }

    bool Match(TokenKind kind) {
        if (token_.kind != kind) {
            return false;
        }
        Advance();
        return true;
    }

    ExprPtr ParseExpression() {
        DepthGuard guard(*this);

        ExprPtr left = ParseTerm();
        for (;;) {
            if (Match(TokenKind::Plus)) {
                left = MakeBinaryExpr('+', std::move(left), ParseTerm());
            } else if (Match(TokenKind::Minus)) {
                left = MakeBinaryExpr('-', std::move(left), ParseTerm());
            } else {
                break;
            }
        }
        return left;
    }

    ExprPtr ParseTerm() {
        ExprPtr left = ParseUnary();
        for (;;) {
            if (Match(TokenKind::Star)) {
                left = MakeBinaryExpr('*', std::move(left), ParseUnary());
            } else if (Match(TokenKind::Slash)) {
                left = MakeBinaryExpr('/', std::move(left), ParseUnary());
            } else if (Match(TokenKind::Percent)) {
                left = MakeBinaryExpr('%', std::move(left), ParseUnary());
            } else if (StartsImplicitMultiplication()) {
                left = MakeBinaryExpr('*', std::move(left), ParseUnary());
            } else {
                break;
            }
        }
        return left;
    }

    bool StartsImplicitMultiplication() const {
        return token_.kind == TokenKind::Number ||
               token_.kind == TokenKind::Identifier ||
               token_.kind == TokenKind::LeftParen;
    }

    ExprPtr ParseUnary() {
        DepthGuard guard(*this);
        if (Match(TokenKind::Plus)) {
            return ParseUnary();
        }
        if (Match(TokenKind::Minus)) {
            return MakeUnaryExpr('-', ParseUnary());
        }
        return ParsePower();
    }

    ExprPtr ParsePower() {
        ExprPtr base = ParsePrimary();
        if (Match(TokenKind::Caret)) {
            return MakeBinaryExpr('^', std::move(base), ParseUnary());
        }
        return base;
    }

    ExprPtr ParsePrimary() {
        if (token_.kind == TokenKind::Number) {
            const double value = token_.number;
            Advance();
            return MakeNumberExpr(value);
        }

        if (token_.kind == TokenKind::Identifier) {
            return ParseIdentifier();
        }

        if (Match(TokenKind::LeftParen)) {
            ExprPtr expression = ParseExpression();
            if (!Match(TokenKind::RightParen)) {
                throw ParseException(token_.column, "缺少右括号");
            }
            return expression;
        }

        throw ParseException(token_.column, "缺少表达式");
    }

    ExprPtr ParseIdentifier() {
        const std::string name = token_.text;
        const int column = token_.column;
        Advance();

        if (name == "pi") {
            return MakeNumberExpr(3.14159265358979323846);
        }
        if (name == "e") {
            return MakeNumberExpr(2.71828182845904523536);
        }

        BuiltinFunction function = BuiltinFunction::Sin;
        if (FindBuiltinFunction(name, function)) {
            if (token_.kind != TokenKind::LeftParen) {
                throw ParseException(column, "函数 " + name + " 缺少括号");
            }
            Advance();

            std::vector<ExprPtr> arguments;
            if (token_.kind != TokenKind::RightParen) {
                arguments.push_back(ParseExpression());
                while (Match(TokenKind::Comma)) {
                    arguments.push_back(ParseExpression());
                }
            }
            if (!Match(TokenKind::RightParen)) {
                throw ParseException(token_.column,
                                     "函数 " + name + " 缺少右括号");
            }

            const int arity = BuiltinArity(function);
            if (static_cast<int>(arguments.size()) != arity) {
                throw ParseException(
                    column, "函数 " + name + " 需要 " +
                                std::to_string(arity) + " 个参数，实际为 " +
                                std::to_string(arguments.size()) + " 个");
            }
            return MakeCallExpr(function, std::move(arguments));
        }

        if (name != "x") {
            freeVariables_.insert(name);
        }
        return MakeVariableExpr(name);
    }

    Lexer lexer_;
    std::set<std::string>& freeVariables_;
    Token token_;
    int depth_ = 0;
};

} // namespace

ParseResult ParseExpression(const std::string& source) {
    ParseResult result;
    std::string text = Trim(source);
    if (text.empty()) {
        result.error = "表达式为空";
        return result;
    }

    try {
        const size_t equals = text.find('=');
        if (equals != std::string::npos) {
            std::string left = Trim(text.substr(0, equals));
            for (char& character : left) {
                character = static_cast<char>(
                    std::tolower(static_cast<unsigned char>(character)));
            }
            if (left != "y") {
                throw ParseException(static_cast<int>(equals) + 1,
                                     "仅支持 y=表达式 的形式");
            }
            text = Trim(text.substr(equals + 1));
            if (text.empty()) {
                throw ParseException(1, "等号右侧为空");
            }
        }

        std::set<std::string> freeVariables;
        Parser parser(text, freeVariables);
        result.expression = parser.Parse();
        result.freeVariables.assign(freeVariables.begin(), freeVariables.end());
    } catch (const ParseException& exception) {
        result.expression.reset();
        result.freeVariables.clear();
        result.error = "第 " + std::to_string(exception.column()) + " 列：" +
                       exception.what();
    } catch (const std::exception& exception) {
        result.expression.reset();
        result.freeVariables.clear();
        result.error = exception.what();
    }
    return result;
}

} // namespace core
