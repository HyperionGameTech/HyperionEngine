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

namespace Hyperion::DataProcessing::JSON {
class Value;
} // namespace Hyperion::DataProcessing::JSON

namespace Hyperion::CodeGen {

namespace JSON = DataProcessing::JSON;

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

struct StrataTypeMapping
{
    String typeName;      // Name of the type in Strata
    bool isHandle = false; // True when typeName names an engine object handle
};

extern const HypScriptTypeMapping g_hypscriptAnyTypeMapping;

TResult<CSharpTypeMapping> MapToCSharpType(const Analyzer& analyzer, const ASTType* type);
TResult<HypScriptTypeMapping> MapToHypScriptType(const Analyzer& analyzer, const ASTType* type);
TResult<StrataTypeMapping> MapToStrataType(const Analyzer& analyzer, const ASTType* type);

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

    SharedPtr<ASTExpr> expr;
    const Operator* op;
    bool isPrefix;

    virtual void ToJSON(JSON::Value& out) const override;
    virtual String ToString() const override;
};

struct ASTBinExpr : ASTExpr
{
    virtual ~ASTBinExpr() override = default;

    SharedPtr<ASTExpr> left;
    SharedPtr<ASTExpr> right;
    const Operator* op;

    virtual void ToJSON(JSON::Value& out) const override;
    virtual String ToString() const override;
};

struct ASTTernaryExpr : ASTExpr
{
    virtual ~ASTTernaryExpr() override = default;

    SharedPtr<ASTExpr> conditional;
    SharedPtr<ASTExpr> trueExpr;
    SharedPtr<ASTExpr> falseExpr;

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

    Array<SharedPtr<ASTExpr>> values;

    virtual void ToJSON(JSON::Value& out) const override;
    virtual String ToString() const override;
};

struct ASTTemplateArgument : ASTNode
{
    virtual ~ASTTemplateArgument() override = default;

    SharedPtr<ASTType> type;
    SharedPtr<ASTExpr> expr;

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
    SharedPtr<ASTType> ptrTo;
    SharedPtr<ASTType> refTo;
    SharedPtr<ASTType> arrayOf;
    Optional<QualifiedName> typeName;

    // Inner value for array - may be null
    SharedPtr<ASTExpr> arrayExpr;

    Array<SharedPtr<ASTTemplateArgument>> templateArguments;

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
    SharedPtr<ASTType> type;
    SharedPtr<ASTExpr> value;

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

    SharedPtr<ASTType> returnType;
    Array<SharedPtr<ASTMemberDecl>> parameters;

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

    SharedPtr<ASTExpr> ParseExpr();
    SharedPtr<ASTExpr> ParseTerm();
    SharedPtr<ASTExpr> ParseUnaryExprPrefix();
    SharedPtr<ASTExpr> ParseUnaryExprPostfix(const SharedPtr<ASTExpr>& innerExpr);
    SharedPtr<ASTExpr> ParseBinaryExpr(int exprPrecedence, SharedPtr<ASTExpr> left);
    SharedPtr<ASTExpr> ParseTernaryExpr(const SharedPtr<ASTExpr>& conditional);
    SharedPtr<ASTExpr> ParseParentheses();
    SharedPtr<ASTExpr> ParseLiteralString();
    SharedPtr<ASTExpr> ParseLiteralInt();
    SharedPtr<ASTExpr> ParseLiteralFloat();
    SharedPtr<ASTIdentifier> ParseIdentifier();
    SharedPtr<ASTInitializerExpr> ParseInitializerExpr();
    SharedPtr<ASTMemberDecl> ParseMemberDecl();
    SharedPtr<ASTMemberDecl> ParseEnumMemberDecl(const SharedPtr<ASTType>& underlyingType);
    SharedPtr<ASTType> ParseType();
    SharedPtr<ASTFunctionType> ParseFunctionType(const SharedPtr<ASTType>& returnType);

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