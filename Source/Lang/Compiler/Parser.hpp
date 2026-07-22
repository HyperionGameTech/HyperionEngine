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

    Handle<AstStatement> ParseStatement(
        bool topLevel = false,
        bool readTerminators = true);
    Handle<AstModuleDeclaration> ParseModuleDeclaration();
    Handle<AstDirective> ParseDirective();
    Handle<AstExpression> ParseTerm(
        bool overrideCommas = false,
        bool overrideFatArrows = false,
        bool overrideAngleBrackets = false,
        bool overrideSquareBrackets = false,
        bool overrideParentheses = false,
        bool overrideQuestionMark = false);
    Handle<AstExpression> ParseParentheses();
    Handle<AstTemplateInstantiation> ParseTemplateInstantiation(Handle<AstExpression> expr);
    Handle<AstConstant> ParseIntegerLiteral();
    Handle<AstFloat> ParseFloatLiteral();
    Handle<AstString> ParseStringLiteral();
    Handle<AstName> ParseNameLiteral();
    Handle<AstIdentifier> ParseIdentifier(bool allowKeyword = false);
    Handle<AstArgument> ParseArgument(Handle<AstExpression> expr);
    Handle<AstArgumentList> ParseArguments(bool requireParentheses = true);
    Handle<AstCallExpression> ParseCallExpression(
        Handle<AstExpression> target,
        bool requireParentheses = true);
    Handle<AstModuleAccess> ParseModuleAccess();
    Handle<AstExpression> ParseMemberExpression(Handle<AstExpression> target);
    Handle<AstArrayAccess> ParseArrayAccess(
        Handle<AstExpression> target,
        bool overrideCommas = false,
        bool overrideFatArrows = false,
        bool overrideAngleBrackets = false,
        bool overrideSquareBrackets = false,
        bool overrideParentheses = false,
        bool overrideQuestionMark = false);
    Handle<AstHasExpression> ParseHasExpression(Handle<AstExpression> target);
    Handle<AstIsExpression> ParseIsExpression(Handle<AstExpression> target);
    Handle<AstAsExpression> ParseAsExpression(Handle<AstExpression> target);
    Handle<AstNewExpression> ParseNewExpression();
    Handle<AstTrue> ParseTrue();
    Handle<AstFalse> ParseFalse();
    Handle<AstNil> ParseNil();
    Handle<AstBlock> ParseBlock(bool requireBraces, bool skipEnd = false, bool endOnCatch = false);
    Handle<AstIfStatement> ParseIfStatement();
    Handle<AstSwitchExpression> ParseSwitchExpression();
    Handle<AstWhileLoop> ParseWhileLoop();
    Handle<AstStatement> ParseForLoop();
    Handle<AstStatement> ParseForEachLoop(const Token& forToken, const Handle<AstStatement>& declPart);
    Handle<AstStatement> ParseBreakStatement();
    Handle<AstStatement> ParseContinueStatement();
    Handle<AstTryCatch> ParseTryCatchStatement();
    Handle<AstThrowExpression> ParseThrowExpression();
    Handle<AstExpression> ParseBinaryExpression(
        int exprPrec,
        Handle<AstExpression> left);
    Handle<AstExpression> ParseUnaryExpressionPrefix();
    Handle<AstExpression> ParseUnaryExpressionPostfix(const Handle<AstExpression>& expr);
    Handle<AstExpression> ParseTernaryExpression(
        const Handle<AstExpression>& conditional);
    Handle<AstExpression> ParseExpression(
        bool overrideCommas = false,
        bool overrideFatArrows = false,
        bool overrideAngleBrackets = false,
        bool overrideQuestionMark = false);
    Handle<AstTypeSpecifier> ParseTypeSpecifier();
    Handle<AstExpression> ParseAssignment();
    Handle<AstVariableDeclaration> ParseVariableDeclaration(
        bool allowKeywordNames = false,
        bool allowQuotedNames = false,
        EnumFlags<IdentifierFlags> flags = IdentifierFlags::NONE);
    Handle<AstStatement> ParseFunctionDefinition(bool requireKeyword = true);
    Handle<AstFunctionExpression> ParseFunctionExpression(
        bool requireKeyword = true,
        bool parseBody = true,
        Array<Handle<AstParameter>> params = {});
    Handle<AstArrayExpression> ParseArrayExpression();
    Handle<AstHashMap> ParseHashMap();
    Handle<AstTypeOfExpression> ParseTypeOfExpression();
    Array<Handle<AstParameter>> ParseFunctionParameters();
    Handle<AstClass> ParseClassDefinition();
    Handle<AstClass> ParseClass(
        bool requireKeyword = true,
        bool allowIdentifier = true,
        EnumFlags<AstClassFlags> classFlags = CLASS_FLAG_NONE,
        String typeName = "<Anonymous Type>");
    Handle<AstStatement> ParseEnumDefinition();
    Handle<AstImport> ParseImport();
    Handle<AstExportStatement> ParseExportStatement();
    Handle<AstFileImport> ParseFileImport();
    Handle<AstModuleImport> ParseModuleImport();
    Handle<AstModuleImportPart> ParseModuleImportPart(bool allowBraces = false);
    Handle<AstReturnStatement> ParseReturnStatement();

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
