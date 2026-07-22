#ifndef HYPERION_CODEGEN_PARSER_HPP
#define HYPERION_CODEGEN_PARSER_HPP

#include <parser/TokenStream.hpp>
#include <parser/SourceLocation.hpp>
#include <parser/CompilationUnit.hpp>
#include <parser/Operator.hpp>

#include <Core/Memory/SharedPtr.hpp>

#include <Core/Utilities/Optional.hpp>
#include <Core/Utilities/Variant.hpp>
#include <Core/Utilities/Result.hpp>

namespace Hyperion::JSON {
class Value;
} // namespace Hyperion::JSON

namespace Hyperion::CodeGen {

class Analyzer;

struct QualifiedName
{
    Array<String> parts;
    bool isGlobal = false;

    String ToString(bool includeNamespace = true) const;
};

struct ASTType;

struct CSharpTypeMapping
{
    String typeName;                   // Name of the type in C#
    Optional<String> getValueOverload; // Method to use instead of GetValue() if a specific one is defined in BoxedValue.cs
    bool isNullable = false;           // Whether the type should be nullable in C#
};

struct HypScriptTypeMapping
{
    String typeName; // Name of the type in HypScript
};

extern const HypScriptTypeMapping g_hypscriptAnyTypeMapping;

TResult<CSharpTypeMapping> MapToCSharpType(const Analyzer& analyzer, const ASTType* type);
TResult<HypScriptTypeMapping> MapToHypScriptType(const Analyzer& analyzer, const ASTType* type);

struct ASTNode
{
    virtual ~ASTNode() = default;

    virtual void ToJSON(JSON::Value& out) const = 0;
    virtual String ToString() const
    {
        return String::empty;
    }
};

struct ASTExpr : ASTNode
{
    virtual ~ASTExpr() override = default;

    virtual void ToJSON(JSON::Value& out) const override = 0;
    virtual String ToString() const override = 0;
};

struct ASTUnaryExpr : ASTExpr
{
    virtual ~ASTUnaryExpr() override = default;

    Handle<AstExpr> expr;
    const Operator* op;
    bool isPrefix;

    virtual void ToJSON(JSON::Value& out) const override;
    virtual String ToString() const override;
};

struct ASTBinExpr : ASTExpr
{
    virtual ~ASTBinExpr() override = default;

    Handle<AstExpr> left;
    Handle<AstExpr> right;
    const Operator* op;

    virtual void ToJSON(JSON::Value& out) const override;
    virtual String ToString() const override;
};

struct ASTTernaryExpr : ASTExpr
{
    virtual ~ASTTernaryExpr() override = default;

    Handle<AstExpr> conditional;
    Handle<AstExpr> trueExpr;
    Handle<AstExpr> falseExpr;

    virtual void ToJSON(JSON::Value& out) const override;
    virtual String ToString() const override;
};

struct ASTLiteralString : ASTExpr
{
    virtual ~ASTLiteralString() override = default;

    String value;

    virtual void ToJSON(JSON::Value& out) const override;

    virtual String ToString() const override
    {
        return "\"" + value + "\"";
    }
};

struct ASTLiteralInt : ASTExpr
{
    virtual ~ASTLiteralInt() override = default;

    int64 value;
    String originalString;

    virtual void ToJSON(JSON::Value& out) const override;
    virtual String ToString() const override;
};

struct ASTLiteralFloat : ASTExpr
{
    virtual ~ASTLiteralFloat() override = default;

    double value;
    String originalString;

    virtual void ToJSON(JSON::Value& out) const override;
    virtual String ToString() const override;
};

struct ASTLiteralBool : ASTExpr
{
    virtual ~ASTLiteralBool() override = default;

    bool value;

    virtual void ToJSON(JSON::Value& out) const override;

    virtual String ToString() const override
    {
        return value ? "true" : "false";
    }
};

struct ASTIdentifier : ASTExpr
{
    virtual ~ASTIdentifier() override = default;

    QualifiedName name;

    virtual void ToJSON(JSON::Value& out) const override;
    virtual String ToString() const override;
};

struct ASTInitializerExpr : ASTExpr
{
    virtual ~ASTInitializerExpr() override = default;

    Array<Handle<AstExpr>> values;

    virtual void ToJSON(JSON::Value& out) const override;
    virtual String ToString() const override;
};

struct ASTTemplateArgument : ASTNode
{
    virtual ~ASTTemplateArgument() override = default;

    Handle<AstType> type;
    Handle<AstExpr> expr;

    virtual void ToJSON(JSON::Value& out) const override;
    virtual String ToString() const override;
};

struct ASTType : ASTNode
{
    virtual ~ASTType() override = default;

    bool isConst = false;
    bool isVolatile = false;
    bool isMutable = false;
    bool isVirtual = false;
    bool isInline = false;
    bool isStatic = false;
    bool isThreadLocal = false;
    bool isConstexpr = false;
    bool isLvalueReference = false;
    bool isRvalueReference = false;
    bool isPointer = false;
    bool isArray = false;
    bool isTemplate = false;
    bool isFunctionPointer = false;
    bool isFunction = false;

    // One of the below is set
    Handle<AstType> ptrTo;
    Handle<AstType> refTo;
    Handle<AstType> arrayOf;
    Optional<QualifiedName> typeName;

    // Inner value for array - may be null
    Handle<AstExpr> arrayExpr;

    Array<Handle<AstTemplateArgument>> templateArguments;

    HYP_FORCE_INLINE bool IsVoid() const
    {
        return typeName.HasValue()
            && typeName->parts.Size() == 1
            && typeName->parts[0] == "void";
    }

    HYP_FORCE_INLINE bool IsChar() const
    {
        return typeName.HasValue()
            && typeName->parts.Size() == 1
            && (typeName->parts[0] == "char");
    }

    HYP_FORCE_INLINE bool IsScriptableDelegate() const
    {
        return typeName.HasValue()
            && typeName->parts.Any()
            && typeName->parts.Back() == "ScriptableDelegate"
            && isTemplate;
    }

    virtual String Format(bool useCsharpSyntax = false) const;
    virtual String FormatDecl(const String& declName, bool useCsharpSyntax = false) const;

    virtual void ToJSON(JSON::Value& out) const override;
    virtual String ToString() const override;
};

struct ASTMemberDecl : ASTNode
{
    virtual ~ASTMemberDecl() override = default;

    String name;
    Handle<AstType> type;
    Handle<AstExpr> value;

    virtual void ToJSON(JSON::Value& out) const override;
    virtual String ToString() const override;
};

struct ASTFunctionType : ASTType
{
    ASTFunctionType()
    {
        isFunction = true;
    }

    virtual ~ASTFunctionType() override = default;

    bool isConstMethod = false;
    bool isOverrideMethod = false;
    bool isNoexceptMethod = false;
    bool isDefaultedMethod = false;
    bool isDeletedMethod = false;
    bool isPureVirtualMethod = false;
    bool isRvalueMethod = false;
    bool isLvalueMethod = false;

    Handle<AstType> returnType;
    Array<Handle<AstMemberDecl>> parameters;

    virtual String Format(bool useCsharpSyntax = false) const override;
    virtual String FormatDecl(const String& declName, bool useCsharpSyntax = false) const override;

    virtual void ToJSON(JSON::Value& out) const override;
    virtual String ToString() const override;
};

class Parser
{
public:
    Parser(
        TokenStream* tokenStream,
        CompilationUnit* compilationUnit);

    Parser(const Parser& other) = delete;
    Parser& operator=(const Parser& other) = delete;

    QualifiedName ReadQualifiedName();

    Handle<AstExpr> ParseExpr();
    Handle<AstExpr> ParseTerm();
    Handle<AstExpr> ParseUnaryExprPrefix();
    Handle<AstExpr> ParseUnaryExprPostfix(const Handle<AstExpr>& innerExpr);
    Handle<AstExpr> ParseBinaryExpr(int exprPrecedence, Handle<AstExpr> left);
    Handle<AstExpr> ParseTernaryExpr(const Handle<AstExpr>& conditional);
    Handle<AstExpr> ParseParentheses();
    Handle<AstExpr> ParseLiteralString();
    Handle<AstExpr> ParseLiteralInt();
    Handle<AstExpr> ParseLiteralFloat();
    Handle<AstIdentifier> ParseIdentifier();
    Handle<AstInitializerExpr> ParseInitializerExpr();
    Handle<AstMemberDecl> ParseMemberDecl();
    Handle<AstMemberDecl> ParseEnumMemberDecl(const Handle<AstType>& underlyingType);
    Handle<AstType> ParseType();
    Handle<AstFunctionType> ParseFunctionType(const Handle<AstType>& returnType);

    Token Match(TokenClass tokenClass, bool read = false);
    Token MatchAhead(TokenClass tokenClass, int n);
    Token MatchOperator(const String& op, bool read = false);
    Token MatchOperatorAhead(const String& op, int n);
    Token Expect(TokenClass tokenClass, bool read = false);
    Token ExpectOperator(const String& op, bool read = false);
    Token MatchIdentifier(const UTF8StringView& value = UTF8StringView(), bool read = false);
    Token ExpectIdentifier(const UTF8StringView& value = UTF8StringView(), bool read = false);
    bool ExpectEndOfStmt();
    SourceLocation CurrentLocation() const;
    void SkipStatementTerminators();
    int OperatorPrecedence(const Operator*& out);

private:
    int m_templateArgumentDepth;

    TokenStream* m_tokenStream;
    CompilationUnit* m_compilationUnit;
};

} // namespace Hyperion::CodeGen

#endif