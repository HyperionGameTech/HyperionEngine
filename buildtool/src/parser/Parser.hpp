#ifndef HYPERION_BUILDTOOL_PARSER_HPP
#define HYPERION_BUILDTOOL_PARSER_HPP

#include <parser/TokenStream.hpp>
#include <parser/SourceLocation.hpp>
#include <parser/CompilationUnit.hpp>
#include <parser/Operator.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/utilities/Optional.hpp>

namespace hyperion::json {
class JSONValue;
} // namespace hyperion::json

namespace hyperion::buildtool {

struct TypeName
{
    Array<String>   parts;
    bool            is_global = false;
};

struct ASTNode
{
    virtual ~ASTNode() = default;

    virtual void ToJSON(json::JSONValue &out) const = 0;
};

struct ASTType : ASTNode
{
    virtual ~ASTType() override = default;

    bool                                is_const = false;
    bool                                is_volatile = false;
    bool                                is_static = false;
    bool                                is_inline = false;
    bool                                is_virtual = false;
    bool                                is_lvalue_reference = false;
    bool                                is_rvalue_reference = false;
    bool                                is_pointer = false;
    bool                                is_array = false;
    bool                                is_template = false;
    bool                                is_function_pointer = false;
    bool                                is_function = false;

    int                                 array_size = -1;

    // One of the below is set

    RC<ASTType>                         ptr_to;
    RC<ASTType>                         ref_to;
    Optional<TypeName>                  type_name;

    Array<RC<ASTType>>                  template_arguments;

    virtual void ToJSON(json::JSONValue &out) const override;
};

struct ASTFunctionType : ASTType
{
    ASTFunctionType()
    {
        is_function = true;
    }

    virtual ~ASTFunctionType() override = default;

    bool                                is_const_method = false;
    bool                                is_override_method = false;
    bool                                is_noexcept_method = false;
    bool                                is_rvalue_method = false;
    bool                                is_lvalue_method = false;

    RC<ASTType>                         return_type;
    Array<Pair<String, RC<ASTType>>>    parameters;

    virtual void ToJSON(json::JSONValue &out) const override;
};

struct ASTMemberDecl : ASTNode
{
    virtual ~ASTMemberDecl() override = default;

    RC<ASTType> type;
    String      name;

    virtual void ToJSON(json::JSONValue &out) const override;
};

class Parser
{
public:
    Parser(
        TokenStream *token_stream,
        CompilationUnit *compilation_unit
    );

    Parser(const Parser &other)             = delete;
    Parser &operator=(const Parser &other)  = delete;

    TypeName ParseTypeName();

    RC<ASTMemberDecl> ParseMemberDecl();
    RC<ASTType> ParseType();
    RC<ASTFunctionType> ParseFunctionType(const RC<ASTType> &return_type);

private:
    int m_template_argument_depth = 0; // until a better way is found..

    TokenStream     *m_token_stream;
    CompilationUnit *m_compilation_unit;

    Token Match(TokenClass token_class, bool read = false);
    Token MatchAhead(TokenClass token_class, int n);
    Token MatchOperator(const String &op, bool read = false);
    Token MatchOperatorAhead(const String &op, int n);
    Token Expect(TokenClass token_class, bool read = false);
    Token ExpectOperator(const String &op, bool read = false);
    Token MatchIdentifier(const UTF8StringView &value = UTF8StringView(), bool read = false);
    Token ExpectIdentifier(const UTF8StringView &value = UTF8StringView(), bool read = false);
    bool ExpectEndOfStmt();
    SourceLocation CurrentLocation() const;
    void SkipStatementTerminators();
    int OperatorPrecedence(const Operator *&out);
};

} // namespace hyperion::buildtool

#endif