#include <Lang/Compiler/Parser.hpp>
#include <Lang/Compiler/Configuration.hpp>
#include <Lang/Compiler/Lexer.hpp>
#include <Lang/Compiler/Keywords.hpp>
#include <Lang/Compiler/TypeSystem/SymbolType.hpp>

#include <Lang/Compiler/TypeSystem/BuiltinTypes.hpp>

#include <Core/Unicode.hpp>
#include <Core/Utilities/StringUtil.hpp>

namespace Hyperion {

Parser::Parser(AstIterator* astIterator,
    TokenStream* tokenStream,
    CompilationUnit* compilationUnit)
    : m_astIterator(astIterator),
      m_tokenStream(tokenStream),
      m_compilationUnit(compilationUnit)
{
}

Parser::Parser(const Parser& other)
    : m_astIterator(other.m_astIterator),
      m_tokenStream(other.m_tokenStream),
      m_compilationUnit(other.m_compilationUnit)
{
}

Token Parser::Match(TokenClass tokenClass, bool read)
{
    Token peek = m_tokenStream->Peek();

    if (peek && peek.GetTokenClass() == tokenClass)
    {
        if (read && m_tokenStream->HasNext())
        {
            m_tokenStream->Next();
        }

        return peek;
    }

    return Token::EMPTY;
}

Token Parser::MatchAhead(TokenClass tokenClass, int n)
{
    Token peek = m_tokenStream->Peek(n);

    if (peek && peek.GetTokenClass() == tokenClass)
    {
        return peek;
    }

    return Token::EMPTY;
}

Token Parser::MatchKeyword(Keywords keyword, bool read)
{
    Token peek = m_tokenStream->Peek();

    if (peek && peek.GetTokenClass() == TK_KEYWORD)
    {
        auto str = Keyword::ToString(keyword);

        if (str && peek.GetValue() == str.Get())
        {
            if (read && m_tokenStream->HasNext())
            {
                m_tokenStream->Next();
            }

            return peek;
        }
    }

    return Token::EMPTY;
}

Token Parser::MatchKeywordAhead(Keywords keyword, int n)
{
    Token peek = m_tokenStream->Peek(n);

    if (peek && peek.GetTokenClass() == TK_KEYWORD)
    {
        auto str = Keyword::ToString(keyword);

        if (str && peek.GetValue() == str.Get())
        {
            return peek;
        }
    }

    return Token::EMPTY;
}

Token Parser::MatchOperator(const String& op, bool read)
{
    Token peek = m_tokenStream->Peek();

    if (peek && peek.GetTokenClass() == TK_OPERATOR)
    {
        if (peek.GetValue() == op)
        {
            if (read && m_tokenStream->HasNext())
            {
                m_tokenStream->Next();
            }

            return peek;
        }
    }

    return Token::EMPTY;
}

Token Parser::MatchOperatorAhead(const String& op, int n)
{
    Token peek = m_tokenStream->Peek(n);

    if (peek && peek.GetTokenClass() == TK_OPERATOR)
    {
        if (peek.GetValue() == op)
        {
            return peek;
        }
    }

    return Token::EMPTY;
}

Token Parser::Expect(TokenClass tokenClass, bool read)
{
    Token token = Match(tokenClass, read);

    if (!token)
    {
        const SourceLocation location = CurrentLocation();

        ErrorMessage errorMsg;
        String errorStr;

        switch (tokenClass)
        {
        case TK_IDENT:
            errorMsg = Msg_expected_identifier;
            break;
        default:
            errorMsg = Msg_expected_token;
            errorStr = Token::TokenTypeToString(tokenClass);
        }

        m_compilationUnit->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            errorMsg,
            location,
            errorStr));
    }

    return token;
}

Token Parser::ExpectKeyword(Keywords keyword, bool read)
{
    Token token = MatchKeyword(keyword, read);

    if (!token)
    {
        const SourceLocation location = CurrentLocation();

        if (read && m_tokenStream->HasNext())
        {
            m_tokenStream->Next();
        }

        ErrorMessage errorMsg;
        String errorStr;

        switch (keyword)
        {
        case Keyword_module:
            errorMsg = Msg_expected_module;
            break;
        default:
        {
            const String* keywordStr = Keyword::ToString(keyword).TryGet();

            errorMsg = Msg_expected_token;
            errorStr = keywordStr ? *keywordStr : "<unknown keyword>";
        }
        }

        m_compilationUnit->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            errorMsg,
            location,
            errorStr));
    }

    return token;
}

Token Parser::ExpectOperator(const String& op, bool read)
{
    Token token = MatchOperator(op, read);

    if (!token)
    {
        const SourceLocation location = CurrentLocation();

        if (read && m_tokenStream->HasNext())
        {
            m_tokenStream->Next();
        }

        m_compilationUnit->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_expected_token,
            location,
            op));
    }

    return token;
}

Token Parser::MatchIdentifier(bool allowKeyword, bool read)
{
    Token ident = Match(TK_IDENT, read);

    if (!ident)
    {
        Token kw = Match(TK_KEYWORD, read);

        if (kw)
        {
            if (allowKeyword)
            {
                return kw;
            }

            // keyword may not be used as an identifier here.
            m_compilationUnit->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_keyword_cannot_be_used_as_identifier,
                kw.GetLocation(),
                kw.GetValue()));
        }

        return Token::EMPTY;
    }

    return ident;
}

Token Parser::ExpectIdentifier(bool allowKeyword, bool read)
{
    Token kw = Match(TK_KEYWORD, read);

    if (!kw)
    {
        // keyword not found, so must be identifier
        return Expect(TK_IDENT, read);
    }

    // handle ident as keyword
    if (allowKeyword)
    {
        return kw;
    }

    m_compilationUnit->GetErrorList().AddError(CompilerError(
        LEVEL_ERROR,
        Msg_keyword_cannot_be_used_as_identifier,
        kw.GetLocation(),
        kw.GetValue()));

    return Token::EMPTY;
}

bool Parser::ExpectEndOfStmt()
{
    const SourceLocation location = CurrentLocation();

    if (!Match(TK_NEWLINE, true) && !Match(TK_SEMICOLON, true) && !Match(TK_CLOSE_BRACE, false))
    {
        m_compilationUnit->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_expected_end_of_statement,
            location));

        return false;
    }

    return true;
}

SourceLocation Parser::CurrentLocation() const
{
    if (m_tokenStream->GetSize() != 0 && !m_tokenStream->HasNext())
    {
        return m_tokenStream->Last().GetLocation();
    }

    return m_tokenStream->Peek().GetLocation();
}

void Parser::SkipStatementTerminators()
{
    // read past statement terminator tokens
    while (Match(TK_SEMICOLON, true) || Match(TK_NEWLINE, true))
        ;
}

void Parser::Parse(bool expectModuleDecl)
{
    SkipStatementTerminators();

    if (expectModuleDecl)
    {
        // create a module based upon the filename
        const String filepath = m_tokenStream->GetInfo().filepath;
        const Array<String> split = filepath.Split('\\', '/');

        String realFilename = split.Any()
            ? split.Back()
            : filepath;

        realFilename = StringUtil::StripExtension(realFilename);

        SharedPtr<AstModuleDeclaration> moduleAst(new AstModuleDeclaration(
            realFilename.Data(),
            SourceLocation(0, 0, filepath)));

        // build up the module declaration with statements
        while (m_tokenStream->HasNext())
        {
            // skip statement terminator tokens
            if (Match(TK_SEMICOLON, true) || Match(TK_NEWLINE, true))
            {
                continue;
            }

            // parse at top level, to allow for nested modules
            if (SharedPtr<AstStatement> stmt = ParseStatement(true))
            {
                moduleAst->AddChild(stmt);
            }
        }

        m_astIterator->Push(moduleAst);
    }
    else
    {
        // build up the module declaration with statements
        while (m_tokenStream->HasNext())
        {
            // skip statement terminator tokens
            if (Match(TK_SEMICOLON, true) || Match(TK_NEWLINE, true))
            {
                return;
            }

            // parse at top level, to allow for nested modules
            if (SharedPtr<AstStatement> stmt = ParseStatement(true))
            {
                m_astIterator->Push(stmt);
            }
        }
    }
}

int Parser::OperatorPrecedence(const Operator*& out)
{
    out = nullptr;

    Token token = m_tokenStream->Peek();

    if (token && token.GetTokenClass() == TK_OPERATOR)
    {
        if (!Operator::IsBinaryOperator(token.GetValue(), out))
        {
            // internal error: operator not defined
            m_compilationUnit->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_internal_error,
                token.GetLocation()));
        }
    }

    if (out != nullptr)
    {
        return out->GetPrecedence();
    }

    return -1;
}

SharedPtr<AstStatement> Parser::ParseStatement(
    bool topLevel,
    bool readTerminators)
{
    SharedPtr<AstStatement> res;

    if (Match(TK_KEYWORD, false))
    {
        if (MatchKeyword(Keyword_module, false) && !MatchAhead(TK_DOT, 1))
        {
            auto moduleDecl = ParseModuleDeclaration();

            if (topLevel)
            {
                res = moduleDecl;
            }
            else
            {
                // module may not be declared in a block
                m_compilationUnit->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_module_declared_in_block,
                    m_tokenStream->Next().GetLocation()));

                res = nullptr;
            }
        }
        else if (MatchKeyword(Keyword_import, false))
        {
            res = ParseImport();
        }
        else if (MatchKeyword(Keyword_export, false))
        {
            res = ParseExportStatement();
        }
        else if (MatchKeyword(Keyword_const, false)
            || MatchKeyword(Keyword_ref, false)
            || (MatchKeyword(Keyword_extern, false) && !MatchKeywordAhead(Keyword_func, 1) && !MatchKeywordAhead(Keyword_class, 1) && !MatchKeywordAhead(Keyword_struct, 1) && !MatchKeywordAhead(Keyword_enum, 1)))
        {
            res = ParseVariableDeclaration();
        }
        else if (MatchKeyword(Keyword_func, false) || (MatchKeyword(Keyword_extern, false) && MatchKeywordAhead(Keyword_func, 1)))
        {
            if (MatchAhead(TK_IDENT, 1) || (MatchKeyword(Keyword_extern, false) && MatchAhead(TK_IDENT, 2)))
            {
                res = ParseFunctionDefinition();
            }
            else
            {
                res = ParseFunctionExpression();
            }
        }
        else if (MatchKeyword(Keyword_class, false) || MatchKeyword(Keyword_struct, false)
            || (MatchKeyword(Keyword_proxy, false) && MatchKeywordAhead(Keyword_class, 1))
            || (MatchKeyword(Keyword_extern, false) && (MatchKeywordAhead(Keyword_class, 1) || MatchKeywordAhead(Keyword_struct, 1))))
        {
            res = ParseClassDefinition();
        }
        else if (MatchKeyword(Keyword_enum, false)
            || (MatchKeyword(Keyword_extern, false) && MatchKeywordAhead(Keyword_enum, 1)))
        {
            res = ParseEnumDefinition();
        }
        else if (MatchKeyword(Keyword_if, false))
        {
            res = ParseIfStatement();
        }
        else if (MatchKeyword(Keyword_while, false))
        {
            res = ParseWhileLoop();
        }
        else if (MatchKeyword(Keyword_for, false))
        {
            res = ParseForLoop();
        }
        else if (MatchKeyword(Keyword_break, false))
        {
            res = ParseBreakStatement();
        }
        else if (MatchKeyword(Keyword_continue, false))
        {
            res = ParseContinueStatement();
        }
        else if (MatchKeyword(Keyword_try, false))
        {
            res = ParseTryCatchStatement();
        }
        else if (MatchKeyword(Keyword_return, false))
        {
            res = ParseReturnStatement();
        }
        else
        {
            res = ParseExpression();
        }
    }
    else if (Match(TK_NAME_LITERAL, false))
    {
        res = ParseDirective();
    }
    else if (Match(TK_OPEN_BRACE, false))
    {
        res = ParseBlock(true);
    }
    else if (Match(TK_IDENT, false) && (MatchAhead(TK_COLON, 1) || MatchAhead(TK_DEFINE, 1)))
    {
        res = ParseVariableDeclaration();
    }
    else
    {
        res = ParseExpression(false);
    }

    if (readTerminators && res != nullptr && m_tokenStream->HasNext())
    {
        ExpectEndOfStmt();
    }

    return res;
}

SharedPtr<AstModuleDeclaration> Parser::ParseModuleDeclaration()
{
    if (Token moduleDecl = ExpectKeyword(Keyword_module, true))
    {
        if (Token moduleName = Expect(TK_IDENT, true))
        {
            // expect open brace
            if (Expect(TK_OPEN_BRACE, true))
            {
                SharedPtr<AstModuleDeclaration> moduleAst(new AstModuleDeclaration(
                    moduleName.GetValue(),
                    moduleDecl.GetLocation()));

                // build up the module declaration with statements
                while (m_tokenStream->HasNext() && !Match(TK_CLOSE_BRACE, false))
                {
                    // skip statement terminator tokens
                    if (!Match(TK_SEMICOLON, true) && !Match(TK_NEWLINE, true))
                    {

                        // parse at top level, to allow for nested modules
                        if (SharedPtr<AstStatement> stmt = ParseStatement(true))
                        {
                            moduleAst->AddChild(stmt);
                        }
                    }
                }

                // expect close brace
                if (Expect(TK_CLOSE_BRACE, true))
                {
                    return moduleAst;
                }
            }
        }
    }

    return nullptr;
}

SharedPtr<AstDirective> Parser::ParseDirective()
{
    if (Token token = Expect(TK_NAME_LITERAL, true))
    {
        // the arguments will be held in an array expression
        Array<String> args;

        while (m_tokenStream->HasNext() && !(Match(TK_SEMICOLON, true) || Match(TK_NEWLINE, true)))
        {
            Token token = m_tokenStream->Peek();

            args.PushBack(token.GetValue());
            m_tokenStream->Next();
        }

        return SharedPtr<AstDirective>(new AstDirective(
            token.GetValue(),
            args,
            token.GetLocation()));
    }

    return nullptr;
}

SharedPtr<AstExpression> Parser::ParseTerm(
    bool overrideCommas,
    bool overrideFatArrows,
    bool overrideAngleBrackets,
    bool overrideSquareBrackets,
    bool overrideParentheses,
    bool overrideQuestionMark)
{
    Token token = Token::EMPTY;

    // Skip comments, newlines, etc between terms
    do
    {
        token = m_tokenStream->Peek();
    }
    while (Match(TokenClass::TK_NEWLINE, true));

    if (!token)
    {
        m_compilationUnit->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_unexpected_eof,
            CurrentLocation()));

        if (m_tokenStream->HasNext())
        {
            m_tokenStream->Next();
        }

        return nullptr;
    }

    SharedPtr<AstExpression> expr;

    if (Match(TK_OPEN_PARENTH))
    {
        expr = ParseParentheses();
    }
    else if (Match(TK_OPEN_BRACKET))
    {
        expr = ParseArrayExpression();
    }
    else if (Match(TK_OPEN_BRACE))
    {
        expr = ParseHashMap();
    }
    else if (Match(TK_INTEGER))
    {
        expr = ParseIntegerLiteral();
    }
    else if (Match(TK_FLOAT))
    {
        expr = ParseFloatLiteral();
    }
    else if (Match(TK_STRING))
    {
        expr = ParseStringLiteral();
    }
    else if (Match(TK_NAME_LITERAL))
    {
        expr = ParseNameLiteral();
    }
    else if (Match(TK_IDENT))
    {
        if (MatchAhead(TK_DOUBLE_COLON, 1))
        {
            expr = ParseModuleAccess();
        }
        else
        {
            expr = ParseIdentifier();
        }
    }
    else if (Match(TK_DOUBLE_COLON))
    {
        expr = ParseModuleAccess();
    }
    else if (MatchKeyword(Keyword_self))
    {
        expr = ParseIdentifier(true);
    }
    else if (MatchKeyword(Keyword_true))
    {
        expr = ParseTrue();
    }
    else if (MatchKeyword(Keyword_false))
    {
        expr = ParseFalse();
    }
    else if (MatchKeyword(Keyword_null))
    {
        expr = ParseNil();
    }
    else if (MatchKeyword(Keyword_new))
    {
        expr = ParseNewExpression();
    }
    else if (MatchKeyword(Keyword_func))
    {
        expr = ParseFunctionExpression();
    }
    else if (MatchKeyword(Keyword_switch, false))
    {
        expr = ParseSwitchExpression();
    }
    else if (MatchKeyword(Keyword_typeof))
    {
        expr = ParseTypeOfExpression();
    }
    else if (MatchKeyword(Keyword_throw))
    {
        expr = ParseThrowExpression();
    }
    else if (Match(TK_OPERATOR))
    {
        expr = ParseUnaryExpressionPrefix();
    }
    else
    {
        if (token.GetTokenClass() == TK_NEWLINE)
        {
            m_compilationUnit->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_unexpected_eol,
                token.GetLocation()));
        }
        else
        {
            m_compilationUnit->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_unexpected_token,
                token.GetLocation(),
                token.GetValue()));
        }

        if (m_tokenStream->HasNext())
        {
            m_tokenStream->Next();
        }

        return nullptr;
    }

    Token operatorToken = Token::EMPTY;

    while (expr != nullptr && (Match(TK_DOT) || (!overrideParentheses && Match(TK_OPEN_PARENTH)) ||
               //(!overrideFatArrows && Match(TK_FAT_ARROW)) ||
               (!overrideSquareBrackets && Match(TK_OPEN_BRACKET)) ||
               // (!overrideAngleBrackets && MatchOperator("<")) ||
               MatchKeyword(Keyword_has) || MatchKeyword(Keyword_is) || MatchKeyword(Keyword_as) || ((operatorToken = Match(TK_OPERATOR)) && Operator::IsUnaryOperator(operatorToken.GetValue(), OperatorType::POSTFIX))))
    {
        if (operatorToken)
        {
            expr = ParseUnaryExpressionPostfix(expr);
            operatorToken = Token::EMPTY;
        }

        if (Match(TK_DOT))
        {
            expr = ParseMemberExpression(expr);
        }

        if (!overrideSquareBrackets && Match(TK_OPEN_BRACKET))
        {
            expr = ParseArrayAccess(expr, overrideCommas, overrideFatArrows, overrideAngleBrackets, overrideSquareBrackets, overrideParentheses, overrideQuestionMark);
        }

        if (!overrideParentheses && Match(TK_OPEN_PARENTH))
        {
            expr = ParseCallExpression(expr);
        }

        if (MatchKeyword(Keyword_has))
        {
            expr = ParseHasExpression(expr);
        }

        if (MatchKeyword(Keyword_is))
        {
            expr = ParseIsExpression(expr);
        }

        if (MatchKeyword(Keyword_as))
        {
            expr = ParseAsExpression(expr);
        }
    }

    return expr;
}

SharedPtr<AstExpression> Parser::ParseParentheses()
{
    SourceLocation location = CurrentLocation();
    SharedPtr<AstExpression> expr;
    const size_t beforePos = m_tokenStream->GetPosition();

    Expect(TK_OPEN_PARENTH, true);

    if (!Match(TK_CLOSE_PARENTH) && !Match(TK_IDENT) && !Match(TK_KEYWORD))
    {
        expr = ParseExpression(true);
        Expect(TK_CLOSE_PARENTH, true);
    }
    else
    {
        if (Match(TK_CLOSE_PARENTH, true))
        {
            // if '()' found, it is a function with empty parameters
            // allow ParseFunctionParameters() to handle parentheses
            m_tokenStream->SetPosition(beforePos);

            Array<SharedPtr<AstParameter>> params;

            if (Match(TK_OPEN_PARENTH, true))
            {
                params = ParseFunctionParameters();
                Expect(TK_CLOSE_PARENTH, true);
            }

            expr = ParseFunctionExpression(
                false, /* requireKeyword */
                true,  /* parseBody */
                params);
        }
        else
        {
            bool foundFunctionToken = false;

            if (MatchKeyword(Keyword_const))
            {
                foundFunctionToken = true;
            }
            else
            {
                expr = ParseExpression(true);
            }

            if (Match(TK_COMMA) || Match(TK_COLON) || Match(TK_ELLIPSIS))
            {

                foundFunctionToken = true;
            }
            else if (Match(TK_CLOSE_PARENTH, false))
            {
                const size_t before = m_tokenStream->GetPosition();
                m_tokenStream->Next();

                // function return type
                if (Match(TK_RIGHT_ARROW))
                {
                    foundFunctionToken = true;
                }

                // go back to where it was before reading the ')' token
                m_tokenStream->SetPosition(before);
            }

            if (foundFunctionToken)
            {
                // go back to before open '(' found,
                // to allow ParseFunctionParameters() to handle it
                m_tokenStream->SetPosition(beforePos);

                Array<SharedPtr<AstParameter>> params;

                if (Match(TK_OPEN_PARENTH, true))
                {
                    params = ParseFunctionParameters();
                    Expect(TK_CLOSE_PARENTH, true);
                }

                // parse function parameters
                expr = ParseFunctionExpression(
                    false, /* requireKeyword */
                    true,  /* parseBody */
                    params);
            }
            else
            {
                Expect(TK_CLOSE_PARENTH, true);

                if (Match(TK_OPEN_BRACE, true))
                {
                    // if '{' found after ')', it is a function
                    m_tokenStream->SetPosition(beforePos);

                    Array<SharedPtr<AstParameter>> params;

                    if (Match(TK_OPEN_PARENTH, true))
                    {
                        params = ParseFunctionParameters();
                        Expect(TK_CLOSE_PARENTH, true);
                    }

                    expr = ParseFunctionExpression(
                        false, /* requireKeyword */
                        true,  /* parseBody */
                        params);
                }
            }
        }
    }

    return expr;
}

SharedPtr<AstTemplateInstantiation> Parser::ParseTemplateInstantiation(SharedPtr<AstExpression> expr)
{
    if (!expr)
    {
        // not type spec
        return nullptr;
    }

    SourceLocation location = CurrentLocation();
    const size_t beforePos = m_tokenStream->GetPosition();

    Array<SharedPtr<AstTypeSpecifier>> args;

    auto parseFunctionReturnType = [&]() -> SharedPtr<AstTypeSpecifier>
    {
        // right arrow for function return type is part of the generic args
        if (Match(TK_RIGHT_ARROW, true))
        {
            // parse return type, add as first argument
            if (SharedPtr<AstTypeSpecifier> returnType = ParseTypeSpecifier())
            {
                return returnType;
            }
        }

        return nullptr;
    };

    if (Token token = ExpectOperator("<", true))
    {
        bool breakout = false;

        // If we see a '>>' here, split it into two '>' so the first one can close this instantiation
        if (MatchOperator(">>", false))
        {
            // replace current token with two '>' tokens at current position
            const SourceLocation loc = m_tokenStream->Peek().GetLocation();
            auto& tokens = m_tokenStream->m_tokens;
            const size_t pos = m_tokenStream->m_position;
            // erase the '>>' token
            tokens.Erase(tokens.Begin() + pos);
            // insert two '>' tokens in its place
            tokens.Insert(tokens.Begin() + pos, Token(TK_OPERATOR, ">", loc));
            tokens.Insert(tokens.Begin() + pos + 1, Token(TK_OPERATOR, ">", loc));
        }

        if (MatchOperator(">", true))
        {
            return SharedPtr<AstTemplateInstantiation>(new AstTemplateInstantiation(
                expr,
                args,
                parseFunctionReturnType(),
                token.GetLocation()));
        }

        ++m_templateArgumentDepth;

        do
        {
            const SourceLocation argLocation = CurrentLocation();
            bool isSplatArg = false;
            bool isNamedArg = false;
            String argName;

            if (Match(TK_ELLIPSIS, true))
            {
                isSplatArg = true;
            }

            // check for name: value expressions (named arguments)
            if (Match(TK_IDENT))
            {
                if (MatchAhead(TK_COLON, 1))
                {
                    // named argument
                    isNamedArg = true;
                    Token nameToken = Expect(TK_IDENT, true);
                    argName = nameToken.GetValue();

                    // read the colon
                    Expect(TK_COLON, true);
                }
            }

            if (SharedPtr<AstTypeSpecifier> arg = ParseTypeSpecifier())
            { // override commas
                args.PushBack(std::move(arg));
            }
            else
            {
                // not an argument, revert to start.
                m_tokenStream->SetPosition(beforePos);

                breakout = true;

                // not a template instantiation, revert to start.
                break;
            }
        }
        while (Match(TK_COMMA, true));

        --m_templateArgumentDepth;

        if (!breakout)
        {
            // If we see a '>>' here, split it so we can consume one '>' and leave the other for the outer template
            if (MatchOperator(">>", false))
            {
                const SourceLocation loc = m_tokenStream->Peek().GetLocation();
                auto& tokens = m_tokenStream->m_tokens;
                const size_t pos = m_tokenStream->m_position;
                tokens.Erase(tokens.Begin() + pos);
                tokens.Insert(tokens.Begin() + pos, Token(TK_OPERATOR, ">", loc));
                tokens.Insert(tokens.Begin() + pos + 1, Token(TK_OPERATOR, ">", loc));
            }

            if (MatchOperator(">", true))
            {
                return SharedPtr<AstTemplateInstantiation>(new AstTemplateInstantiation(
                    expr,
                    args,
                    parseFunctionReturnType(),
                    token.GetLocation()));
            }
        }

        // no closing bracket found, revert to start.
        m_tokenStream->SetPosition(beforePos);
    }

    return nullptr;
}

SharedPtr<AstConstant> Parser::ParseIntegerLiteral()
{
    if (Token token = Expect(TK_INTEGER, true))
    {
        if (token.GetFlags()[0] != '\0')
        {
            auto suffixIt = Lexer::s_numericSuffixes.Find(String(token.GetFlags()));
            Assert(suffixIt != Lexer::s_numericSuffixes.End());

            if (token.GetFlags()[0] == 'i' || token.GetFlags()[0] == 'l')
            {
                const char* beginPtr = token.GetValue().Data();
                char* endPtr = nullptr;

                const int64 value = std::strtoll(beginPtr, &endPtr, 10);

                if (!endPtr || (endPtr - beginPtr) < token.GetValue().Size())
                {
                    m_compilationUnit->GetErrorList().AddError(CompilerError(
                        LEVEL_ERROR,
                        Msg_illegal_expression,
                        token.GetLocation()));

                    return nullptr;
                }

                return SharedPtr<AstInteger>(new AstInteger(
                    value,
                    suffixIt->second.cbs,
                    token.GetLocation()));
            }
            else if (token.GetFlags()[0] == 'u')
            {
                const char* beginPtr = token.GetValue().Data();
                char* endPtr = nullptr;

                const uint64 value = std::strtoull(beginPtr, &endPtr, 10);

                if (!endPtr || (endPtr - beginPtr) < token.GetValue().Size())
                {
                    m_compilationUnit->GetErrorList().AddError(CompilerError(
                        LEVEL_ERROR,
                        Msg_illegal_expression,
                        token.GetLocation()));

                    return nullptr;
                }

                return SharedPtr<AstUnsignedInteger>(new AstUnsignedInteger(
                    value,
                    suffixIt->second.cbs,
                    token.GetLocation()));
            }
            else if (token.GetFlags()[0] == 'f')
            {
                const char* beginPtr = token.GetValue().Data();
                char* endPtr = nullptr;

                const double value = std::strtod(beginPtr, &endPtr);

                if (!endPtr || (endPtr - beginPtr) < token.GetValue().Size())
                {
                    m_compilationUnit->GetErrorList().AddError(CompilerError(
                        LEVEL_ERROR,
                        Msg_illegal_expression,
                        token.GetLocation()));

                    return nullptr;
                }

                return SharedPtr<AstFloat>(new AstFloat(
                    value,
                    suffixIt->second.cbs,
                    token.GetLocation()));
            }
            else
            {
                m_compilationUnit->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_unrecognized_numeric_suffix,
                    token.GetLocation(),
                    String(token.GetFlags())));
            }
        }
        else
        {
            const char* beginPtr = token.GetValue().Data();
            char* endPtr = nullptr;

            const int64 value = std::strtoll(beginPtr, &endPtr, 10);

            if (!endPtr || (endPtr - beginPtr) < token.GetValue().Size())
            {
                m_compilationUnit->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_illegal_expression,
                    token.GetLocation()));

                return nullptr;
            }

            return SharedPtr<AstInteger>(new AstInteger(
                value,
                CBS_32,
                token.GetLocation()));
        }
    }

    return nullptr;
}

SharedPtr<AstFloat> Parser::ParseFloatLiteral()
{
    if (Token token = Expect(TK_FLOAT, true))
    {
        const double value = std::atof(token.GetValue().Data());

        if (token.GetFlags()[0] == 'f')
        {
            return SharedPtr<AstFloat>(new AstFloat(
                value,
                CBS_32,
                token.GetLocation()));
        }

        return SharedPtr<AstFloat>(new AstFloat(
            value,
            CBS_64,
            token.GetLocation()));
    }

    return nullptr;
}

SharedPtr<AstString> Parser::ParseStringLiteral()
{
    if (Token token = Expect(TK_STRING, true))
    {
        return SharedPtr<AstString>(new AstString(
            token.GetValue(),
            token.GetLocation()));
    }

    return nullptr;
}

SharedPtr<AstName> Parser::ParseNameLiteral()
{
    if (Token token = Expect(TK_NAME_LITERAL, true))
    {
        return SharedPtr<AstName>(new AstName(
            token.GetValue(),
            token.GetLocation()));
    }

    return nullptr;
}

SharedPtr<AstIdentifier> Parser::ParseIdentifier(bool allowKeyword)
{
    if (Token token = ExpectIdentifier(allowKeyword, false))
    {
        // read identifier token
        if (m_tokenStream->HasNext())
        {
            m_tokenStream->Next();
        }

        // return variable
        return SharedPtr<AstVariable>(new AstVariable(
            token.GetValue(),
            token.GetLocation()));
    }

    return nullptr;
}

SharedPtr<AstArgument> Parser::ParseArgument(SharedPtr<AstExpression> expr)
{
    SourceLocation location = CurrentLocation();

    bool isSplatArg = false;
    bool isNamedArg = false;
    String argName;

    if (expr == nullptr)
    {
        if (Match(TK_ELLIPSIS, true))
        {
            isSplatArg = true;
        }
        else if (Match(TK_IDENT))
        { // check for name: value expressions (named arguments)
            if (MatchAhead(TK_COLON, 1))
            {
                // named argument
                isNamedArg = true;
                Token nameToken = Expect(TK_IDENT, true);
                argName = nameToken.GetValue();

                // read the colon
                Expect(TK_COLON, true);
            }
        }

        expr = ParseExpression(true, true);
    }

    if (expr != nullptr)
    {
        return SharedPtr<AstArgument>(new AstArgument(
            expr,
            isSplatArg,
            isNamedArg,
            false,
            false,
            argName,
            location));
    }

    m_compilationUnit->GetErrorList().AddError(CompilerError(
        LEVEL_ERROR,
        Msg_illegal_expression,
        location));

    return nullptr;
}

SharedPtr<AstArgumentList> Parser::ParseArguments(bool requireParentheses)
{
    const SourceLocation location = CurrentLocation();

    Array<SharedPtr<AstArgument>> args;

    if (requireParentheses)
    {
        Expect(TK_OPEN_PARENTH, true);
    }

    while (!requireParentheses || !Match(TK_CLOSE_PARENTH, false))
    {
        if (auto arg = ParseArgument(nullptr))
        {
            args.PushBack(arg);

            if (!Match(TK_COMMA, true))
            {
                break;
            }
        }
        else
        {
            return nullptr;
        }
    }

    if (requireParentheses)
    {
        Expect(TK_CLOSE_PARENTH, true);
    }

    return SharedPtr<AstArgumentList>(new AstArgumentList(
        args,
        location));
}

SharedPtr<AstCallExpression> Parser::ParseCallExpression(SharedPtr<AstExpression> target, bool requireParentheses)
{
    if (target != nullptr)
    {
        if (auto args = ParseArguments(requireParentheses))
        {
            return SharedPtr<AstCallExpression>(new AstCallExpression(
                target,
                args->GetArguments(),
                true, // allow 'self' to be inserted
                target->GetLocation()));
        }
    }

    return nullptr;
}

SharedPtr<AstModuleAccess> Parser::ParseModuleAccess()
{
    const SourceLocation location = CurrentLocation();

    Token token = Token::EMPTY;
    bool globalModuleAccess = false;

    if (Match(TK_DOUBLE_COLON, true))
    {
        // global module access for prepended double colon.
        globalModuleAccess = true;
    }
    else
    {
        token = Expect(TK_IDENT, true);
        Expect(TK_DOUBLE_COLON, true);
    }

    if (token || globalModuleAccess)
    {
        SharedPtr<AstExpression> expr;

        if (MatchAhead(TK_DOUBLE_COLON, 1))
        {
            expr = ParseModuleAccess();
        }
        else
        {
            expr = ParseIdentifier(true);
        }

        if (expr != nullptr)
        {
            return SharedPtr<AstModuleAccess>(new AstModuleAccess(
                globalModuleAccess
                    ? ScriptConfig::GlobalModuleName
                    : token.GetValue(),
                expr,
                location));
        }
    }

    return nullptr;
}

SharedPtr<AstExpression> Parser::ParseMemberExpression(SharedPtr<AstExpression> target)
{
    Expect(TK_DOT, true);

    // allow quoted strings as data member names
    Token ident = Match(TK_STRING, false)
        ? m_tokenStream->Next()
        : ExpectIdentifier(true, true);

    SharedPtr<AstExpression> expr;

    if (ident)
    {
        expr = SharedPtr<AstMember>(new AstMember(
            ident.GetValue(),
            target,
            ident.GetLocation()));
    }

    return expr;
}

SharedPtr<AstArrayAccess> Parser::ParseArrayAccess(
    SharedPtr<AstExpression> target,
    bool overrideCommas,
    bool overrideFatArrows,
    bool overrideAngleBrackets,
    bool overrideSquareBrackets,
    bool overrideParentheses,
    bool overrideQuestionMark)
{
    if (Token token = Expect(TK_OPEN_BRACKET, true))
    {
        SharedPtr<AstExpression> expr;
        // SharedPtr<AstExpression> rhs;

        if (Match(TK_CLOSE_BRACKET, true))
        {
            m_compilationUnit->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_illegal_expression,
                token.GetLocation()));
        }
        else
        {
            expr = ParseExpression();
            Expect(TK_CLOSE_BRACKET, true);
        }

        // // check for assignment operator
        // Token operatorToken = Token::EMPTY;

        // if (Token operatorToken = Match(TK_OPERATOR))
        // {
        //     if (Operator::IsBinaryOperator(operatorToken.GetValue(), OperatorType::ASSIGNMENT))
        //     {
        //         // eat the operator token
        //         m_tokenStream->Next();

        //         rhs = ParseExpression(
        //             overrideCommas,
        //             overrideFatArrows,
        //             overrideAngleBrackets,
        //             overrideQuestionMark);
        //     }
        // }

        if (expr != nullptr)
        {
            return SharedPtr<AstArrayAccess>(new AstArrayAccess(
                target,
                expr,
                true, // allow operator overloading for []
                token.GetLocation()));
        }
    }

    return nullptr;
}

SharedPtr<AstHasExpression> Parser::ParseHasExpression(SharedPtr<AstExpression> target)
{
    if (Token token = ExpectKeyword(Keyword_has, true))
    {
        if (Token field = Expect(TK_STRING, true))
        {
            return SharedPtr<AstHasExpression>(new AstHasExpression(
                target,
                field.GetValue(),
                target->GetLocation()));
        }
    }

    return nullptr;
}

SharedPtr<AstIsExpression> Parser::ParseIsExpression(SharedPtr<AstExpression> target)
{
    if (Token token = ExpectKeyword(Keyword_is, true))
    {
        if (auto typeExpression = ParseTypeSpecifier())
        {
            return SharedPtr<AstIsExpression>(new AstIsExpression(
                target,
                typeExpression,
                target->GetLocation()));
        }
    }

    return nullptr;
}

SharedPtr<AstAsExpression> Parser::ParseAsExpression(SharedPtr<AstExpression> target)
{
    if (Token token = ExpectKeyword(Keyword_as, true))
    {
        if (auto typeExpression = ParseTypeSpecifier())
        {
            return SharedPtr<AstAsExpression>(new AstAsExpression(
                target,
                typeExpression,
                target->GetLocation()));
        }
    }

    return nullptr;
}

SharedPtr<AstNewExpression> Parser::ParseNewExpression()
{
    if (Token token = ExpectKeyword(Keyword_new, true))
    {
        if (auto proto = ParseTypeSpecifier())
        {
            SharedPtr<AstArgumentList> argList;

            if (Match(TK_OPEN_PARENTH, false))
            {
                // parse args
                argList = ParseArguments();
            }

            return SharedPtr<AstNewExpression>(new AstNewExpression(
                proto,
                argList,
                true, // enable construct call
                token.GetLocation()));
        }
    }

    return nullptr;
}

SharedPtr<AstTrue> Parser::ParseTrue()
{
    if (Token token = ExpectKeyword(Keyword_true, true))
    {
        return SharedPtr<AstTrue>(new AstTrue(
            token.GetLocation()));
    }

    return nullptr;
}

SharedPtr<AstFalse> Parser::ParseFalse()
{
    if (Token token = ExpectKeyword(Keyword_false, true))
    {
        return SharedPtr<AstFalse>(new AstFalse(
            token.GetLocation()));
    }

    return nullptr;
}

SharedPtr<AstNil> Parser::ParseNil()
{
    if (Token token = ExpectKeyword(Keyword_null, true))
    {
        return SharedPtr<AstNil>(new AstNil(
            token.GetLocation()));
    }

    return nullptr;
}

SharedPtr<AstBlock> Parser::ParseBlock(bool requireBraces, bool skipEnd, bool endOnCatch)
{
    SourceLocation location = CurrentLocation();

    SkipStatementTerminators();

    if (requireBraces)
    {
        if (!Expect(TK_OPEN_BRACE, true))
        {
            return nullptr;
        }
    }

    SharedPtr<AstBlock> block(new AstBlock(location));

    while (requireBraces ? !Match(TK_CLOSE_BRACE, false) : (!MatchKeyword(Keyword_end, false) && !MatchKeyword(Keyword_else, false) && !(endOnCatch && MatchKeyword(Keyword_catch, false))))
    {
        // skip statement terminator tokens
        if (!Match(TK_SEMICOLON, true) && !Match(TK_NEWLINE, true))
        {
            if (auto stmt = ParseStatement())
            {
                block->AddChild(stmt);
            }
            else
            {
                break;
            }
        }
    }

    if (requireBraces)
    {
        Expect(TK_CLOSE_BRACE, true);
    }
    else if (!skipEnd)
    {
        ExpectKeyword(Keyword_end, true);
    }

    return block;
}

SharedPtr<AstIfStatement> Parser::ParseIfStatement()
{
    if (Token token = ExpectKeyword(Keyword_if, true))
    {
        bool hasParentheses = false;

        if (Match(TK_OPEN_PARENTH, true))
        {
            hasParentheses = true;
        }

        SharedPtr<AstExpression> conditional;
        if (!(conditional = ParseExpression()))
        {
            return nullptr;
        }

        if (hasParentheses)
        {
            if (!Match(TK_CLOSE_PARENTH, true))
            {
                m_compilationUnit->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_unmatched_parentheses,
                    token.GetLocation()));

                if (m_tokenStream->HasNext())
                {
                    m_tokenStream->Next();
                }
            }
        }

        bool useBraces = !Match(TK_OPEN_BRACE, false).Empty();

        SharedPtr<AstBlock> block;
        if (!(block = ParseBlock(/* requireBraces */ useBraces, /* skipEnd */ true)))
        {
            return nullptr;
        }

        SharedPtr<AstBlock> elseBlock = nullptr;
        bool isElseIfChain = false; // true if this if has an else-if, so 'end' is consumed by the inner if
        // parse else statement if the "else" keyword is found
        if (Token elseToken = MatchKeyword(Keyword_else, true))
        {
            // check for "if" keyword for else-if
            if (MatchKeyword(Keyword_if, false))
            {
                isElseIfChain = true;

                elseBlock = SharedPtr<AstBlock>(new AstBlock(
                    elseToken.GetLocation()));

                if (auto elseIfBlock = ParseIfStatement())
                {
                    elseBlock->AddChild(elseIfBlock);
                }
            }
            else
            {
                const bool useBraces = !Match(TK_OPEN_BRACE, false).Empty();

                // parse block after "else" keyword
                if (!(elseBlock = ParseBlock(/* requireBraces */ useBraces, /* skipEnd */ true)))
                {
                    return nullptr;
                }
            }
        }

        if (!useBraces && !isElseIfChain)
        {
            if (!ExpectKeyword(Keyword_end, true))
            {
                return nullptr;
            }
        }

        return SharedPtr<AstIfStatement>(new AstIfStatement(
            conditional,
            block,
            elseBlock,
            token.GetLocation()));
    }

    return nullptr;
}

SharedPtr<AstSwitchExpression> Parser::ParseSwitchExpression()
{
    if (Token token = ExpectKeyword(Keyword_switch, true))
    {
        bool hasParentheses = false;

        if (Match(TK_OPEN_PARENTH, true))
        {
            hasParentheses = true;
        }

        SharedPtr<AstExpression> expression;
        if (!(expression = ParseExpression()))
        {
            return nullptr;
        }

        if (hasParentheses)
        {
            if (!Match(TK_CLOSE_PARENTH, true))
            {
                m_compilationUnit->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_unmatched_parentheses,
                    token.GetLocation()));

                if (m_tokenStream->HasNext())
                {
                    m_tokenStream->Next();
                }
            }
        }

        bool useBraces = !Match(TK_OPEN_BRACE, false).Empty();

        if (useBraces)
        {
            if (!Expect(TK_OPEN_BRACE, true))
            {
                return nullptr;
            }
        }

        Array<CaseClause> clauses;

        while (useBraces
            ? !Match(TK_CLOSE_BRACE, false)
            : !MatchKeyword(Keyword_end, false)
                && !MatchKeyword(Keyword_else, false))
        {
            if (!Match(TK_SEMICOLON, true) && !Match(TK_NEWLINE, true))
            {
                if (MatchKeyword(Keyword_case, false))
                {
                    Token caseTok = ExpectKeyword(Keyword_case, true);

                    SharedPtr<AstExpression> caseValue;
                    if (!(caseValue = ParseExpression()))
                    {
                        return nullptr;
                    }

                    if (!Match(TK_COLON, true))
                    {
                        m_compilationUnit->GetErrorList().AddError(CompilerError(
                            LEVEL_ERROR,
                            Msg_expected_token,
                            CurrentLocation(),
                            ":"));

                        return nullptr;
                    }

                    SharedPtr<AstBlock> caseBlock(new AstBlock(caseTok.GetLocation()));

                    while (useBraces
                        ? !Match(TK_CLOSE_BRACE, false)
                        : !MatchKeyword(Keyword_end, false)
                            && !MatchKeyword(Keyword_else, false))
                    {
                        if (!Match(TK_SEMICOLON, true) && !Match(TK_NEWLINE, true))
                        {
                            if (MatchKeyword(Keyword_case, false)
                                || MatchKeyword(Keyword_default, false)
                                || (useBraces && Match(TK_CLOSE_BRACE, false))
                                || (!useBraces && (MatchKeyword(Keyword_end, false) || MatchKeyword(Keyword_else, false))))
                            {
                                break;
                            }

                            if (auto stmt = ParseStatement())
                            {
                                caseBlock->AddChild(stmt);
                            }
                            else
                            {
                                break;
                            }
                        }
                    }

                    CaseClause clause;
                    clause.m_value = caseValue;
                    clause.m_block = caseBlock;
                    clause.m_isDefault = false;
                    clauses.PushBack(clause);
                }
                else if (MatchKeyword(Keyword_default, false))
                {
                    Token defaultTok = ExpectKeyword(Keyword_default, true);

                    if (!Match(TK_COLON, true))
                    {
                        m_compilationUnit->GetErrorList().AddError(CompilerError(
                            LEVEL_ERROR,
                            Msg_expected_token,
                            CurrentLocation(),
                            ":"));

                        return nullptr;
                    }

                    SharedPtr<AstBlock> defaultBlock(new AstBlock(defaultTok.GetLocation()));

                    while (useBraces
                        ? !Match(TK_CLOSE_BRACE, false)
                        : !MatchKeyword(Keyword_end, false)
                            && !MatchKeyword(Keyword_else, false))
                    {
                        if (!Match(TK_SEMICOLON, true) && !Match(TK_NEWLINE, true))
                        {
                            if (MatchKeyword(Keyword_case, false)
                                || MatchKeyword(Keyword_default, false)
                                || (useBraces && Match(TK_CLOSE_BRACE, false))
                                || (!useBraces && (MatchKeyword(Keyword_end, false) || MatchKeyword(Keyword_else, false))))
                            {
                                break;
                            }

                            if (auto stmt = ParseStatement())
                            {
                                defaultBlock->AddChild(stmt);
                            }
                            else
                            {
                                break;
                            }
                        }
                    }

                    CaseClause clause;
                    clause.m_value = nullptr;
                    clause.m_block = defaultBlock;
                    clause.m_isDefault = true;
                    clauses.PushBack(clause);
                }
                else if (auto stmt = ParseStatement())
                {
                    m_compilationUnit->GetErrorList().AddError(CompilerError(
                        LEVEL_ERROR,
                        Msg_case_outside_switch,
                        CurrentLocation()));
                }
                else
                {
                    break;
                }
            }
        }

        if (useBraces)
        {
            Expect(TK_CLOSE_BRACE, true);
        }
        else
        {
            ExpectKeyword(Keyword_end, true);
        }

        return SharedPtr<AstSwitchExpression>(new AstSwitchExpression(
            expression,
            clauses,
            token.GetLocation()));
    }

    return nullptr;
}

SharedPtr<AstWhileLoop> Parser::ParseWhileLoop()
{
    if (Token token = ExpectKeyword(Keyword_while, true))
    {
        if (!Expect(TK_OPEN_PARENTH, true))
        {
            return nullptr;
        }

        SharedPtr<AstExpression> conditional;
        if (!(conditional = ParseExpression()))
        {
            return nullptr;
        }

        if (!Expect(TK_CLOSE_PARENTH, true))
        {
            return nullptr;
        }

        const bool useBraces = !Match(TK_OPEN_BRACE, false).Empty();

        SharedPtr<AstBlock> block;
        if (!(block = ParseBlock(/* requireBraces */ useBraces, /* skipEnd */ false)))
        {
            return nullptr;
        }

        return SharedPtr<AstWhileLoop>(new AstWhileLoop(
            conditional,
            block,
            token.GetLocation()));
    }

    return nullptr;
}

SharedPtr<AstStatement> Parser::ParseForLoop()
{
    if (Token token = ExpectKeyword(Keyword_for, true))
    {
        if (!Expect(TK_OPEN_PARENTH, true))
        {
            return nullptr;
        }

        SharedPtr<AstStatement> declPart;

        if (!Match(TK_SEMICOLON))
        {
            if (!(declPart = ParseStatement(false, false)))
            {
                return nullptr;
            }
        }

        if (MatchKeyword(Keyword_in, false))
        {
            return ParseForEachLoop(token, declPart);
        }

        if (!Expect(TK_SEMICOLON, true))
        {
            return nullptr;
        }

        SharedPtr<AstExpression> conditionPart;

        if (!Match(TK_SEMICOLON))
        {
            if (!(conditionPart = ParseExpression()))
            {
                return nullptr;
            }
        }

        if (!Expect(TK_SEMICOLON, true))
        {
            return nullptr;
        }

        SharedPtr<AstExpression> incrementPart;

        if (!Match(TK_CLOSE_PARENTH))
        {
            if (!(incrementPart = ParseExpression()))
            {
                return nullptr;
            }
        }

        if (!Expect(TK_CLOSE_PARENTH, true))
        {
            return nullptr;
        }

        SkipStatementTerminators();

        const bool useBraces = !Match(TK_OPEN_BRACE, false).Empty();

        SharedPtr<AstBlock> block;
        if (!(block = ParseBlock(/* requireBraces */ useBraces, /* skipEnd */ false)))
        {
            return nullptr;
        }

        return SharedPtr<AstForLoop>(new AstForLoop(
            declPart,
            conditionPart,
            incrementPart,
            block,
            token.GetLocation()));
    }

    return nullptr;
}

static SharedPtr<AstVariableDeclaration> MakeVarDeclFromExpression(
    const SharedPtr<AstStatement>& stmt,
    const SourceLocation& location)
{
    if (stmt->IsA<AstVariableDeclaration>())
    {
        return stmt.CastUnchecked<AstVariableDeclaration>();
    }

    if (auto* variable = DynamicCast<AstVariable>(stmt.Get()))
    {
        return SharedPtr<AstVariableDeclaration>(new AstVariableDeclaration(
            variable->GetName(),
            nullptr,
            nullptr,
            IdentifierFlags::NONE,
            location));
    }

    return nullptr;
}

SharedPtr<AstStatement> Parser::ParseForEachLoop(const Token& forToken, const SharedPtr<AstStatement>& declPart)
{
    // We've already consumed: for ( <declPart>
    // Now we need to consume: in <iterable> ) <body>

    if (!ExpectKeyword(Keyword_in, true))
    {
        return nullptr;
    }

    SharedPtr<AstVariableDeclaration> varDecl = MakeVarDeclFromExpression(declPart, forToken.GetLocation());

    if (varDecl == nullptr)
    {
        m_compilationUnit->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_expected_identifier,
            forToken.GetLocation()));

        return nullptr;
    }

    SharedPtr<AstExpression> iterable;
    if (!(iterable = ParseExpression()))
    {
        return nullptr;
    }

    if (!Expect(TK_CLOSE_PARENTH, true))
    {
        return nullptr;
    }

    SkipStatementTerminators();

    const bool useBraces = !Match(TK_OPEN_BRACE, false).Empty();

    SharedPtr<AstBlock> block;
    if (!(block = ParseBlock(/* requireBraces */ useBraces, /* skipEnd */ false)))
    {
        return nullptr;
    }

    return SharedPtr<AstForEachLoop>(new AstForEachLoop(
        varDecl,
        iterable,
        block,
        forToken.GetLocation()));
}

SharedPtr<AstStatement> Parser::ParseBreakStatement()
{
    if (Token token = ExpectKeyword(Keyword_break, true))
    {
        return SharedPtr<AstBreakStatement>(new AstBreakStatement(
            token.GetLocation()));
    }

    return nullptr;
}

SharedPtr<AstStatement> Parser::ParseContinueStatement()
{
    if (Token token = ExpectKeyword(Keyword_continue, true))
    {
        return SharedPtr<AstContinueStatement>(new AstContinueStatement(
            token.GetLocation()));
    }

    return nullptr;
}

SharedPtr<AstTryCatch> Parser::ParseTryCatchStatement()
{
    if (Token token = ExpectKeyword(Keyword_try, true))
    {
        bool useBraces = !Match(TK_OPEN_BRACE, false).Empty();

        SharedPtr<AstBlock> tryBlock = ParseBlock(/* requireBraces */ useBraces, /* skipEnd */ true, /* endOnCatch */ !useBraces);
        SharedPtr<AstBlock> catchBlock;

        SkipStatementTerminators();

        if (ExpectKeyword(Keyword_catch, true))
        {
            // TODO: Add exception argument

            useBraces = !Match(TK_OPEN_BRACE, false).Empty();

            catchBlock = ParseBlock(useBraces, /* skipEnd */ true);
        }

        if (!useBraces)
        {
            if (!ExpectKeyword(Keyword_end, true)) {
                return nullptr;
            }
        }

        // @TODO finally

        if (tryBlock != nullptr && catchBlock != nullptr)
        {
            return SharedPtr<AstTryCatch>(new AstTryCatch(
                tryBlock,
                catchBlock,
                token.GetLocation()));
        }
    }

    return nullptr;
}

SharedPtr<AstThrowExpression> Parser::ParseThrowExpression()
{
    if (Token token = ExpectKeyword(Keyword_throw, true))
    {
        if (auto expr = ParseExpression())
        {
            return SharedPtr<AstThrowExpression>(new AstThrowExpression(
                expr,
                token.GetLocation()));
        }
    }

    return nullptr;
}

SharedPtr<AstExpression> Parser::ParseBinaryExpression(
    int exprPrec,
    SharedPtr<AstExpression> left)
{
    while (true)
    {
        // get precedence
        const Operator* op = nullptr;

        int precedence = OperatorPrecedence(op);

        if (precedence < exprPrec)
        {
            return left;
        }

        // read the operator token
        Token token = Expect(TK_OPERATOR, true);

        if (SharedPtr<AstExpression> right = ParseTerm())
        {
            // next part of expression's precedence
            const Operator* nextOp = nullptr;

            int nextPrec = OperatorPrecedence(nextOp);

            if (precedence < nextPrec)
            {
                right = ParseBinaryExpression(precedence + 1, right);

                if (!right)
                {
                    return nullptr;
                }
            }

            left = SharedPtr<AstBinaryExpression>(new AstBinaryExpression(
                left,
                right,
                op,
                token.GetLocation()));
        }
    }

    return nullptr;
}

SharedPtr<AstExpression> Parser::ParseUnaryExpressionPrefix()
{
    // read the operator token
    if (Token token = Expect(TK_OPERATOR, true))
    {
        const Operator* op = nullptr;
        if (Operator::IsUnaryOperator(token.GetValue(), /*OperatorType::PREFIX,*/ op))
        {
            if (auto term = ParseTerm())
            {
                return SharedPtr<AstUnaryExpression>(new AstUnaryExpression(
                    term,
                    op,
                    false, // postfix version
                    token.GetLocation()));
            }

            return nullptr;
        }
        else
        {
            // internal error: operator not defined
            m_compilationUnit->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_illegal_operator,
                token.GetLocation(),
                token.GetValue()));
        }
    }

    return nullptr;
}

SharedPtr<AstExpression> Parser::ParseUnaryExpressionPostfix(const SharedPtr<AstExpression>& expr)
{
    // read the operator token
    if (Token token = Expect(TK_OPERATOR, true))
    {
        const Operator* op = nullptr;
        if (Operator::IsUnaryOperator(token.GetValue(), /*OperatorType::POSTFIX,*/ op))
        {
            return SharedPtr<AstUnaryExpression>(new AstUnaryExpression(
                expr,
                op,
                true, // postfix version
                token.GetLocation()));
        }
        else
        {
            // internal error: operator not defined
            m_compilationUnit->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_illegal_operator,
                token.GetLocation(),
                token.GetValue()));
        }
    }

    return nullptr;
}

SharedPtr<AstExpression> Parser::ParseTernaryExpression(const SharedPtr<AstExpression>& conditional)
{
    if (Token token = Expect(TK_QUESTION_MARK, true))
    {
        // parse next ('true' part)

        auto trueExpr = ParseExpression();

        if (trueExpr == nullptr)
        {
            return nullptr;
        }

        if (!Expect(TK_COLON, true))
        {
            return nullptr;
        }

        auto falseExpr = ParseExpression();

        if (falseExpr == nullptr)
        {
            return nullptr;
        }

        return SharedPtr<AstTernaryExpression>(new AstTernaryExpression(
            conditional,
            trueExpr,
            falseExpr,
            conditional->GetLocation()));
    }

    return nullptr;
}

SharedPtr<AstExpression> Parser::ParseExpression(
    bool overrideCommas,
    bool overrideFatArrows,
    bool overrideAngleBrackets,
    bool overrideQuestionMark)
{
    if (auto term = ParseTerm(overrideCommas, overrideFatArrows, overrideAngleBrackets, false, false, overrideQuestionMark))
    {
        if (Match(TK_OPERATOR, false))
        {
            // drop out of expression, return to parent call
            if ((MatchOperator(">", false) || MatchOperator(">>", false)) && m_templateArgumentDepth > 0)
            {
                return term;
            }

            if (auto binExpr = ParseBinaryExpression(0, term))
            {
                term = binExpr;
            }
            else
            {
                return nullptr;
            }
        }

        if (Match(TK_QUESTION_MARK))
        {
            if (auto ternaryExpr = ParseTernaryExpression(term))
            {
                term = ternaryExpr;
            }
        }

        return term;
    }

    return nullptr;
}

SharedPtr<AstTypeSpecifier> Parser::ParseTypeSpecifier()
{
    const SourceLocation location = CurrentLocation();

    if (auto term = ParseTerm(
            true,  // override commas
            true,  // override =>
            true,  // override <>
            false, // override []
            true   // override ()
            ))
    {
        // if (Token token = Match(TK_OPEN_BRACKET, true))
        // {
        //     // array braces at the end of a type are syntactical sugar for `Array<T>`
        //     term = SharedPtr<AstTemplateInstantiation>(new AstTemplateInstantiation(
        //         SharedPtr<AstPrototypeSpecification>(new AstPrototypeSpecification(
        //             term,
        //             term->GetLocation())),
        //         { SharedPtr<AstPrototypeSpecification>(new AstPrototypeSpecification(
        //             SharedPtr<AstUnsignedInteger>(new AstUnsignedInteger(
        //                 0,
        //                 token.GetLocation())),
        //             token.GetLocation())) },
        //         term->GetLocation()));

        //     if (!Expect(TK_CLOSE_BRACKET, true))
        //     {
        //         return nullptr;
        //     }
        // }

        // check for template instantiation
        if (Token lt = MatchOperator("<", false))
        {
            if (SharedPtr<AstTemplateInstantiation> templateInstantiation = ParseTemplateInstantiation(term))
            {
                return templateInstantiation;
            }
        }

        return SharedPtr<AstTypeSpecifier>(new AstTypeSpecifier(term, location));
    }

    return nullptr;
}

SharedPtr<AstExpression> Parser::ParseAssignment()
{
    // read assignment expression
    const SourceLocation exprLocation = CurrentLocation();

    if (auto assignment = ParseExpression())
    {
        return assignment;
    }

    m_compilationUnit->GetErrorList().AddError(CompilerError(
        LEVEL_ERROR,
        Msg_illegal_expression,
        exprLocation));

    return nullptr;
}

SharedPtr<AstVariableDeclaration> Parser::ParseVariableDeclaration(
    bool allowKeywordNames,
    bool allowQuotedNames,
    EnumFlags<IdentifierFlags> flags)
{
    const SourceLocation location = CurrentLocation();

    static const TMap<Keywords, IdentifierFlags> s_prefixKeywordMap = {
        { Keyword_extern, IdentifierFlags::EXTERN },
        { Keyword_const, IdentifierFlags::CONSTANT },
        { Keyword_ref, IdentifierFlags::REF }
    };

    TSet<Keywords> usedSpecifiers;

    while (Match(TK_KEYWORD, false))
    {
        bool foundKeyword = false;

        for (const auto& it : s_prefixKeywordMap)
        {
            const Keywords keyword = it.first;
            const IdentifierFlags flag = it.second;

            if (MatchKeyword(keyword, true))
            {
                if (usedSpecifiers.Find(keyword) == usedSpecifiers.End())
                {
                    usedSpecifiers.Insert(keyword);

                    flags |= flag;

                    foundKeyword = true;

                    break;
                }
            }
        }

        if (!foundKeyword)
        {
            Token token = m_tokenStream->Next();

            m_compilationUnit->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_unexpected_token,
                token.GetLocation(),
                token.GetValue()));

            break;
        }
    }

    const bool isExtern = flags[IdentifierFlags::EXTERN];

    Token identifier = Token::EMPTY;

    // an identifier name that is enquoted in strings is valid
    if (allowQuotedNames)
    {
        identifier = Match(TK_STRING, false)
            ? m_tokenStream->Next()
            : ExpectIdentifier(allowKeywordNames, true);
    }
    else
    {
        identifier = ExpectIdentifier(allowKeywordNames, true);
    }

    if (!identifier.Empty())
    {
        SharedPtr<AstTypeSpecifier> typeSpec;
        SharedPtr<AstExpression> assignment;

        // if extern, we can't assign it
        bool requiresAssignmentOperator = !isExtern;

        if (Match(TK_COLON, true))
        {
            // read object type
            typeSpec = ParseTypeSpecifier();
        }
        else if (Match(TK_DEFINE, true))
        {
            requiresAssignmentOperator = false;
        }

        if (!requiresAssignmentOperator || MatchOperator("=", true))
        {
            if (isExtern)
            {
                m_compilationUnit->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_extern_cannot_have_assignment,
                    CurrentLocation()));
            }

            assignment = ParseAssignment();
        }

        return SharedPtr<AstVariableDeclaration>(new AstVariableDeclaration(
            identifier.GetValue(),
            typeSpec,
            assignment,
            flags,
            location));
    }

    return nullptr;
}

SharedPtr<AstStatement> Parser::ParseFunctionDefinition(bool requireKeyword)
{
    const SourceLocation location = CurrentLocation();

    EnumFlags<IdentifierFlags> flags = IdentifierFlags::CONSTANT
        | IdentifierFlags::FUNCTION;

    if (MatchKeyword(Keyword_extern, true))
    {
        flags |= IdentifierFlags::EXTERN;
    }

    if (requireKeyword)
    {
        if (!ExpectKeyword(Keyword_func, true))
        {
            return nullptr;
        }
    }
    else
    {
        // match and read in the case that it is found
        MatchKeyword(Keyword_func, true);
    }

    if (Token identifier = Expect(TK_IDENT, true))
    {
        SharedPtr<AstExpression> assignment;
        Array<SharedPtr<AstParameter>> params;

        if (Match(TK_OPEN_PARENTH, true))
        {
            params = ParseFunctionParameters();
            Expect(TK_CLOSE_PARENTH, true);
        }

        assignment = ParseFunctionExpression(
            false,                              /* requireKeyword */
            !(flags & IdentifierFlags::EXTERN), /* parseBody - externs have no body */
            params);

        if (!assignment)
        {
            return nullptr;
        }

        return SharedPtr<AstVariableDeclaration>(new AstVariableDeclaration(
            identifier.GetValue(),
            nullptr, // type specifier
            assignment,
            flags,
            location));
    }

    return nullptr;
}

SharedPtr<AstFunctionExpression> Parser::ParseFunctionExpression(
    bool requireKeyword,
    bool parseBody,
    Array<SharedPtr<AstParameter>> params)
{
    const Token token = requireKeyword
        ? ExpectKeyword(Keyword_func, true)
        : Token::EMPTY;

    const SourceLocation location = token
        ? token.GetLocation()
        : CurrentLocation();

    if (requireKeyword || !token)
    {
        if (requireKeyword)
        {
            // read params
            if (Match(TK_OPEN_PARENTH, true))
            {
                params = ParseFunctionParameters();
                Expect(TK_CLOSE_PARENTH, true);
            }
        }

        SharedPtr<AstTypeSpecifier> typeSpec;

        if (Match(TK_RIGHT_ARROW, true))
        {
            // read return type for functions
            typeSpec = ParseTypeSpecifier();
        }

        if (!parseBody)
        {
            return SharedPtr<AstFunctionExpression>(new AstFunctionExpression(
                params,
                typeSpec,
                nullptr, // no body
                location));
        }

        SharedPtr<AstBlock> block;

        if (Match(TK_FAT_ARROW, true))
        {
            SharedPtr<AstReturnStatement> returnStatement(new AstReturnStatement(
                ParseExpression(),
                location));

            block.Reset(new AstBlock(
                { std::move(returnStatement) },
                location));
        }
        else
        {
            SkipStatementTerminators();

            const bool useBraces = !Match(TK_OPEN_BRACE, false).Empty();

            block = ParseBlock(/* requireBraces */ useBraces, /* skipEnd */ false);
        }

        if (block != nullptr)
        {
            return SharedPtr<AstFunctionExpression>(new AstFunctionExpression(
                params,
                typeSpec,
                block,
                location));
        }
    }

    return nullptr;
}

SharedPtr<AstArrayExpression> Parser::ParseArrayExpression()
{
    if (Token token = Expect(TK_OPEN_BRACKET, true))
    {
        Array<SharedPtr<AstExpression>> members;

        do
        {
            if (Match(TK_CLOSE_BRACKET, false))
            {
                break;
            }

            if (auto expr = ParseExpression(true))
            {
                members.PushBack(expr);
            }
        }
        while (Match(TK_COMMA, true));

        Expect(TK_CLOSE_BRACKET, true);

        return SharedPtr<AstArrayExpression>(new AstArrayExpression(
            members,
            token.GetLocation()));
    }

    return nullptr;
}

SharedPtr<AstHashMap> Parser::ParseHashMap()
{
    if (Token token = Expect(TK_OPEN_BRACE, true))
    {
        Array<SharedPtr<AstExpression>> keys;
        Array<SharedPtr<AstExpression>> values;

        do
        {
            // skip newline tokens
            while (Match(TK_NEWLINE, true))
                ;

            if (Match(TK_CLOSE_BRACE, false))
            {
                break;
            }

            Token identToken = Token::EMPTY;

            // check for identifier, string, or keyword. if found, assume it is a key, with colon and value after
            if (((identToken = Match(TK_IDENT)) || (identToken = Match(TK_KEYWORD)) || (identToken = Match(TK_STRING))) && MatchAhead(TK_COLON, 1))
            {
                m_tokenStream->Next(); // eat the token
                m_tokenStream->Next(); // eat the colon

                keys.PushBack(SharedPtr<AstString>(new AstString(
                    identToken.GetValue(),
                    identToken.GetLocation())));
            }
            else
            {
                if (auto key = ParseExpression(true))
                {
                    keys.PushBack(std::move(key));
                }
                else
                {
                    // add error
                    m_compilationUnit->GetErrorList().AddError(CompilerError(
                        LEVEL_ERROR,
                        Msg_illegal_expression,
                        CurrentLocation()));
                }

                Expect(TK_FAT_ARROW, true);
            }

            if (auto value = ParseExpression(true))
            {
                values.PushBack(std::move(value));
            }
            else
            {
                // add error
                m_compilationUnit->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_illegal_expression,
                    CurrentLocation()));
            }
        }
        while (Match(TK_COMMA, true));

        // skip newline tokens
        while (Match(TK_NEWLINE, true))
            ;

        Expect(TK_CLOSE_BRACE, true);

        return SharedPtr<AstHashMap>(new AstHashMap(
            keys,
            values,
            token.GetLocation()));
    }

    return nullptr;
}

SharedPtr<AstTypeOfExpression> Parser::ParseTypeOfExpression()
{
    const SourceLocation location = CurrentLocation();

    if (Token token = ExpectKeyword(Keyword_typeof, true))
    {
        SourceLocation exprLocation = CurrentLocation();
        if (auto term = ParseTerm())
        {
            return SharedPtr<AstTypeOfExpression>(new AstTypeOfExpression(
                term,
                location));
        }
        else
        {
            m_compilationUnit->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_illegal_expression,
                exprLocation));
        }
    }

    return nullptr;
}

Array<SharedPtr<AstParameter>> Parser::ParseFunctionParameters()
{
    Array<SharedPtr<AstParameter>> parameters;

    bool foundVariadic = false;
    bool keepReading = true;

    while (keepReading)
    {
        Token token = Token::EMPTY;

        if (Match(TK_CLOSE_PARENTH, false))
        {
            keepReading = false;
            break;
        }

        EnumFlags<IdentifierFlags> flags = IdentifierFlags::NONE;

        if (MatchKeyword(Keyword_const, true))
        {
            flags |= IdentifierFlags::CONSTANT;
        }

        if (MatchKeyword(Keyword_ref, true))
        {
            flags |= IdentifierFlags::REF;
        }

        if ((token = ExpectIdentifier(true, true)))
        {
            SharedPtr<AstTypeSpecifier> typeSpec;
            SharedPtr<AstExpression> defaultParam;

            // check if parameter type has been declared
            if (Match(TK_COLON, true))
            {
                typeSpec = ParseTypeSpecifier();
            }

            if (foundVariadic)
            {
                // found another parameter after variadic
                m_compilationUnit->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_argument_after_varargs,
                    token.GetLocation()));
            }

            // if this parameter is variadic
            bool isVariadic = false;

            if (Match(TK_ELLIPSIS, true))
            {
                isVariadic = true;
                foundVariadic = true;
            }

            // check for default assignment
            if (MatchOperator("=", true))
            {
                defaultParam = ParseExpression(true);
            }

            parameters.PushBack(SharedPtr<AstParameter>(new AstParameter(
                token.GetValue(),
                typeSpec,
                defaultParam,
                isVariadic,
                flags,
                token.GetLocation())));

            if (!Match(TK_COMMA, true))
            {
                keepReading = false;
            }
        }
        else
        {
            keepReading = false;
        }
    }

    return parameters;
}

SharedPtr<AstClass> Parser::ParseClassDefinition()
{
    EnumFlags<AstClassFlags> classFlags = CLASS_FLAG_NONE;

    if (Token externToken = MatchKeyword(Keyword_extern, true))
    {
        classFlags |= CLASS_FLAG_EXTERN;
    }
    else if (Token proxyToken = MatchKeyword(Keyword_proxy, true))
    {
        classFlags |= CLASS_FLAG_IS_PROXY;
    }

    Token descToken = Token::EMPTY;

    if ((descToken = MatchKeyword(Keyword_struct, true)))
    {
        if (classFlags & CLASS_FLAG_IS_PROXY)
        {
            m_compilationUnit->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_struct_cannot_be_proxy,
                descToken.GetLocation()));
        }
        else
        {
            classFlags |= CLASS_FLAG_IS_STRUCT;
        }
    }
    else
    {
        descToken = ExpectKeyword(Keyword_class, true);
    }

    if (descToken)
    {
        if (Token identifier = ExpectIdentifier(false, true))
        {
            return ParseClass(false, false, classFlags, identifier.GetValue());
        }
    }

    return nullptr;
}

SharedPtr<AstClass> Parser::ParseClass(
    bool requireKeyword,
    bool allowIdentifier,
    EnumFlags<AstClassFlags> classFlags,
    String typeName)
{
    const SourceLocation location = CurrentLocation();

    if (requireKeyword)
    {
        if (MatchKeyword(Keyword_struct, true))
        {
            classFlags |= CLASS_FLAG_IS_STRUCT;
        }
        else if (!ExpectKeyword(Keyword_class, true))
        {
            return nullptr;
        }
    }

    if (allowIdentifier)
    {
        if (Token ident = Match(TK_IDENT, true))
        {
            typeName = ident.GetValue();
        }
    }

    SharedPtr<AstTypeSpecifier> baseSpec;

    if (Match(TK_COLON, true))
    {
        baseSpec = ParseTypeSpecifier();
    }

    // for hoisting, so functions can use later declared members
    Array<SharedPtr<AstVariableDeclaration>> memberFunctions;
    Array<SharedPtr<AstVariableDeclaration>> memberVariables;

    Array<SharedPtr<AstVariableDeclaration>> staticFunctions;
    Array<SharedPtr<AstVariableDeclaration>> staticVariables;

    String currentAccessSpecifier = Keyword::ToString(Keyword_private).Get();

    SkipStatementTerminators();

    const bool useBraces = !Match(TK_OPEN_BRACE, true).Empty();

    while (useBraces ? !Match(TK_CLOSE_BRACE, true) : !MatchKeyword(Keyword_end, true))
    {
        const SourceLocation location = CurrentLocation();
        Token specifierToken = Token::EMPTY;

        if ((specifierToken = MatchKeyword(Keyword_public, true))
            || (specifierToken = MatchKeyword(Keyword_private, true))
            || (specifierToken = MatchKeyword(Keyword_protected, true)))
        {

            // read ':'
            if (Expect(TK_COLON, true))
            {
                currentAccessSpecifier = specifierToken.GetValue();
            }
        }

        EnumFlags<IdentifierFlags> flags = IdentifierFlags::NONE;

        // read ident
        bool isStatic = false,
             isFunction = false,
             isVariable = false;

        if (MatchKeyword(Keyword_static, true))
        {
            isStatic = true;
        }

        // place rollback position here because ParseVariableDeclaration()
        // will handle everything. put keywords that ParseVariableDeclaration()
        // does /not/ handle, above.
        const size_t positionBefore = m_tokenStream->GetPosition();

        if (MatchKeyword(Keyword_ref, true))
        {
            isVariable = true;

            flags |= IdentifierFlags::REF;
        }

        if (MatchKeyword(Keyword_const, true))
        {
            isVariable = true;

            flags |= IdentifierFlags::CONSTANT;
        }

        if (MatchKeyword(Keyword_func, true))
        {
            isFunction = true;
        }

        //! Match(TK_STRING, false) || ExpectIdentifier(true, false);

        if (!MatchIdentifier(true, false) && !Match(TK_STRING, false))
        {
            // error; unexpected token
            m_compilationUnit->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_unexpected_token,
                m_tokenStream->Peek().GetLocation(),
                m_tokenStream->Peek().GetValue()));

            if (m_tokenStream->HasNext())
            {
                m_tokenStream->Next();
            }

            continue;
        }

        // read the identifier token
        Token identifier = Match(TK_STRING, false)
            ? m_tokenStream->Next()
            : ExpectIdentifier(true, true);

        // read generic params after identifier

        SharedPtr<AstExpression> assignment;

        if (currentAccessSpecifier == Keyword::ToString(Keyword_public).Get())
        {
            flags |= IdentifierFlags::ACCESS_PUBLIC;
        }
        else if (currentAccessSpecifier == Keyword::ToString(Keyword_private).Get())
        {
            flags |= IdentifierFlags::ACCESS_PRIVATE;
        }
        else if (currentAccessSpecifier == Keyword::ToString(Keyword_protected).Get())
        {
            flags |= IdentifierFlags::ACCESS_PROTECTED;
        }

        if (!isVariable && (isFunction || Match(TK_OPEN_PARENTH)))
        { // it is a member function
            Array<SharedPtr<AstParameter>> params;

#if HYP_SCRIPT_AUTO_SELF_INSERTION
            params.Reserve(1); // reserve at least 1 for 'self' parameter

            if (!isStatic)
            {
                SymbolType* selfTypePlaceholder = SymbolType::Placeholder("SelfType");
                selfTypePlaceholder->Register(m_compilationUnit);

                SharedPtr<AstTypeSpecifier> selfTypeSpec(new AstTypeSpecifier(
                    SharedPtr<AstTypeRef>(new AstTypeRef(selfTypePlaceholder, location)),
                    location));

                params.PushBack(SharedPtr<AstParameter>(new AstParameter(
                    "self",
                    selfTypeSpec,
                    nullptr,
                    false, /* variadic */
                    IdentifierFlags::CONSTANT,
                    location)));
            }
#endif

            if (Match(TK_OPEN_PARENTH, true))
            {
                params.Concat(ParseFunctionParameters());
                Expect(TK_CLOSE_PARENTH, true);
            }

            assignment = ParseFunctionExpression(
                false,                             /* requireKeyword */
                !(classFlags & CLASS_FLAG_EXTERN), /* parseBody - extern classes have no body in their methods */
                params);

            if (assignment == nullptr)
            {
                return nullptr;
            }

            SharedPtr<AstVariableDeclaration> member(new AstVariableDeclaration(
                identifier.GetValue(),
                nullptr, // type specifier
                assignment,
                flags,
                location));

            if (isStatic || (classFlags[CLASS_FLAG_IS_PROXY])) // <--- all methods for proxy classes are static
            {
                member->ApplyIdentifierFlags(IdentifierFlags::STATIC_MEMBER);

                staticFunctions.PushBack(std::move(member));
            }
            else
            {
                member->ApplyIdentifierFlags(IdentifierFlags::MEMBER);

                memberFunctions.PushBack(std::move(member));
            }
        }
        else
        {
            // rollback
            m_tokenStream->SetPosition(positionBefore);

            if (SharedPtr<AstVariableDeclaration> member = ParseVariableDeclaration(
                    true, // allow keyword names
                    true, // allow quoted names
                    flags))
            {
                if (isStatic)
                {
                    member->ApplyIdentifierFlags(IdentifierFlags::STATIC_MEMBER);

                    staticVariables.PushBack(member);
                }
                else
                {
                    member->ApplyIdentifierFlags(IdentifierFlags::MEMBER);

                    memberVariables.PushBack(member);
                }
            }
            else
            {
                break;
            }

            if (classFlags[CLASS_FLAG_IS_PROXY])
            {
                m_compilationUnit->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_proxy_class_may_only_contain_methods,
                    m_tokenStream->Peek().GetLocation()));
            }
        }

        ExpectEndOfStmt();
        SkipStatementTerminators();
    }

    Array<SharedPtr<AstVariableDeclaration>> allFunctions;
    allFunctions.Reserve(staticFunctions.Size() + memberFunctions.Size());
    allFunctions.Concat(staticFunctions);
    allFunctions.Concat(memberFunctions);

    return SharedPtr<AstClass>(new AstClass(
        typeName,
        baseSpec,
        memberVariables,
        allFunctions,
        staticVariables,
        classFlags,
        location));
}

SharedPtr<AstStatement> Parser::ParseEnumDefinition()
{
    String enumName;
    EnumFlags<AstClassFlags> classFlags = CLASS_FLAG_IS_ENUM;
    const SourceLocation location = CurrentLocation();

    if (Token externToken = MatchKeyword(Keyword_extern, true))
    {
        classFlags |= CLASS_FLAG_EXTERN;
    }

    if (!ExpectKeyword(Keyword_enum, true))
    {
        return nullptr;
    }

    if (Token ident = Match(TK_IDENT, true))
    {
        enumName = ident.GetValue();
    }

    SharedPtr<AstTypeSpecifier> underlyingType;

    if (Match(TK_COLON, true))
    {
        // underlying type
        underlyingType = ParseTypeSpecifier();
    }

    SkipStatementTerminators();

    Array<SharedPtr<AstVariableDeclaration>> entries;

    const bool useBraces = !Match(TK_OPEN_BRACE, true).Empty();

    while (useBraces ? !Match(TK_CLOSE_BRACE, true) : !MatchKeyword(Keyword_end, true))
    {
        if (const Token ident = Expect(TK_IDENT, true))
        {
            SymbolType* selfTypePlaceholder = SymbolType::Placeholder("SelfType");
            selfTypePlaceholder->Register(m_compilationUnit);

            SharedPtr<AstTypeSpecifier> typeSpec(new AstTypeSpecifier(
                SharedPtr<AstTypeRef>(new AstTypeRef(selfTypePlaceholder, ident.GetLocation())),
                ident.GetLocation()));

            SharedPtr<AstExpression> assignment;

            if (const Token op = MatchOperator("=", true))
            {
                assignment = ParseExpression(/* overrideCommas */ true);
            }

            SharedPtr<AstVariableDeclaration> entry(new AstVariableDeclaration(
                ident.GetValue(),
                typeSpec,
                assignment,
                IdentifierFlags::STATIC_MEMBER
                    | IdentifierFlags::ENUM_MEMBER
                    | IdentifierFlags::CONSTANT
                    | IdentifierFlags::ACCESS_PUBLIC,
                ident.GetLocation()));

            entries.PushBack(std::move(entry));
        }
        else
        {
            break;
        }

        while (Match(TK_NEWLINE, true))
            ;

        if (useBraces ? !Match(TK_CLOSE_BRACE, false) : !MatchKeyword(Keyword_end, false))
        {
            Expect(TK_COMMA, true);
        }
    }

    return SharedPtr<AstClass>(new AstClass(
        enumName,
        underlyingType,
        {}, // no member variables
        {}, // no member functions
        entries,
        classFlags,
        location));
}

SharedPtr<AstImport> Parser::ParseImport()
{
    if (ExpectKeyword(Keyword_import))
    {
        if (MatchAhead(TK_STRING, 1))
        {
            return ParseFileImport();
        }
        else if (MatchAhead(TK_IDENT, 1))
        {
            return ParseModuleImport();
        }
    }

    return nullptr;
}

SharedPtr<AstExportStatement> Parser::ParseExportStatement()
{
    if (Token exportToken = ExpectKeyword(Keyword_export, true))
    {
        if (auto stmt = ParseStatement())
        {
            return SharedPtr<AstExportStatement>(new AstExportStatement(
                stmt,
                exportToken.GetLocation()));
        }
    }

    return nullptr;
}

SharedPtr<AstFileImport> Parser::ParseFileImport()
{
    if (Token token = ExpectKeyword(Keyword_import, true))
    {
        if (Token file = Expect(TK_STRING, true))
        {
            SharedPtr<AstFileImport> result(new AstFileImport(
                file.GetValue(),
                token.GetLocation()));

            return result;
        }
    }

    return nullptr;
}

SharedPtr<AstModuleImportPart> Parser::ParseModuleImportPart(bool allowBraces)
{
    static const String s_wildcardImportToken = "*";

    const SourceLocation location = CurrentLocation();

    Array<SharedPtr<AstModuleImportPart>> parts;

    Token ident = Match(TK_IDENT, true);

    if (!ident)
    {
        if (Token wildcard = MatchOperator(s_wildcardImportToken, true))
        {
            ident = wildcard;
        }
    }

    if (ident)
    {
        if (Match(TK_DOT, true))
        {
            if (ident.GetValue() == s_wildcardImportToken)
            {
                m_compilationUnit->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_unexpected_token,
                    ident.GetLocation(),
                    ident.GetValue()));

                return nullptr;
            }

            if (Match(TK_OPEN_BRACE, true))
            {
                while (!Match(TK_CLOSE_BRACE, false))
                {
                    SharedPtr<AstModuleImportPart> part = ParseModuleImportPart(false);

                    if (part == nullptr)
                    {
                        return nullptr;
                    }

                    parts.PushBack(part);

                    if (!Match(TK_COMMA, true))
                    {
                        break;
                    }
                }

                Expect(TK_CLOSE_BRACE, true);
            }
            else
            {
                // match next
                SharedPtr<AstModuleImportPart> part = ParseModuleImportPart(true);

                if (part == nullptr)
                {
                    return nullptr;
                }

                parts.PushBack(part);
            }
        }

        return SharedPtr<AstModuleImportPart>(new AstModuleImportPart(
            ident.GetValue(),
            parts,
            location));
    }

    return nullptr;
}

SharedPtr<AstModuleImport> Parser::ParseModuleImport()
{
    if (Token token = ExpectKeyword(Keyword_import, true))
    {
        Array<SharedPtr<AstModuleImportPart>> parts;

        if (auto part = ParseModuleImportPart(false))
        {
            parts.PushBack(part);

            return SharedPtr<AstModuleImport>(new AstModuleImport(
                parts,
                token.GetLocation()));
        }
    }

    return nullptr;
}

SharedPtr<AstReturnStatement> Parser::ParseReturnStatement()
{
    const SourceLocation location = CurrentLocation();

    if (Token token = ExpectKeyword(Keyword_return, true))
    {
        SharedPtr<AstExpression> expr;

        if (!Match(TK_SEMICOLON, true))
        {
            expr = ParseExpression();
        }

        return SharedPtr<AstReturnStatement>(new AstReturnStatement(
            expr,
            location));
    }

    return nullptr;
}

} // namespace Hyperion
