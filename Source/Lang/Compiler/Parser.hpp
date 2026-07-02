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
#include <Lang/Compiler/Ast/AstSwitchExpression.hpp>
#include <Lang/Compiler/Ast/AstWhileLoop.hpp>
#include <Lang/Compiler/Ast/AstForLoop.hpp>
#include <Lang/Compiler/Ast/AstForEachLoop.hpp>
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

    SharedPtr<AstStatement> ParseStatement(
        bool topLevel = false,
        bool readTerminators = true);
    SharedPtr<AstModuleDeclaration> ParseModuleDeclaration();
    SharedPtr<AstDirective> ParseDirective();
    SharedPtr<AstExpression> ParseTerm(
        bool overrideCommas = false,
        bool overrideFatArrows = false,
        bool overrideAngleBrackets = false,
        bool overrideSquareBrackets = false,
        bool overrideParentheses = false,
        bool overrideQuestionMark = false);
    SharedPtr<AstExpression> ParseParentheses();
    SharedPtr<AstTemplateInstantiation> ParseTemplateInstantiation(SharedPtr<AstExpression> expr);
    SharedPtr<AstConstant> ParseIntegerLiteral();
    SharedPtr<AstFloat> ParseFloatLiteral();
    SharedPtr<AstString> ParseStringLiteral();
    SharedPtr<AstName> ParseNameLiteral();
    SharedPtr<AstIdentifier> ParseIdentifier(bool allowKeyword = false);
    SharedPtr<AstArgument> ParseArgument(SharedPtr<AstExpression> expr);
    SharedPtr<AstArgumentList> ParseArguments(bool requireParentheses = true);
    SharedPtr<AstCallExpression> ParseCallExpression(
        SharedPtr<AstExpression> target,
        bool requireParentheses = true);
    SharedPtr<AstModuleAccess> ParseModuleAccess();
    SharedPtr<AstExpression> ParseMemberExpression(SharedPtr<AstExpression> target);
    SharedPtr<AstArrayAccess> ParseArrayAccess(
        SharedPtr<AstExpression> target,
        bool overrideCommas = false,
        bool overrideFatArrows = false,
        bool overrideAngleBrackets = false,
        bool overrideSquareBrackets = false,
        bool overrideParentheses = false,
        bool overrideQuestionMark = false);
    SharedPtr<AstHasExpression> ParseHasExpression(SharedPtr<AstExpression> target);
    SharedPtr<AstIsExpression> ParseIsExpression(SharedPtr<AstExpression> target);
    SharedPtr<AstAsExpression> ParseAsExpression(SharedPtr<AstExpression> target);
    SharedPtr<AstNewExpression> ParseNewExpression();
    SharedPtr<AstTrue> ParseTrue();
    SharedPtr<AstFalse> ParseFalse();
    SharedPtr<AstNil> ParseNil();
    SharedPtr<AstBlock> ParseBlock(bool requireBraces, bool skipEnd = false, bool endOnCatch = false);
    SharedPtr<AstIfStatement> ParseIfStatement();
    SharedPtr<AstSwitchExpression> ParseSwitchExpression();
    SharedPtr<AstWhileLoop> ParseWhileLoop();
    SharedPtr<AstStatement> ParseForLoop();
    SharedPtr<AstStatement> ParseForEachLoop(const Token& forToken, const SharedPtr<AstStatement>& declPart);
    SharedPtr<AstStatement> ParseBreakStatement();
    SharedPtr<AstStatement> ParseContinueStatement();
    SharedPtr<AstTryCatch> ParseTryCatchStatement();
    SharedPtr<AstThrowExpression> ParseThrowExpression();
    SharedPtr<AstExpression> ParseBinaryExpression(
        int exprPrec,
        SharedPtr<AstExpression> left);
    SharedPtr<AstExpression> ParseUnaryExpressionPrefix();
    SharedPtr<AstExpression> ParseUnaryExpressionPostfix(const SharedPtr<AstExpression>& expr);
    SharedPtr<AstExpression> ParseTernaryExpression(
        const SharedPtr<AstExpression>& conditional);
    SharedPtr<AstExpression> ParseExpression(
        bool overrideCommas = false,
        bool overrideFatArrows = false,
        bool overrideAngleBrackets = false,
        bool overrideQuestionMark = false);
    SharedPtr<AstTypeSpecifier> ParseTypeSpecifier();
    SharedPtr<AstExpression> ParseAssignment();
    SharedPtr<AstVariableDeclaration> ParseVariableDeclaration(
        bool allowKeywordNames = false,
        bool allowQuotedNames = false,
        EnumFlags<IdentifierFlags> flags = IdentifierFlags::NONE);
    SharedPtr<AstStatement> ParseFunctionDefinition(bool requireKeyword = true);
    SharedPtr<AstFunctionExpression> ParseFunctionExpression(
        bool requireKeyword = true,
        bool parseBody = true,
        Array<SharedPtr<AstParameter>> params = {});
    SharedPtr<AstArrayExpression> ParseArrayExpression();
    SharedPtr<AstHashMap> ParseHashMap();
    SharedPtr<AstTypeOfExpression> ParseTypeOfExpression();
    Array<SharedPtr<AstParameter>> ParseFunctionParameters();
    SharedPtr<AstClass> ParseClassDefinition();
    SharedPtr<AstClass> ParseClass(
        bool requireKeyword = true,
        bool allowIdentifier = true,
        EnumFlags<AstClassFlags> classFlags = CLASS_FLAG_NONE,
        String typeName = "<Anonymous Type>");
    SharedPtr<AstStatement> ParseEnumDefinition();
    SharedPtr<AstImport> ParseImport();
    SharedPtr<AstExportStatement> ParseExportStatement();
    SharedPtr<AstFileImport> ParseFileImport();
    SharedPtr<AstModuleImport> ParseModuleImport();
    SharedPtr<AstModuleImportPart> ParseModuleImportPart(bool allowBraces = false);
    SharedPtr<AstReturnStatement> ParseReturnStatement();

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
