#ifndef HYPERION_BUILDTOOL_PARSER_HPP
#define HYPERION_BUILDTOOL_PARSER_HPP

#include <parser/TokenStream.hpp>
#include <parser/SourceLocation.hpp>
#include <parser/CompilationUnit.hpp>
#include <parser/Operator.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/utilities/Optional.hpp>
#include <core/utilities/Variant.hpp>
#include <core/utilities/Result.hpp>

namespace hyperion::json {
class JSONValue;
} // namespace hyperion::json

namespace hyperion::buildtool {

struct QualifiedName
{
    Array<String> parts;
    bool is_global = false;

    String ToString(bool include_namespace = true) const;
};

class ASTType;
TResult<String> MapToCSharpType(const ASTType* type);

struct ASTNode
{
    virtual ~ASTNode() = default;

    virtual void ToJSON(json::JSONValue& out) const = 0;
};

struct ASTExpr : ASTNode
{
    virtual ~ASTExpr() override = default;

    virtual void ToJSON(json::JSONValue& out) const override = 0;
};

struct ASTUnaryExpr : ASTExpr
{
    virtual ~ASTUnaryExpr() override = default;

    RC<ASTExpr> expr;
    const Operator* op;
    bool is_prefix;

    virtual void ToJSON(json::JSONValue& out) const override;
};

struct ASTBinExpr : ASTExpr
{
    virtual ~ASTBinExpr() override = default;

    RC<ASTExpr> left;
    RC<ASTExpr> right;
    const Operator* op;

    virtual void ToJSON(json::JSONValue& out) const override;
};

struct ASTTernaryExpr : ASTExpr
{
    virtual ~ASTTernaryExpr() override = default;

    RC<ASTExpr> conditional;
    RC<ASTExpr> true_expr;
    RC<ASTExpr> false_expr;

    virtual void ToJSON(json::JSONValue& out) const override;
};

struct ASTLiteralString : ASTExpr
{
    virtual ~ASTLiteralString() override = default;

    String value;

    virtual void ToJSON(json::JSONValue& out) const override;
};

struct ASTLiteralInt : ASTExpr
{
    virtual ~ASTLiteralInt() override = default;

    int value;

    virtual void ToJSON(json::JSONValue& out) const override;
};

struct ASTLiteralFloat : ASTExpr
{
    virtual ~ASTLiteralFloat() override = default;

    double value;

    virtual void ToJSON(json::JSONValue& out) const override;
};

struct ASTLiteralBool : ASTExpr
{
    virtual ~ASTLiteralBool() override = default;

    bool value;

    virtual void ToJSON(json::JSONValue& out) const override;
};

struct ASTIdentifier : ASTExpr
{
    virtual ~ASTIdentifier() override = default;

    QualifiedName name;

    virtual void ToJSON(json::JSONValue& out) const override;
};

struct ASTInitializerExpr : ASTExpr
{
    virtual ~ASTInitializerExpr() override = default;

    Array<RC<ASTExpr>> values;

    virtual void ToJSON(json::JSONValue& out) const override;
};

struct ASTTemplateArgument : ASTNode
{
    virtual ~ASTTemplateArgument() override = default;

    RC<ASTType> type;
    RC<ASTExpr> expr;

    virtual void ToJSON(json::JSONValue& out) const override;
};

struct ASTType : ASTNode
{
    virtual ~ASTType() override = default;

    bool is_const = false;
    bool is_volatile = false;
    bool is_virtual = false;
    bool is_inline = false;
    bool is_static = false;
    bool is_thread_local = false;
    bool is_constexpr = false;
    bool is_lvalue_reference = false;
    bool is_rvalue_reference = false;
    bool is_pointer = false;
    bool is_array = false;
    bool is_template = false;
    bool is_function_pointer = false;
    bool is_function = false;

    // One of the below is set
    RC<ASTType> ptr_to;
    RC<ASTType> ref_to;
    RC<ASTType> array_of;
    Optional<QualifiedName> type_name;

    // Inner value for array - may be null
    RC<ASTExpr> array_expr;

    Array<RC<ASTTemplateArgument>> template_arguments;

    HYP_FORCE_INLINE bool IsVoid() const
    {
        return type_name.HasValue()
            && type_name->parts.Size() == 1
            && type_name->parts[0] == "void";
    }

    HYP_FORCE_INLINE bool IsScriptableDelegate() const
    {
        return type_name.HasValue()
            && type_name->parts.Any()
            && type_name->parts.Back() == "ScriptableDelegate"
            && is_template;
    }

    virtual String Format(bool use_csharp_syntax = false) const;
    virtual String FormatDecl(const String& decl_name, bool use_csharp_syntax = false) const;

    virtual void ToJSON(json::JSONValue& out) const override;
};

struct ASTMemberDecl : ASTNode
{
    virtual ~ASTMemberDecl() override = default;

    String name;
    RC<ASTType> type;
    RC<ASTExpr> value;

    virtual void ToJSON(json::JSONValue& out) const override;
};

struct ASTFunctionType : ASTType
{
    ASTFunctionType()
    {
        is_function = true;
    }

    virtual ~ASTFunctionType() override = default;

    bool is_const_method = false;
    bool is_override_method = false;
    bool is_noexcept_method = false;
    bool is_defaulted_method = false;
    bool is_deleted_method = false;
    bool is_pure_virtual_method = false;
    bool is_rvalue_method = false;
    bool is_lvalue_method = false;

    RC<ASTType> return_type;
    Array<RC<ASTMemberDecl>> parameters;

    virtual String Format(bool use_csharp_syntax = false) const override;
    virtual String FormatDecl(const String& decl_name, bool use_csharp_syntax = false) const override;

    virtual void ToJSON(json::JSONValue& out) const override;
};

class Parser
{
public:
    Parser(
        TokenStream* token_stream,
        CompilationUnit* compilation_unit);

    Parser(const Parser& other) = delete;
    Parser& operator=(const Parser& other) = delete;

    QualifiedName ReadQualifiedName();

    RC<ASTExpr> ParseExpr();
    RC<ASTExpr> ParseTerm();
    RC<ASTExpr> ParseUnaryExprPrefix();
    RC<ASTExpr> ParseUnaryExprPostfix(const RC<ASTExpr>& inner_expr);
    RC<ASTExpr> ParseBinaryExpr(int expr_precedence, RC<ASTExpr> left);
    RC<ASTExpr> ParseTernaryExpr(const RC<ASTExpr>& conditional);
    RC<ASTExpr> ParseParentheses();
    RC<ASTExpr> ParseLiteralString();
    RC<ASTExpr> ParseLiteralInt();
    RC<ASTExpr> ParseLiteralFloat();
    RC<ASTIdentifier> ParseIdentifier();
    RC<ASTInitializerExpr> ParseInitializerExpr();
    RC<ASTMemberDecl> ParseMemberDecl();
    RC<ASTType> ParseType();
    RC<ASTFunctionType> ParseFunctionType(const RC<ASTType>& return_type);

private:
    int m_template_argument_depth;

    TokenStream* m_token_stream;
    CompilationUnit* m_compilation_unit;

    Token Match(TokenClass token_class, bool read = false);
    Token MatchAhead(TokenClass token_class, int n);
    Token MatchOperator(const String& op, bool read = false);
    Token MatchOperatorAhead(const String& op, int n);
    Token Expect(TokenClass token_class, bool read = false);
    Token ExpectOperator(const String& op, bool read = false);
    Token MatchIdentifier(const UTF8StringView& value = UTF8StringView(), bool read = false);
    Token ExpectIdentifier(const UTF8StringView& value = UTF8StringView(), bool read = false);
    bool ExpectEndOfStmt();
    SourceLocation CurrentLocation() const;
    void SkipStatementTerminators();
    int OperatorPrecedence(const Operator*& out);
};

} // namespace hyperion::buildtool

#endif