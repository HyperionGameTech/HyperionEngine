#pragma once

#include <Lang/compiler/TokenStream.hpp>
#include <Lang/SourceLocation.hpp>
#include <Lang/compiler/CompilationUnit.hpp>
#include <Lang/compiler/AstIterator.hpp>
#include <Lang/compiler/Keywords.hpp>
#include <Lang/compiler/Identifier.hpp>
#include <Lang/compiler/ast/AstModuleDeclaration.hpp>
#include <Lang/compiler/ast/AstDirective.hpp>
#include <Lang/compiler/ast/AstVariableDeclaration.hpp>
#include <Lang/compiler/ast/AstFunctionExpression.hpp>
#include <Lang/compiler/ast/AstArrayExpression.hpp>
#include <Lang/compiler/ast/AstHashMap.hpp>
#include <Lang/compiler/ast/AstClass.hpp>
#include <Lang/compiler/ast/AstTypeAlias.hpp>
#include <Lang/compiler/ast/AstStatement.hpp>
#include <Lang/compiler/ast/AstExpression.hpp>
#include <Lang/compiler/ast/AstImport.hpp>
#include <Lang/compiler/ast/AstExportStatement.hpp>
#include <Lang/compiler/ast/AstFileImport.hpp>
#include <Lang/compiler/ast/AstModuleImport.hpp>
#include <Lang/compiler/ast/AstInteger.hpp>
#include <Lang/compiler/ast/AstUnsignedInteger.hpp>
#include <Lang/compiler/ast/AstFloat.hpp>
#include <Lang/compiler/ast/AstString.hpp>
#include <Lang/compiler/ast/AstName.hpp>
#include <Lang/compiler/ast/AstBinaryExpression.hpp>
#include <Lang/compiler/ast/AstUnaryExpression.hpp>
#include <Lang/compiler/ast/AstTernaryExpression.hpp>
#include <Lang/compiler/ast/AstCallExpression.hpp>
#include <Lang/compiler/ast/AstArgument.hpp>
#include <Lang/compiler/ast/AstArgumentList.hpp>
#include <Lang/compiler/ast/AstVariable.hpp>
#include <Lang/compiler/ast/AstModuleAccess.hpp>
#include <Lang/compiler/ast/AstMember.hpp>
#include <Lang/compiler/ast/AstBreakStatement.hpp>
#include <Lang/compiler/ast/AstContinueStatement.hpp>
#include <Lang/compiler/ast/AstMemberCallExpression.hpp>
#include <Lang/compiler/ast/AstArrayAccess.hpp>
#include <Lang/compiler/ast/AstHasExpression.hpp>
#include <Lang/compiler/ast/AstIsExpression.hpp>
#include <Lang/compiler/ast/AstAsExpression.hpp>
#include <Lang/compiler/ast/AstNewExpression.hpp>
#include <Lang/compiler/ast/AstTrue.hpp>
#include <Lang/compiler/ast/AstFalse.hpp>
#include <Lang/compiler/ast/AstNil.hpp>
#include <Lang/compiler/ast/AstBlock.hpp>
#include <Lang/compiler/ast/AstIfStatement.hpp>
#include <Lang/compiler/ast/AstWhileLoop.hpp>
#include <Lang/compiler/ast/AstForLoop.hpp>
#include <Lang/compiler/ast/AstTryCatch.hpp>
#include <Lang/compiler/ast/AstThrowExpression.hpp>
#include <Lang/compiler/ast/AstTypeSpecifier.hpp>
#include <Lang/compiler/ast/AstTypeOfExpression.hpp>
#include <Lang/compiler/ast/AstReturnStatement.hpp>
#include <Lang/compiler/ast/AstTemplateInstantiation.hpp>

#include <string>

namespace Hyperion {

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
    RC<AstName> ParseNameLiteral();
    RC<AstIdentifier> ParseIdentifier(bool allowKeyword = false);
    RC<AstArgument> ParseArgument(RC<AstExpression> expr);
    RC<AstArgumentList> ParseArguments(bool requireParentheses = true);
    RC<AstCallExpression> ParseCallExpression(
        RC<AstExpression> target,
        bool requireParentheses = true);
    RC<AstModuleAccess> ParseModuleAccess();
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
    RC<AstBlock> ParseBlock(bool requireBraces, bool skipEnd = false, bool endOnCatch = false);
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
        EnumFlags<IdentifierFlags> flags = IdentifierFlags::NONE);
    RC<AstStatement> ParseFunctionDefinition(bool requireKeyword = true);
    RC<AstFunctionExpression> ParseFunctionExpression(
        bool requireKeyword = true,
        bool parseBody = true,
        Array<RC<AstParameter>> params = {});
    RC<AstArrayExpression> ParseArrayExpression();
    RC<AstHashMap> ParseHashMap();
    RC<AstTypeOfExpression> ParseTypeOfExpression();
    Array<RC<AstParameter>> ParseFunctionParameters();
    RC<AstClass> ParseClassDefinition();
    RC<AstClass> ParseClass(
        bool requireKeyword = true,
        bool allowIdentifier = true,
        EnumFlags<AstClassFlags> classFlags = CLASS_FLAG_NONE,
        String typeName = "<Anonymous Type>");
    RC<AstStatement> ParseEnumDefinition();
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

} // namespace Hyperion
