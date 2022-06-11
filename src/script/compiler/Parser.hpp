#ifndef PARSER_HPP
#define PARSER_HPP

#include <script/compiler/TokenStream.hpp>
#include <script/SourceLocation.hpp>
#include <script/compiler/CompilationUnit.hpp>
#include <script/compiler/AstIterator.hpp>
#include <script/compiler/Keywords.hpp>
#include <script/compiler/ast/AstModuleDeclaration.hpp>
#include <script/compiler/ast/AstDirective.hpp>
#include <script/compiler/ast/AstVariableDeclaration.hpp>
#include <script/compiler/ast/AstFunctionDefinition.hpp>
#include <script/compiler/ast/AstFunctionExpression.hpp>
#include <script/compiler/ast/AstArrayExpression.hpp>
#include <script/compiler/ast/AstTupleExpression.hpp>
#include <script/compiler/ast/AstTypeExpression.hpp>
#include <script/compiler/ast/AstEnumExpression.hpp>
#include <script/compiler/ast/AstPrototypeDefinition.hpp>
#include <script/compiler/ast/AstTypeAlias.hpp>
#include <script/compiler/ast/AstAliasDeclaration.hpp>
#include <script/compiler/ast/AstMixinDeclaration.hpp>
#include <script/compiler/ast/AstMixin.hpp>
#include <script/compiler/ast/AstStatement.hpp>
#include <script/compiler/ast/AstExpression.hpp>
#include <script/compiler/ast/AstImport.hpp>
#include <script/compiler/ast/AstFileImport.hpp>
#include <script/compiler/ast/AstModuleImport.hpp>
#include <script/compiler/ast/AstInteger.hpp>
#include <script/compiler/ast/AstFloat.hpp>
#include <script/compiler/ast/AstString.hpp>
#include <script/compiler/ast/AstBinaryExpression.hpp>
#include <script/compiler/ast/AstUnaryExpression.hpp>
#include <script/compiler/ast/AstCallExpression.hpp>
#include <script/compiler/ast/AstArgument.hpp>
#include <script/compiler/ast/AstArgumentList.hpp>
#include <script/compiler/ast/AstVariable.hpp>
#include <script/compiler/ast/AstModuleAccess.hpp>
#include <script/compiler/ast/AstModuleProperty.hpp>
#include <script/compiler/ast/AstMember.hpp>
#include <script/compiler/ast/AstArrayAccess.hpp>
#include <script/compiler/ast/AstHasExpression.hpp>
#include <script/compiler/ast/AstNewExpression.hpp>
#include <script/compiler/ast/AstTrue.hpp>
#include <script/compiler/ast/AstFalse.hpp>
#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/ast/AstBlock.hpp>
#include <script/compiler/ast/AstBlockExpression.hpp>
#include <script/compiler/ast/AstIfStatement.hpp>
#include <script/compiler/ast/AstWhileLoop.hpp>
#include <script/compiler/ast/AstForLoop.hpp>
#include <script/compiler/ast/AstPrintStatement.hpp>
#include <script/compiler/ast/AstTryCatch.hpp>
#include <script/compiler/ast/AstPrototypeSpecification.hpp>
#include <script/compiler/ast/AstTypeOfExpression.hpp>
#include <script/compiler/ast/AstEvent.hpp>
#include <script/compiler/ast/AstActionExpression.hpp>
#include <script/compiler/ast/AstReturnStatement.hpp>
#include <script/compiler/ast/AstYieldStatement.hpp>
#include <script/compiler/ast/AstMetaBlock.hpp>
#include <script/compiler/ast/AstSymbolQuery.hpp>
#include <script/compiler/ast/AstTemplateExpression.hpp>
#include <script/compiler/ast/AstTemplateInstantiation.hpp>
#include <script/compiler/ast/AstSyntaxDefinition.hpp>

#include <string>

namespace hyperion::compiler {

class Parser {
public:
    Parser(AstIterator *ast_iterator,
        TokenStream *token_stream,
        CompilationUnit *compilation_unit);
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

    std::shared_ptr<AstStatement> ParseStatement(bool top_level = false);
    std::shared_ptr<AstModuleDeclaration> ParseModuleDeclaration();
    std::shared_ptr<AstDirective> ParseDirective();
    std::shared_ptr<AstExpression> ParseTerm(
        bool override_commas = false,
        bool override_fat_arrows = false,
        bool override_angle_brackets = false,
        bool override_square_brackets = false
    );
    std::shared_ptr<AstExpression> ParseParentheses();
    std::shared_ptr<AstExpression> ParseAngleBrackets(std::shared_ptr<AstExpression> target);
    std::shared_ptr<AstInteger> ParseIntegerLiteral();
    std::shared_ptr<AstFloat> ParseFloatLiteral();
    std::shared_ptr<AstString> ParseStringLiteral();
    std::shared_ptr<AstIdentifier> ParseIdentifier(bool allow_keyword = false);
    std::shared_ptr<AstArgument> ParseArgument(std::shared_ptr<AstExpression> expr);
    std::shared_ptr<AstArgumentList> ParseArguments(bool require_parentheses = true,
        const std::shared_ptr<AstExpression> &expr = nullptr);
    std::shared_ptr<AstCallExpression> ParseCallExpression(std::shared_ptr<AstExpression> target,
        bool require_parentheses = true);
    std::shared_ptr<AstModuleAccess> ParseModuleAccess();
    std::shared_ptr<AstModuleProperty> ParseModuleProperty();
    std::shared_ptr<AstMember> ParseMemberExpression(std::shared_ptr<AstExpression> target);
    std::shared_ptr<AstArrayAccess> ParseArrayAccess(std::shared_ptr<AstExpression> target);
    std::shared_ptr<AstHasExpression> ParseHasExpression(std::shared_ptr<AstExpression> target);
    std::shared_ptr<AstActionExpression> ParseActionExpression(std::shared_ptr<AstExpression> expr);
    std::shared_ptr<AstTemplateInstantiation> ParseTemplateInstantiation(std::shared_ptr<AstIdentifier> expr);
    std::shared_ptr<AstNewExpression> ParseNewExpression();
    std::shared_ptr<AstTrue> ParseTrue();
    std::shared_ptr<AstFalse> ParseFalse();
    std::shared_ptr<AstNil> ParseNil();
    std::shared_ptr<AstBlock> ParseBlock();
    std::shared_ptr<AstBlockExpression> ParseBlockExpression();
    std::shared_ptr<AstIfStatement> ParseIfStatement();
    std::shared_ptr<AstWhileLoop> ParseWhileLoop();
    std::shared_ptr<AstForLoop> ParseForLoop();
    std::shared_ptr<AstPrintStatement> ParsePrintStatement();
    std::shared_ptr<AstTryCatch> ParseTryCatchStatement();
    std::shared_ptr<AstExpression> ParseBinaryExpression(int expr_prec,
        std::shared_ptr<AstExpression> left);
    std::shared_ptr<AstExpression> ParseUnaryExpression();
    std::shared_ptr<AstExpression> ParseExpression(
        bool override_commas = false,
        bool override_fat_arrows = false,
        bool override_angle_brackets = false
    );
    std::shared_ptr<AstPrototypeSpecification> ParsePrototypeSpecification();
    std::shared_ptr<AstExpression> ParseAssignment();
    std::shared_ptr<AstVariableDeclaration> ParseVariableDeclaration(bool allow_keyword_names = false,
        bool allow_quoted_names = false);
    std::shared_ptr<AstFunctionDefinition> ParseFunctionDefinition(bool require_keyword = true);
    std::shared_ptr<AstFunctionExpression> ParseFunctionExpression(bool require_keyword = true,
        std::vector<std::shared_ptr<AstParameter>> params = {},
        bool is_async = false,
        bool is_pure = false);
    std::shared_ptr<AstArrayExpression> ParseArrayExpression();
    std::shared_ptr<AstTupleExpression> ParseTupleExpression(
        const std::shared_ptr<AstExpression> &expr = nullptr);
    std::shared_ptr<AstExpression> ParseAsyncExpression();
    std::shared_ptr<AstExpression> ParsePureExpression();
    std::shared_ptr<AstExpression> ParseImpureExpression();
    std::shared_ptr<AstExpression> ParseValueOfExpression();
    std::shared_ptr<AstTypeOfExpression> ParseTypeOfExpression();
    std::vector<std::shared_ptr<AstParameter>> ParseFunctionParameters();
    std::shared_ptr<AstStatement> ParseTypeDefinition();
    std::shared_ptr<AstTypeExpression> ParseTypeExpression();
    std::shared_ptr<AstEnumExpression> ParseEnumExpression();
    std::shared_ptr<AstAliasDeclaration> ParseAliasDeclaration();
    std::shared_ptr<AstMixinDeclaration> ParseMixinDeclaration();
    std::shared_ptr<AstMixin> ParseMixinExpression(const std::string &name);
    std::shared_ptr<AstImport> ParseImport();
    std::shared_ptr<AstFileImport> ParseFileImport();
    std::shared_ptr<AstModuleImport> ParseModuleImport();
    std::shared_ptr<AstModuleImportPart> ParseModuleImportPart(bool allow_braces = false);
    std::shared_ptr<AstReturnStatement> ParseReturnStatement();
    std::shared_ptr<AstYieldStatement> ParseYieldStatement();
    std::shared_ptr<AstMetaBlock> ParseMetaBlock();
    std::shared_ptr<AstExpression> ParseMetaProperty();
    std::shared_ptr<AstSyntaxDefinition> ParseSyntaxDefinition();
};

} // namespace hyperion::compiler

#endif
