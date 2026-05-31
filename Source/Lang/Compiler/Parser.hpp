#pragma once

#include <Lang/Compiler/TokenStream.hpp>
#include <Lang/SourceLocation.hpp>
#include <Lang/Compiler/CompilationUnit.hpp>
#include <Lang/Compiler/AstIterator.hpp>
#include <Lang/Compiler/Keywords.hpp>
#include <Lang/Compiler/Identifier.hpp>
#include <Lang/Compiler/Ast/AstModuleDeclaration.hpp>
#include <Lang/Compiler/Ast/AstDirective.hpp>
#include <Lang/Compiler/Ast/AstVariableDeclaration.hpp>
#include <Lang/Compiler/Ast/AstFunctionExpression.hpp>
#include <Lang/Compiler/Ast/AstArrayExpression.hpp>
#include <Lang/Compiler/Ast/AstHashMap.hpp>
#include <Lang/Compiler/Ast/AstClass.hpp>
#include <Lang/Compiler/Ast/AstTypeAlias.hpp>
#include <Lang/Compiler/Ast/AstStatement.hpp>
#include <Lang/Compiler/Ast/AstExpression.hpp>
#include <Lang/Compiler/Ast/AstImport.hpp>
#include <Lang/Compiler/Ast/AstExportStatement.hpp>
#include <Lang/Compiler/Ast/AstFileImport.hpp>
#include <Lang/Compiler/Ast/AstModuleImport.hpp>
#include <Lang/Compiler/Ast/AstInteger.hpp>
#include <Lang/Compiler/Ast/AstUnsignedInteger.hpp>
#include <Lang/Compiler/Ast/AstFloat.hpp>
#include <Lang/Compiler/Ast/AstString.hpp>
#include <Lang/Compiler/Ast/AstName.hpp>
#include <Lang/Compiler/Ast/AstBinaryExpression.hpp>
#include <Lang/Compiler/Ast/AstUnaryExpression.hpp>
#include <Lang/Compiler/Ast/AstTernaryExpression.hpp>
#include <Lang/Compiler/Ast/AstCallExpression.hpp>
#include <Lang/Compiler/Ast/AstArgument.hpp>
#include <Lang/Compiler/Ast/AstArgumentList.hpp>
#include <Lang/Compiler/Ast/AstVariable.hpp>
#include <Lang/Compiler/Ast/AstModuleAccess.hpp>
#include <Lang/Compiler/Ast/AstMember.hpp>
#include <Lang/Compiler/Ast/AstBreakStatement.hpp>
#include <Lang/Compiler/Ast/AstContinueStatement.hpp>
#include <Lang/Compiler/Ast/AstMemberCallExpression.hpp>
#include <Lang/Compiler/Ast/AstArrayAccess.hpp>
#include <Lang/Compiler/Ast/AstHasExpression.hpp>
#include <Lang/Compiler/Ast/AstIsExpression.hpp>
#include <Lang/Compiler/Ast/AstAsExpression.hpp>
#include <Lang/Compiler/Ast/AstNewExpression.hpp>
#include <Lang/Compiler/Ast/AstTrue.hpp>
#include <Lang/Compiler/Ast/AstFalse.hpp>
#include <Lang/Compiler/Ast/AstNil.hpp>
#include <Lang/Compiler/Ast/AstBlock.hpp>
#include <Lang/Compiler/Ast/AstIfStatement.hpp>
#include <Lang/Compiler/Ast/AstWhileLoop.hpp>
#include <Lang/Compiler/Ast/AstForLoop.hpp>
#include <Lang/Compiler/Ast/AstTryCatch.hpp>
#include <Lang/Compiler/Ast/AstThrowExpression.hpp>
#include <Lang/Compiler/Ast/AstTypeSpecifier.hpp>
#include <Lang/Compiler/Ast/AstTypeOfExpression.hpp>
#include <Lang/Compiler/Ast/AstReturnStatement.hpp>
#include <Lang/Compiler/Ast/AstTemplateInstantiation.hpp>

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
