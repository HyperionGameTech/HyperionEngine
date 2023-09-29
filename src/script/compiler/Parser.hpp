#ifndef PARSER_HPP
#define PARSER_HPP

#include <script/compiler/TokenStream.hpp>
#include <script/SourceLocation.hpp>
#include <script/compiler/CompilationUnit.hpp>
#include <script/compiler/AstIterator.hpp>
#include <script/compiler/Keywords.hpp>
#include <script/compiler/Identifier.hpp>
#include <script/compiler/ast/AstModuleDeclaration.hpp>
#include <script/compiler/ast/AstDirective.hpp>
#include <script/compiler/ast/AstVariableDeclaration.hpp>
#include <script/compiler/ast/AstFunctionDefinition.hpp>
#include <script/compiler/ast/AstFunctionExpression.hpp>
#include <script/compiler/ast/AstArrayExpression.hpp>
#include <script/compiler/ast/AstTypeExpression.hpp>
#include <script/compiler/ast/AstEnumExpression.hpp>
#include <script/compiler/ast/AstTypeAlias.hpp>
#include <script/compiler/ast/AstStatement.hpp>
#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstImport.hpp>
#include <script/compiler/ast/AstExportStatement.hpp>
#include <script/compiler/ast/AstFileImport.hpp>
#include <script/compiler/ast/AstModuleImport.hpp>
#include <script/compiler/ast/AstInteger.hpp>
#include <script/compiler/ast/AstUnsignedInteger.hpp>
#include <script/compiler/ast/AstFloat.hpp>
#include <script/compiler/ast/AstString.hpp>
#include <script/compiler/ast/AstBinaryExpression.hpp>
#include <script/compiler/ast/AstUnaryExpression.hpp>
#include <script/compiler/ast/AstTernaryExpression.hpp>
#include <script/compiler/ast/AstCallExpression.hpp>
#include <script/compiler/ast/AstArgument.hpp>
#include <script/compiler/ast/AstArgumentList.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/ast/AstModuleAccess.hpp>
#include <script/compiler/ast/AstModuleProperty.hpp>
#include <script/compiler/ast/AstMember.hpp>
#include <script/compiler/ast/AstMemberCallExpression.hpp>
#include <script/compiler/ast/AstArrayAccess.hpp>
#include <script/compiler/ast/AstHasExpression.hpp>
#include <script/compiler/ast/AstIsExpression.hpp>
#include <script/compiler/ast/AstNewExpression.hpp>
#include <script/compiler/ast/AstTrue.hpp>
#include <script/compiler/ast/AstFalse.hpp>
#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/ast/AstBlock.hpp>
#include <script/compiler/ast/AstIfStatement.hpp>
#include <script/compiler/ast/AstWhileLoop.hpp>
#include <script/compiler/ast/AstForLoop.hpp>
#include <script/compiler/ast/AstTryCatch.hpp>
#include <script/compiler/ast/AstThrowExpression.hpp>
#include <script/compiler/ast/AstPrototypeSpecification.hpp>
#include <script/compiler/ast/AstTypeOfExpression.hpp>
#include <script/compiler/ast/AstReturnStatement.hpp>
#include <script/compiler/ast/AstSymbolQuery.hpp>
#include <script/compiler/ast/AstTemplateExpression.hpp>
#include <script/compiler/ast/AstTemplateInstantiation.hpp>

#include <string>

namespace hyperion::compiler {

class Parser
{
public:
    Parser(
        AstIterator *ast_iterator,
        TokenStream *token_stream,
        CompilationUnit *compilation_unit
    );

    Parser(const Parser &other);

    void Parse(bool expect_module_decl = true);

private:
    int m_template_argument_depth = 0; // until a better way is found..

    AstIterator *m_ast_iterator;
    TokenStream *m_token_stream;
    CompilationUnit *m_compilation_unit;

    Token Match(TokenClass token_class, bool read = false);
    Token MatchAhead(TokenClass token_class, int n);
    Token MatchKeyword(Keywords keyword, bool read = false);
    Token MatchKeywordAhead(Keywords keyword, int n);
    Token MatchOperator(const std::string &op, bool read = false);
    Token MatchOperatorAhead(const std::string &op, int n);
    Token Expect(TokenClass token_class, bool read = false);
    Token ExpectKeyword(Keywords keyword, bool read = false);
    Token ExpectOperator(const std::string &op, bool read = false);
    Token MatchIdentifier(bool allow_keyword = false, bool read = false);
    Token ExpectIdentifier(bool allow_keyword = false, bool read = false);
    bool ExpectEndOfStmt();
    SourceLocation CurrentLocation() const;
    void SkipStatementTerminators();
    int OperatorPrecedence(const Operator *&out);

    RC<AstStatement> ParseStatement(
        bool top_level = false,
        bool read_terminators = true
    );
    RC<AstModuleDeclaration> ParseModuleDeclaration();
    RC<AstDirective> ParseDirective();
    RC<AstExpression> ParseTerm(
        bool override_commas = false,
        bool override_fat_arrows = false,
        bool override_angle_brackets = false,
        bool override_square_brackets = false,
        bool override_parentheses = false,
        bool override_question_mark = false
    );
    RC<AstExpression> ParseParentheses();
    RC<AstExpression> ParseAngleBrackets(RC<AstIdentifier> target);
    RC<AstConstant> ParseIntegerLiteral();
    RC<AstFloat> ParseFloatLiteral();
    RC<AstString> ParseStringLiteral();
    RC<AstIdentifier> ParseIdentifier(bool allow_keyword = false);
    RC<AstArgument> ParseArgument(RC<AstExpression> expr);
    RC<AstArgumentList> ParseArguments(bool require_parentheses = true);
    RC<AstCallExpression> ParseCallExpression(RC<AstExpression> target,
        bool require_parentheses = true);
    RC<AstModuleAccess> ParseModuleAccess();
    RC<AstModuleProperty> ParseModuleProperty();
    RC<AstMember> ParseMemberExpression(RC<AstExpression> target);
    RC<AstArrayAccess> ParseArrayAccess(RC<AstExpression> target);
    RC<AstHasExpression> ParseHasExpression(RC<AstExpression> target);
    RC<AstIsExpression> ParseIsExpression(RC<AstExpression> target);
    RC<AstNewExpression> ParseNewExpression();
    RC<AstTrue> ParseTrue();
    RC<AstFalse> ParseFalse();
    RC<AstNil> ParseNil();
    RC<AstBlock> ParseBlock();
    RC<AstIfStatement> ParseIfStatement();
    RC<AstWhileLoop> ParseWhileLoop();
    RC<AstStatement> ParseForLoop();
    RC<AstTryCatch> ParseTryCatchStatement();
    RC<AstThrowExpression> ParseThrowExpression();
    RC<AstExpression> ParseBinaryExpression(
        int expr_prec,
        RC<AstExpression> left
    );
    RC<AstExpression> ParseUnaryExpressionPrefix();
    RC<AstExpression> ParseUnaryExpressionPostfix(const RC<AstExpression> &expr);
    RC<AstExpression> ParseTernaryExpression(
        const RC<AstExpression> &conditional
    );
    RC<AstExpression> ParseExpression(
        bool override_commas = false,
        bool override_fat_arrows = false,
        bool override_angle_brackets = false,
        bool override_question_mark = false
    );
    RC<AstPrototypeSpecification> ParsePrototypeSpecification();
    RC<AstExpression> ParseAssignment();
    RC<AstVariableDeclaration> ParseVariableDeclaration(
        bool allow_keyword_names = false,
        bool allow_quoted_names = false,
        IdentifierFlagBits flags = 0
    );
    RC<AstStatement> ParseFunctionDefinition(bool require_keyword = true);
    RC<AstFunctionExpression> ParseFunctionExpression(
        bool require_keyword = true,
        Array<RC<AstParameter>> params = {}
    );
    RC<AstArrayExpression> ParseArrayExpression();
    RC<AstExpression> ParseValueOfExpression();
    RC<AstTypeOfExpression> ParseTypeOfExpression();
    Array<RC<AstParameter>> ParseFunctionParameters();
    Array<RC<AstParameter>> ParseGenericParameters();
    RC<AstStatement> ParseTypeDefinition();
    RC<AstTypeExpression> ParseTypeExpression(
        bool require_keyword = true,
        bool allow_identifier = true,
        bool is_proxy_class = false,
        std::string type_name = "<Anonymous Type>"
    );
    RC<AstStatement> ParseEnumDefinition();
    RC<AstEnumExpression> ParseEnumExpression(
        bool require_keyword = true,
        bool allow_identifier = true,
        std::string enum_name = "<Anonymous Enum>"
    );
    RC<AstImport> ParseImport();
    RC<AstExportStatement> ParseExportStatement();
    RC<AstFileImport> ParseFileImport();
    RC<AstModuleImport> ParseModuleImport();
    RC<AstModuleImportPart> ParseModuleImportPart(bool allow_braces = false);
    RC<AstReturnStatement> ParseReturnStatement();
    RC<AstExpression> ParseMetaProperty();
};

} // namespace hyperion::compiler

#endif
