#pragma once

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
#include <script/compiler/ast/AstClass.hpp>
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
#include <script/compiler/ast/AstTypeSpecifier.hpp>
#include <script/compiler/ast/AstTypeOfExpression.hpp>
#include <script/compiler/ast/AstReturnStatement.hpp>
#include <script/compiler/ast/AstTemplateInstantiation.hpp>

#include <string>

namespace hyperion::compiler {

class Parser
{
public:
    Parser(
        AstIterator* astIterator,
        TokenStream* tokenStream,
        CompilationUnit* compilationUnit);

    Parser(const Parser& other);

    void Parse(bool expectModuleDecl = true);

    RC<AstStatement> ParseStatement(
        bool topLevel = false,
        bool readTerminators = true);
    RC<AstModuleDeclaration> ParseModuleDeclaration();
    RC<AstDirective> ParseDirective();
    RC<AstExpression> ParseTerm(
        bool overrideCommas = false,
        bool overrideFatArrows = false,
        bool overrideAngleBrackets = false,
        bool overrideSquareBrackets = false,
        bool overrideParentheses = false,
        bool overrideQuestionMark = false);
    RC<AstExpression> ParseParentheses();
    RC<AstTemplateInstantiation> ParseTemplateInstantiation(RC<AstExpression> expr);
    RC<AstConstant> ParseIntegerLiteral();
    RC<AstFloat> ParseFloatLiteral();
    RC<AstString> ParseStringLiteral();
    RC<AstIdentifier> ParseIdentifier(bool allowKeyword = false);
    RC<AstArgument> ParseArgument(RC<AstExpression> expr);
    RC<AstArgumentList> ParseArguments(bool requireParentheses = true);
    RC<AstCallExpression> ParseCallExpression(
        RC<AstExpression> target,
        bool requireParentheses = true);
    RC<AstModuleAccess> ParseModuleAccess();
    RC<AstModuleProperty> ParseModuleProperty();
    RC<AstExpression> ParseMemberExpression(RC<AstExpression> target);
    RC<AstArrayAccess> ParseArrayAccess(
        RC<AstExpression> target,
        bool overrideCommas = false,
        bool overrideFatArrows = false,
        bool overrideAngleBrackets = false,
        bool overrideSquareBrackets = false,
        bool overrideParentheses = false,
        bool overrideQuestionMark = false);
    RC<AstHasExpression> ParseHasExpression(RC<AstExpression> target);
    RC<AstIsExpression> ParseIsExpression(RC<AstExpression> target);
    RC<AstAsExpression> ParseAsExpression(RC<AstExpression> target);
    RC<AstNewExpression> ParseNewExpression();
    RC<AstTrue> ParseTrue();
    RC<AstFalse> ParseFalse();
    RC<AstNil> ParseNil();
    RC<AstBlock> ParseBlock(bool requireBraces, bool skipEnd = false);
    RC<AstIfStatement> ParseIfStatement();
    RC<AstWhileLoop> ParseWhileLoop();
    RC<AstStatement> ParseForLoop();
    RC<AstStatement> ParseBreakStatement();
    RC<AstStatement> ParseContinueStatement();
    RC<AstTryCatch> ParseTryCatchStatement();
    RC<AstThrowExpression> ParseThrowExpression();
    RC<AstExpression> ParseBinaryExpression(
        int exprPrec,
        RC<AstExpression> left);
    RC<AstExpression> ParseUnaryExpressionPrefix();
    RC<AstExpression> ParseUnaryExpressionPostfix(const RC<AstExpression>& expr);
    RC<AstExpression> ParseTernaryExpression(
        const RC<AstExpression>& conditional);
    RC<AstExpression> ParseExpression(
        bool overrideCommas = false,
        bool overrideFatArrows = false,
        bool overrideAngleBrackets = false,
        bool overrideQuestionMark = false);
    RC<AstTypeSpecifier> ParseTypeSpecifier();
    RC<AstExpression> ParseAssignment();
    RC<AstVariableDeclaration> ParseVariableDeclaration(
        bool allowKeywordNames = false,
        bool allowQuotedNames = false,
        IdentifierFlagBits flags = 0);
    RC<AstStatement> ParseFunctionDefinition(bool requireKeyword = true);
    RC<AstFunctionExpression> ParseFunctionExpression(
        bool requireKeyword = true,
        Array<RC<AstParameter>> params = {});
    RC<AstArrayExpression> ParseArrayExpression();
    RC<AstHashMap> ParseHashMap();
    RC<AstTypeOfExpression> ParseTypeOfExpression();
    Array<RC<AstParameter>> ParseFunctionParameters();
    RC<AstClass> ParseClassDefinition();
    RC<AstClass> ParseClass(
        bool requireKeyword = true,
        bool allowIdentifier = true,
        bool isProxyClass = false,
        String typeName = "<Anonymous Type>");
    RC<AstStatement> ParseEnumDefinition();
    RC<AstEnumExpression> ParseEnumExpression(
        bool requireKeyword = true,
        bool allowIdentifier = true,
        String enumName = "<Anonymous Enum>");
    RC<AstImport> ParseImport();
    RC<AstExportStatement> ParseExportStatement();
    RC<AstFileImport> ParseFileImport();
    RC<AstModuleImport> ParseModuleImport();
    RC<AstModuleImportPart> ParseModuleImportPart(bool allowBraces = false);
    RC<AstReturnStatement> ParseReturnStatement();

private:
    int m_templateArgumentDepth = 0; // until a better way is found..

    AstIterator* m_astIterator;
    TokenStream* m_tokenStream;
    CompilationUnit* m_compilationUnit;

    Token Match(TokenClass tokenClass, bool read = false);
    Token MatchAhead(TokenClass tokenClass, int n);
    Token MatchKeyword(Keywords keyword, bool read = false);
    Token MatchKeywordAhead(Keywords keyword, int n);
    Token MatchOperator(const String& op, bool read = false);
    Token MatchOperatorAhead(const String& op, int n);
    Token Expect(TokenClass tokenClass, bool read = false);
    Token ExpectKeyword(Keywords keyword, bool read = false);
    Token ExpectOperator(const String& op, bool read = false);
    Token MatchIdentifier(bool allowKeyword = false, bool read = false);
    Token ExpectIdentifier(bool allowKeyword = false, bool read = false);
    bool ExpectEndOfStmt();
    SourceLocation CurrentLocation() const;
    void SkipStatementTerminators();
    int OperatorPrecedence(const Operator*& out);
};

} // namespace hyperion::compiler
