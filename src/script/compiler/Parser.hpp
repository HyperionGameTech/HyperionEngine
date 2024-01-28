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
#include <script/compiler/ast/AstHashMap.hpp>
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
#include <script/compiler/ast/AstBreakStatement.hpp>
#include <script/compiler/ast/AstContinueStatement.hpp>
#include <script/compiler/ast/AstMemberCallExpression.hpp>
#include <script/compiler/ast/AstArrayAccess.hpp>
#include <script/compiler/ast/AstHasExpression.hpp>
#include <script/compiler/ast/AstIsExpression.hpp>
#include <script/compiler/ast/AstAsExpression.hpp>
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

    void Parse(Bool expect_module_decl = true);

    RC<AstStatement> ParseStatement(
        Bool top_level = false,
        Bool read_terminators = true
    );
    RC<AstModuleDeclaration> ParseModuleDeclaration();
    RC<AstDirective> ParseDirective();
    RC<AstExpression> ParseTerm(
        Bool override_commas = false,
        Bool override_fat_arrows = false,
        Bool override_angle_brackets = false,
        Bool override_square_brackets = false,
        Bool override_parentheses = false,
        Bool override_question_mark = false
    );
    RC<AstExpression> ParseParentheses();
    RC<AstTemplateInstantiation> ParseTemplateInstantiation(RC<AstExpression> expr);
    RC<AstExpression> ParseAngleBrackets(RC<AstExpression> target);
    RC<AstConstant> ParseIntegerLiteral();
    RC<AstFloat> ParseFloatLiteral();
    RC<AstString> ParseStringLiteral();
    RC<AstIdentifier> ParseIdentifier(Bool allow_keyword = false);
    RC<AstArgument> ParseArgument(RC<AstExpression> expr);
    RC<AstArgumentList> ParseArguments(Bool require_parentheses = true);
    RC<AstCallExpression> ParseCallExpression(
        RC<AstExpression> target,
        Bool require_parentheses = true
    );
    RC<AstModuleAccess> ParseModuleAccess();
    RC<AstModuleProperty> ParseModuleProperty();
    RC<AstExpression> ParseMemberExpression(RC<AstExpression> target);
    RC<AstArrayAccess> ParseArrayAccess(
        RC<AstExpression> target,
        Bool override_commas = false,
        Bool override_fat_arrows = false,
        Bool override_angle_brackets = false,
        Bool override_square_brackets = false,
        Bool override_parentheses = false,
        Bool override_question_mark = false
    );
    RC<AstHasExpression> ParseHasExpression(RC<AstExpression> target);
    RC<AstIsExpression> ParseIsExpression(RC<AstExpression> target);
    RC<AstAsExpression> ParseAsExpression(RC<AstExpression> target);
    RC<AstNewExpression> ParseNewExpression();
    RC<AstTrue> ParseTrue();
    RC<AstFalse> ParseFalse();
    RC<AstNil> ParseNil();
    RC<AstBlock> ParseBlock(bool require_braces, bool skip_end = false);
    RC<AstIfStatement> ParseIfStatement();
    RC<AstWhileLoop> ParseWhileLoop();
    RC<AstStatement> ParseForLoop();
    RC<AstStatement> ParseBreakStatement();
    RC<AstStatement> ParseContinueStatement();
    RC<AstTryCatch> ParseTryCatchStatement();
    RC<AstThrowExpression> ParseThrowExpression();
    RC<AstExpression> ParseBinaryExpression(
        Int expr_prec,
        RC<AstExpression> left
    );
    RC<AstExpression> ParseUnaryExpressionPrefix();
    RC<AstExpression> ParseUnaryExpressionPostfix(const RC<AstExpression> &expr);
    RC<AstExpression> ParseTernaryExpression(
        const RC<AstExpression> &conditional
    );
    RC<AstExpression> ParseExpression(
        Bool override_commas = false,
        Bool override_fat_arrows = false,
        Bool override_angle_brackets = false,
        Bool override_question_mark = false
    );
    RC<AstPrototypeSpecification> ParsePrototypeSpecification();
    RC<AstExpression> ParseAssignment();
    RC<AstVariableDeclaration> ParseVariableDeclaration(
        Bool allow_keyword_names = false,
        Bool allow_quoted_names = false,
        IdentifierFlagBits flags = 0
    );
    RC<AstStatement> ParseFunctionDefinition(Bool require_keyword = true);
    RC<AstFunctionExpression> ParseFunctionExpression(
        Bool require_keyword = true,
        Array<RC<AstParameter>> params = {}
    );
    RC<AstArrayExpression> ParseArrayExpression();
    RC<AstHashMap> ParseHashMap();
    RC<AstExpression> ParseValueOfExpression();
    RC<AstTypeOfExpression> ParseTypeOfExpression();
    Array<RC<AstParameter>> ParseFunctionParameters();
    Array<RC<AstParameter>> ParseGenericParameters();
    RC<AstStatement> ParseTypeDefinition();
    RC<AstTypeExpression> ParseTypeExpression(
        Bool require_keyword = true,
        Bool allow_identifier = true,
        Bool is_proxy_class = false,
        String type_name = "<Anonymous Type>"
    );
    RC<AstStatement> ParseEnumDefinition();
    RC<AstEnumExpression> ParseEnumExpression(
        Bool require_keyword = true,
        Bool allow_identifier = true,
        String enum_name = "<Anonymous Enum>"
    );
    RC<AstImport> ParseImport();
    RC<AstExportStatement> ParseExportStatement();
    RC<AstFileImport> ParseFileImport();
    RC<AstModuleImport> ParseModuleImport();
    RC<AstModuleImportPart> ParseModuleImportPart(Bool allow_braces = false);
    RC<AstReturnStatement> ParseReturnStatement();
    RC<AstExpression> ParseMetaProperty();

private:
    int m_template_argument_depth = 0; // until a better way is found..

    AstIterator *m_ast_iterator;
    TokenStream *m_token_stream;
    CompilationUnit *m_compilation_unit;

    Token Match(TokenClass token_class, Bool read = false);
    Token MatchAhead(TokenClass token_class, int n);
    Token MatchKeyword(Keywords keyword, Bool read = false);
    Token MatchKeywordAhead(Keywords keyword, int n);
    Token MatchOperator(const String &op, Bool read = false);
    Token MatchOperatorAhead(const String &op, int n);
    Token Expect(TokenClass token_class, Bool read = false);
    Token ExpectKeyword(Keywords keyword, Bool read = false);
    Token ExpectOperator(const String &op, Bool read = false);
    Token MatchIdentifier(Bool allow_keyword = false, Bool read = false);
    Token ExpectIdentifier(Bool allow_keyword = false, Bool read = false);
    Bool ExpectEndOfStmt();
    SourceLocation CurrentLocation() const;
    void SkipStatementTerminators();
    Int OperatorPrecedence(const Operator *&out);
};

} // namespace hyperion::compiler

#endif
