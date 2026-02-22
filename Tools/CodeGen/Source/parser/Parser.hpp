#ifndef HYPERION_CODEGEN_PARSER_HPP
#define HYPERION_CODEGEN_PARSER_HPP

#include <parser/TokenStream.hpp>
#include <parser/SourceLocation.hpp>
#include <parser/CompilationUnit.hpp>
#include <parser/Operator.hpp>

#include <Core/memory/RefCountedPtr.hpp>

#include <Core/utilities/Optional.hpp>
#include <Core/utilities/Variant.hpp>
#include <Core/utilities/Result.hpp>

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

    RC<ASTExpr> expr;
    const Operator* op;
    bool isPrefix;

    virtual void ToJSON(JSON::Value& out) const override;
    virtual String ToString() const override;
};

struct ASTBinExpr : ASTExpr
{
    virtual ~ASTBinExpr() override = default;

    RC<ASTExpr> left;
    RC<ASTExpr> right;
    const Operator* op;

    virtual void ToJSON(JSON::Value& out) const override;
    virtual String ToString() const override;
};

struct ASTTernaryExpr : ASTExpr
{
    virtual ~ASTTernaryExpr() override = default;

    RC<ASTExpr> conditional;
    RC<ASTExpr> trueExpr;
    RC<ASTExpr> falseExpr;

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

    Array<RC<ASTExpr>> values;

    virtual void ToJSON(JSON::Value& out) const override;
    virtual String ToString() const override;
};

struct ASTTemplateArgument : ASTNode
{
    virtual ~ASTTemplateArgument() override = default;

    RC<ASTType> type;
    RC<ASTExpr> expr;

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
    RC<ASTType> ptrTo;
    RC<ASTType> refTo;
    RC<ASTType> arrayOf;
    Optional<QualifiedName> typeName;

    // Inner value for array - may be null
    RC<ASTExpr> arrayExpr;

    Array<RC<ASTTemplateArgument>> templateArguments;

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
    RC<ASTType> type;
    RC<ASTExpr> value;

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

    RC<ASTType> returnType;
    Array<RC<ASTMemberDecl>> parameters;

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

    RC<ASTExpr> ParseExpr();
    RC<ASTExpr> ParseTerm();
    RC<ASTExpr> ParseUnaryExprPrefix();
    RC<ASTExpr> ParseUnaryExprPostfix(const RC<ASTExpr>& innerExpr);
    RC<ASTExpr> ParseBinaryExpr(int exprPrecedence, RC<ASTExpr> left);
    RC<ASTExpr> ParseTernaryExpr(const RC<ASTExpr>& conditional);
    RC<ASTExpr> ParseParentheses();
    RC<ASTExpr> ParseLiteralString();
    RC<ASTExpr> ParseLiteralInt();
    RC<ASTExpr> ParseLiteralFloat();
    RC<ASTIdentifier> ParseIdentifier();
    RC<ASTInitializerExpr> ParseInitializerExpr();
    RC<ASTMemberDecl> ParseMemberDecl();
    RC<ASTMemberDecl> ParseEnumMemberDecl(const RC<ASTType>& underlyingType);
    RC<ASTType> ParseType();
    RC<ASTFunctionType> ParseFunctionType(const RC<ASTType>& returnType);

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