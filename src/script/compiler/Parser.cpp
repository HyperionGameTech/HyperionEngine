#include <script/compiler/Parser.hpp>
#include <script/compiler/Configuration.hpp>
#include <script/compiler/Keywords.hpp>
#include <script/compiler/type-system/SymbolType.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <util/UTF8.hpp>
#include <core/utilities/StringUtil.hpp>

namespace hyperion {

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

        RC<AstModuleDeclaration> moduleAst(new AstModuleDeclaration(
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
            if (RC<AstStatement> stmt = ParseStatement(true))
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
            if (RC<AstStatement> stmt = ParseStatement(true))
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

RC<AstStatement> Parser::ParseStatement(
    bool topLevel,
    bool readTerminators)
{
    RC<AstStatement> res;

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
            || (MatchKeyword(Keyword_extern, false) && !MatchKeywordAhead(Keyword_func, 1) && !MatchKeywordAhead(Keyword_class, 1)))
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
        else if (MatchKeyword(Keyword_class, false)
            || (MatchKeyword(Keyword_proxy, false) && MatchKeywordAhead(Keyword_class, 1))
            || (MatchKeyword(Keyword_extern, false) && MatchKeywordAhead(Keyword_class, 1)))
        {
            res = ParseClassDefinition();
        }
        else if (MatchKeyword(Keyword_enum, false))
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
    else if (Match(TK_DIRECTIVE, false))
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

RC<AstModuleDeclaration> Parser::ParseModuleDeclaration()
{
    if (Token moduleDecl = ExpectKeyword(Keyword_module, true))
    {
        if (Token moduleName = Expect(TK_IDENT, true))
        {
            // expect open brace
            if (Expect(TK_OPEN_BRACE, true))
            {
                RC<AstModuleDeclaration> moduleAst(new AstModuleDeclaration(
                    moduleName.GetValue(),
                    moduleDecl.GetLocation()));

                // build up the module declaration with statements
                while (m_tokenStream->HasNext() && !Match(TK_CLOSE_BRACE, false))
                {
                    // skip statement terminator tokens
                    if (!Match(TK_SEMICOLON, true) && !Match(TK_NEWLINE, true))
                    {

                        // parse at top level, to allow for nested modules
                        if (RC<AstStatement> stmt = ParseStatement(true))
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

RC<AstDirective> Parser::ParseDirective()
{
    if (Token token = Expect(TK_DIRECTIVE, true))
    {
        // the arguments will be held in an array expression
        Array<String> args;

        while (m_tokenStream->HasNext() && !(Match(TK_SEMICOLON, true) || Match(TK_NEWLINE, true)))
        {
            Token token = m_tokenStream->Peek();

            args.PushBack(token.GetValue());
            m_tokenStream->Next();
        }

        return RC<AstDirective>(new AstDirective(
            token.GetValue(),
            args,
            token.GetLocation()));
    }

    return nullptr;
}

RC<AstExpression> Parser::ParseTerm(
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

    RC<AstExpression> expr;

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

RC<AstExpression> Parser::ParseParentheses()
{
    SourceLocation location = CurrentLocation();
    RC<AstExpression> expr;
    const SizeType beforePos = m_tokenStream->GetPosition();

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

            Array<RC<AstParameter>> params;

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
                const SizeType before = m_tokenStream->GetPosition();
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

                Array<RC<AstParameter>> params;

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

                    Array<RC<AstParameter>> params;

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

RC<AstTemplateInstantiation> Parser::ParseTemplateInstantiation(RC<AstExpression> expr)
{
    if (!expr)
    {
        // not type spec
        return nullptr;
    }

    SourceLocation location = CurrentLocation();
    const SizeType beforePos = m_tokenStream->GetPosition();

    Array<RC<AstTypeSpecifier>> args;

    auto parseFunctionReturnType = [&]() -> RC<AstTypeSpecifier>
    {
        // right arrow for function return type is part of the generic args
        if (Match(TK_RIGHT_ARROW, true))
        {
            // parse return type, add as first argument
            if (RC<AstTypeSpecifier> returnType = ParseTypeSpecifier())
            {
                return returnType;
            }
        }

        return nullptr;
    };

    if (Token token = ExpectOperator("<", true))
    {
        bool breakout = false;

        if (MatchOperator(">", true))
        {
            return RC<AstTemplateInstantiation>(new AstTemplateInstantiation(
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

            if (RC<AstTypeSpecifier> arg = ParseTypeSpecifier())
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

        if (!breakout && MatchOperator(">", true))
        {
            return RC<AstTemplateInstantiation>(new AstTemplateInstantiation(
                expr,
                args,
                parseFunctionReturnType(),
                token.GetLocation()));
        }

        // no closing bracket found, revert to start.
        m_tokenStream->SetPosition(beforePos);
    }

    return nullptr;
}

RC<AstConstant> Parser::ParseIntegerLiteral()
{
    if (Token token = Expect(TK_INTEGER, true))
    {
        std::istringstream ss(token.GetValue().Data());

        if (token.GetFlags()[0] == '\0' || token.GetFlags()[0] == 'i')
        {
            hyperion::int64 value;
            ss >> value;

            return RC<AstInteger>(new AstInteger(
                value,
                CBS_32, // @TODO: determine size from value
                token.GetLocation()));
        }
        else if (token.GetFlags()[0] == 'u')
        {
            hyperion::uint64 value;
            ss >> value;

            return RC<AstUnsignedInteger>(new AstUnsignedInteger(
                value,
                CBS_32, // @TODO: determine size from value
                token.GetLocation()));
        }
        else if (token.GetFlags()[0] == 'f')
        {
            double value;
            ss >> value;

            return RC<AstFloat>(new AstFloat(
                value,
                CBS_32, // @TODO: determine size from value
                token.GetLocation()));
        }
        else
        {
            m_compilationUnit->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_illegal_expression,
                token.GetLocation()));
        }
    }

    return nullptr;
}

RC<AstFloat> Parser::ParseFloatLiteral()
{
    if (Token token = Expect(TK_FLOAT, true))
    {
        float value = std::atof(token.GetValue().Data());

        return RC<AstFloat>(new AstFloat(
            value,
            CBS_32, // @TODO: determine size from value
            token.GetLocation()));
    }

    return nullptr;
}

RC<AstString> Parser::ParseStringLiteral()
{
    if (Token token = Expect(TK_STRING, true))
    {
        return RC<AstString>(new AstString(
            token.GetValue(),
            token.GetLocation()));
    }

    return nullptr;
}

RC<AstIdentifier> Parser::ParseIdentifier(bool allowKeyword)
{
    if (Token token = ExpectIdentifier(allowKeyword, false))
    {
        // read identifier token
        if (m_tokenStream->HasNext())
        {
            m_tokenStream->Next();
        }

        // return variable
        return RC<AstVariable>(new AstVariable(
            token.GetValue(),
            token.GetLocation()));
    }

    return nullptr;
}

RC<AstArgument> Parser::ParseArgument(RC<AstExpression> expr)
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
        return RC<AstArgument>(new AstArgument(
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

RC<AstArgumentList> Parser::ParseArguments(bool requireParentheses)
{
    const SourceLocation location = CurrentLocation();

    Array<RC<AstArgument>> args;

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

    return RC<AstArgumentList>(new AstArgumentList(
        args,
        location));
}

RC<AstCallExpression> Parser::ParseCallExpression(RC<AstExpression> target, bool requireParentheses)
{
    if (target != nullptr)
    {
        if (auto args = ParseArguments(requireParentheses))
        {
            return RC<AstCallExpression>(new AstCallExpression(
                target,
                args->GetArguments(),
                true, // allow 'self' to be inserted
                target->GetLocation()));
        }
    }

    return nullptr;
}

RC<AstModuleAccess> Parser::ParseModuleAccess()
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
        RC<AstExpression> expr;

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
            return RC<AstModuleAccess>(new AstModuleAccess(
                globalModuleAccess
                    ? Config::globalModuleName
                    : token.GetValue(),
                expr,
                location));
        }
    }

    return nullptr;
}

RC<AstExpression> Parser::ParseMemberExpression(RC<AstExpression> target)
{
    Expect(TK_DOT, true);

    // allow quoted strings as data member names
    Token ident = Match(TK_STRING, false)
        ? m_tokenStream->Next()
        : ExpectIdentifier(true, true);

    RC<AstExpression> expr;

    if (ident)
    {
        expr = RC<AstMember>(new AstMember(
            ident.GetValue(),
            target,
            ident.GetLocation()));
    }

    return expr;
}

RC<AstArrayAccess> Parser::ParseArrayAccess(
    RC<AstExpression> target,
    bool overrideCommas,
    bool overrideFatArrows,
    bool overrideAngleBrackets,
    bool overrideSquareBrackets,
    bool overrideParentheses,
    bool overrideQuestionMark)
{
    if (Token token = Expect(TK_OPEN_BRACKET, true))
    {
        RC<AstExpression> expr;
        // RC<AstExpression> rhs;

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
            return RC<AstArrayAccess>(new AstArrayAccess(
                target,
                expr,
                true, // allow operator overloading for []
                token.GetLocation()));
        }
    }

    return nullptr;
}

RC<AstHasExpression> Parser::ParseHasExpression(RC<AstExpression> target)
{
    if (Token token = ExpectKeyword(Keyword_has, true))
    {
        if (Token field = Expect(TK_STRING, true))
        {
            return RC<AstHasExpression>(new AstHasExpression(
                target,
                field.GetValue(),
                target->GetLocation()));
        }
    }

    return nullptr;
}

RC<AstIsExpression> Parser::ParseIsExpression(RC<AstExpression> target)
{
    if (Token token = ExpectKeyword(Keyword_is, true))
    {
        if (auto typeExpression = ParseTypeSpecifier())
        {
            return RC<AstIsExpression>(new AstIsExpression(
                target,
                typeExpression,
                target->GetLocation()));
        }
    }

    return nullptr;
}

RC<AstAsExpression> Parser::ParseAsExpression(RC<AstExpression> target)
{
    if (Token token = ExpectKeyword(Keyword_as, true))
    {
        if (auto typeExpression = ParseTypeSpecifier())
        {
            return RC<AstAsExpression>(new AstAsExpression(
                target,
                typeExpression,
                target->GetLocation()));
        }
    }

    return nullptr;
}

RC<AstNewExpression> Parser::ParseNewExpression()
{
    if (Token token = ExpectKeyword(Keyword_new, true))
    {
        if (auto proto = ParseTypeSpecifier())
        {
            RC<AstArgumentList> argList;

            if (Match(TK_OPEN_PARENTH, false))
            {
                // parse args
                argList = ParseArguments();
            }

            return RC<AstNewExpression>(new AstNewExpression(
                proto,
                argList,
                true, // enable construct call
                token.GetLocation()));
        }
    }

    return nullptr;
}

RC<AstTrue> Parser::ParseTrue()
{
    if (Token token = ExpectKeyword(Keyword_true, true))
    {
        return RC<AstTrue>(new AstTrue(
            token.GetLocation()));
    }

    return nullptr;
}

RC<AstFalse> Parser::ParseFalse()
{
    if (Token token = ExpectKeyword(Keyword_false, true))
    {
        return RC<AstFalse>(new AstFalse(
            token.GetLocation()));
    }

    return nullptr;
}

RC<AstNil> Parser::ParseNil()
{
    if (Token token = ExpectKeyword(Keyword_null, true))
    {
        return RC<AstNil>(new AstNil(
            token.GetLocation()));
    }

    return nullptr;
}

RC<AstBlock> Parser::ParseBlock(bool requireBraces, bool skipEnd)
{
    SourceLocation location = CurrentLocation();

    if (requireBraces)
    {
        if (!Expect(TK_OPEN_BRACE, true))
        {
            return nullptr;
        }
    }

    RC<AstBlock> block(new AstBlock(location));

    while (requireBraces ? !Match(TK_CLOSE_BRACE, false) : !MatchKeyword(Keyword_end, false))
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

RC<AstIfStatement> Parser::ParseIfStatement()
{
    if (Token token = ExpectKeyword(Keyword_if, true))
    {
        bool hasParentheses = false;

        if (Match(TK_OPEN_PARENTH, true))
        {
            hasParentheses = true;
        }

        RC<AstExpression> conditional;
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

        RC<AstBlock> block;
        if (!(block = ParseBlock(true, true)))
        {
            return nullptr;
        }

        RC<AstBlock> elseBlock = nullptr;
        // parse else statement if the "else" keyword is found
        if (Token elseToken = MatchKeyword(Keyword_else, true))
        {
            // check for "if" keyword for else-if
            if (MatchKeyword(Keyword_if, false))
            {
                elseBlock = RC<AstBlock>(new AstBlock(
                    elseToken.GetLocation()));

                if (auto elseIfBlock = ParseIfStatement())
                {
                    elseBlock->AddChild(elseIfBlock);
                }
            }
            else
            {
                // parse block after "else keyword
                if (!(elseBlock = ParseBlock(true, true)))
                {
                    return nullptr;
                }
            }
        }

        // // expect "end" keyword
        // if (!ExpectKeyword(Keyword_end, true)) {
        //     return nullptr;
        // }

        return RC<AstIfStatement>(new AstIfStatement(
            conditional,
            block,
            elseBlock,
            token.GetLocation()));
    }

    return nullptr;
}

RC<AstWhileLoop> Parser::ParseWhileLoop()
{
    if (Token token = ExpectKeyword(Keyword_while, true))
    {
        if (!Expect(TK_OPEN_PARENTH, true))
        {
            return nullptr;
        }

        RC<AstExpression> conditional;
        if (!(conditional = ParseExpression()))
        {
            return nullptr;
        }

        if (!Expect(TK_CLOSE_PARENTH, true))
        {
            return nullptr;
        }

        RC<AstBlock> block;
        if (!(block = ParseBlock(true)))
        {
            return nullptr;
        }

        return RC<AstWhileLoop>(new AstWhileLoop(
            conditional,
            block,
            token.GetLocation()));
    }

    return nullptr;
}

RC<AstStatement> Parser::ParseForLoop()
{
    if (Token token = ExpectKeyword(Keyword_for, true))
    {
        if (!Expect(TK_OPEN_PARENTH, true))
        {
            return nullptr;
        }

        RC<AstStatement> declPart;

        if (!Match(TK_SEMICOLON))
        {
            if (!(declPart = ParseStatement(false, false)))
            {
                return nullptr;
            }
        }

        if (!Expect(TK_SEMICOLON, true))
        {
            return nullptr;
        }

        RC<AstExpression> conditionPart;

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

        RC<AstExpression> incrementPart;

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

        RC<AstBlock> block;
        if (!(block = ParseBlock(true)))
        {
            return nullptr;
        }

        return RC<AstForLoop>(new AstForLoop(
            declPart,
            conditionPart,
            incrementPart,
            block,
            token.GetLocation()));
    }

    return nullptr;
}

RC<AstStatement> Parser::ParseBreakStatement()
{
    if (Token token = ExpectKeyword(Keyword_break, true))
    {
        return RC<AstBreakStatement>(new AstBreakStatement(
            token.GetLocation()));
    }

    return nullptr;
}

RC<AstStatement> Parser::ParseContinueStatement()
{
    if (Token token = ExpectKeyword(Keyword_continue, true))
    {
        return RC<AstContinueStatement>(new AstContinueStatement(
            token.GetLocation()));
    }

    return nullptr;
}

RC<AstTryCatch> Parser::ParseTryCatchStatement()
{
    if (Token token = ExpectKeyword(Keyword_try, true))
    {
        RC<AstBlock> tryBlock = ParseBlock(true, true);
        RC<AstBlock> catchBlock;

        if (ExpectKeyword(Keyword_catch, true))
        {
            // TODO: Add exception argument
            catchBlock = ParseBlock(true);
        }
        else
        {
            // // No catch keyword, expect 'end'
            // if (!ExpectKeyword(Keyword_end, true)) {
            //     return nullptr;
            // }
        }

        if (tryBlock != nullptr && catchBlock != nullptr)
        {
            return RC<AstTryCatch>(new AstTryCatch(
                tryBlock,
                catchBlock,
                token.GetLocation()));
        }
    }

    return nullptr;
}

RC<AstThrowExpression> Parser::ParseThrowExpression()
{
    if (Token token = ExpectKeyword(Keyword_throw, true))
    {
        if (auto expr = ParseExpression())
        {
            return RC<AstThrowExpression>(new AstThrowExpression(
                expr,
                token.GetLocation()));
        }
    }

    return nullptr;
}

RC<AstExpression> Parser::ParseBinaryExpression(
    int exprPrec,
    RC<AstExpression> left)
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

        if (RC<AstExpression> right = ParseTerm())
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

            left = RC<AstBinaryExpression>(new AstBinaryExpression(
                left,
                right,
                op,
                token.GetLocation()));
        }
    }

    return nullptr;
}

RC<AstExpression> Parser::ParseUnaryExpressionPrefix()
{
    // read the operator token
    if (Token token = Expect(TK_OPERATOR, true))
    {
        const Operator* op = nullptr;
        if (Operator::IsUnaryOperator(token.GetValue(), /*OperatorType::PREFIX,*/ op))
        {
            if (auto term = ParseTerm())
            {
                return RC<AstUnaryExpression>(new AstUnaryExpression(
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

RC<AstExpression> Parser::ParseUnaryExpressionPostfix(const RC<AstExpression>& expr)
{
    // read the operator token
    if (Token token = Expect(TK_OPERATOR, true))
    {
        const Operator* op = nullptr;
        if (Operator::IsUnaryOperator(token.GetValue(), /*OperatorType::POSTFIX,*/ op))
        {
            return RC<AstUnaryExpression>(new AstUnaryExpression(
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

RC<AstExpression> Parser::ParseTernaryExpression(const RC<AstExpression>& conditional)
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

        return RC<AstTernaryExpression>(new AstTernaryExpression(
            conditional,
            trueExpr,
            falseExpr,
            conditional->GetLocation()));
    }

    return nullptr;
}

RC<AstExpression> Parser::ParseExpression(
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
            if (MatchOperator(">", false) && m_templateArgumentDepth > 0)
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

RC<AstTypeSpecifier> Parser::ParseTypeSpecifier()
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
        //     term = RC<AstTemplateInstantiation>(new AstTemplateInstantiation(
        //         RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
        //             term,
        //             term->GetLocation())),
        //         { RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
        //             RC<AstUnsignedInteger>(new AstUnsignedInteger(
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
            if (RC<AstTemplateInstantiation> templateInstantiation = ParseTemplateInstantiation(term))
            {
                return templateInstantiation;
            }
        }

        return RC<AstTypeSpecifier>(new AstTypeSpecifier(term, location));
    }

    return nullptr;
}

RC<AstExpression> Parser::ParseAssignment()
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

RC<AstVariableDeclaration> Parser::ParseVariableDeclaration(
    bool allowKeywordNames,
    bool allowQuotedNames,
    EnumFlags<IdentifierFlags> flags)
{
    const SourceLocation location = CurrentLocation();

    static const HashMap<Keywords, IdentifierFlags> s_prefixKeywordMap = {
        { Keyword_extern, IdentifierFlags::EXTERN },
        { Keyword_const, IdentifierFlags::CONST },
        { Keyword_ref, IdentifierFlags::REF }
    };

    HashSet<Keywords> usedSpecifiers;

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
        RC<AstTypeSpecifier> typeSpec;
        RC<AstExpression> assignment;

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

        return RC<AstVariableDeclaration>(new AstVariableDeclaration(
            identifier.GetValue(),
            typeSpec,
            assignment,
            flags,
            location));
    }

    return nullptr;
}

RC<AstStatement> Parser::ParseFunctionDefinition(bool requireKeyword)
{
    const SourceLocation location = CurrentLocation();

    EnumFlags<IdentifierFlags> flags = IdentifierFlags::CONST
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
        RC<AstExpression> assignment;
        Array<RC<AstParameter>> params;

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

        return RC<AstVariableDeclaration>(new AstVariableDeclaration(
            identifier.GetValue(),
            nullptr, // type specifier
            assignment,
            flags,
            location));
    }

    return nullptr;
}

RC<AstFunctionExpression> Parser::ParseFunctionExpression(
    bool requireKeyword,
    bool parseBody,
    Array<RC<AstParameter>> params)
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

        RC<AstTypeSpecifier> typeSpec;

        if (Match(TK_RIGHT_ARROW, true))
        {
            // read return type for functions
            typeSpec = ParseTypeSpecifier();
        }

        if (!parseBody)
        {
            return RC<AstFunctionExpression>(new AstFunctionExpression(
                params,
                typeSpec,
                nullptr, // no body
                location));
        }

        RC<AstBlock> block;

        if (Match(TK_FAT_ARROW, true))
        {
            RC<AstReturnStatement> returnStatement(new AstReturnStatement(
                ParseExpression(),
                location));

            block.Reset(new AstBlock(
                { std::move(returnStatement) },
                location));
        }
        else
        {
            SkipStatementTerminators();

            // bool useBraces = !Match(TK_OPEN_BRACE, false).Empty();

            block = ParseBlock(true);
        }

        if (block != nullptr)
        {
            return RC<AstFunctionExpression>(new AstFunctionExpression(
                params,
                typeSpec,
                block,
                location));
        }
    }

    return nullptr;
}

RC<AstArrayExpression> Parser::ParseArrayExpression()
{
    if (Token token = Expect(TK_OPEN_BRACKET, true))
    {
        Array<RC<AstExpression>> members;

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

        return RC<AstArrayExpression>(new AstArrayExpression(
            members,
            token.GetLocation()));
    }

    return nullptr;
}

RC<AstHashMap> Parser::ParseHashMap()
{
    if (Token token = Expect(TK_OPEN_BRACE, true))
    {
        Array<RC<AstExpression>> keys;
        Array<RC<AstExpression>> values;

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

                keys.PushBack(RC<AstString>(new AstString(
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

        return RC<AstHashMap>(new AstHashMap(
            keys,
            values,
            token.GetLocation()));
    }

    return nullptr;
}

RC<AstTypeOfExpression> Parser::ParseTypeOfExpression()
{
    const SourceLocation location = CurrentLocation();

    if (Token token = ExpectKeyword(Keyword_typeof, true))
    {
        SourceLocation exprLocation = CurrentLocation();
        if (auto term = ParseTerm())
        {
            return RC<AstTypeOfExpression>(new AstTypeOfExpression(
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

Array<RC<AstParameter>> Parser::ParseFunctionParameters()
{
    Array<RC<AstParameter>> parameters;

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
            flags |= IdentifierFlags::CONST;
        }

        if (MatchKeyword(Keyword_ref, true))
        {
            flags |= IdentifierFlags::REF;
        }

        if ((token = ExpectIdentifier(true, true)))
        {
            RC<AstTypeSpecifier> typeSpec;
            RC<AstExpression> defaultParam;

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

            parameters.PushBack(RC<AstParameter>(new AstParameter(
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

RC<AstClass> Parser::ParseClassDefinition()
{
    EnumFlags<ClassFlags> classFlags = ClassFlags::CLASS_FLAG_NONE;

    if (Token externToken = MatchKeyword(Keyword_extern, true))
    {
        classFlags |= ClassFlags::CLASS_FLAG_EXTERN;
    }
    else if (Token proxyToken = MatchKeyword(Keyword_proxy, true))
    {
        classFlags |= ClassFlags::CLASS_FLAG_IS_PROXY;
    }

    if (Token token = ExpectKeyword(Keyword_class, true))
    {
        if (Token identifier = ExpectIdentifier(false, true))
        {
            return ParseClass(false, false, classFlags, identifier.GetValue());
        }
    }

    return nullptr;
}

RC<AstClass> Parser::ParseClass(
    bool requireKeyword,
    bool allowIdentifier,
    EnumFlags<ClassFlags> classFlags,
    String typeName)
{
    const SourceLocation location = CurrentLocation();

    if (requireKeyword && !ExpectKeyword(Keyword_class, true))
    {
        return nullptr;
    }

    if (allowIdentifier)
    {
        if (Token ident = Match(TK_IDENT, true))
        {
            typeName = ident.GetValue();
        }
    }

    RC<AstTypeSpecifier> baseSpec;

    if (Match(TK_COLON, true))
    {
        baseSpec = ParseTypeSpecifier();
    }

    // for hoisting, so functions can use later declared members
    Array<RC<AstVariableDeclaration>> memberFunctions;
    Array<RC<AstVariableDeclaration>> memberVariables;

    Array<RC<AstVariableDeclaration>> staticFunctions;
    Array<RC<AstVariableDeclaration>> staticVariables;

    String currentAccessSpecifier = Keyword::ToString(Keyword_private).Get();

    SkipStatementTerminators();

    if (!Expect(TK_OPEN_BRACE, true))
    {
        return nullptr;
    }

    while (!Match(TK_CLOSE_BRACE, true))
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
        const SizeType positionBefore = m_tokenStream->GetPosition();

        if (MatchKeyword(Keyword_ref, true))
        {
            isVariable = true;

            flags |= IdentifierFlags::REF;
        }

        if (MatchKeyword(Keyword_const, true))
        {
            isVariable = true;

            flags |= IdentifierFlags::CONST;
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

        RC<AstExpression> assignment;

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
            Array<RC<AstParameter>> params;

#if HYP_SCRIPT_AUTO_SELF_INSERTION
            params.Reserve(1); // reserve at least 1 for 'self' parameter

            if (!isStatic)
            {
                RC<AstTypeSpecifier> selfTypeSpec(new AstTypeSpecifier(
                    RC<AstTypeRef>(new AstTypeRef(SymbolType::Placeholder("SelfType"), location)),
                    location));

                params.PushBack(RC<AstParameter>(new AstParameter(
                    "self",
                    selfTypeSpec,
                    nullptr,
                    false, /* variadic */
                    IdentifierFlags::CONST,
                    location)));
            }
#endif

            if (Match(TK_OPEN_PARENTH, true))
            {
                params.Concat(ParseFunctionParameters());
                Expect(TK_CLOSE_PARENTH, true);
            }

            assignment = ParseFunctionExpression(
                false,                                         /* requireKeyword */
                !(classFlags & ClassFlags::CLASS_FLAG_EXTERN), /* parseBody - extern classes have no body in their methods */
                params);

            if (assignment == nullptr)
            {
                return nullptr;
            }

            RC<AstVariableDeclaration> member(new AstVariableDeclaration(
                identifier.GetValue(),
                nullptr, // type specifier
                assignment,
                flags,
                location));

            if (isStatic || (classFlags[ClassFlags::CLASS_FLAG_IS_PROXY])) // <--- all methods for proxy classes are static
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

            if (RC<AstVariableDeclaration> member = ParseVariableDeclaration(
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

            if (classFlags[ClassFlags::CLASS_FLAG_IS_PROXY])
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

    Array<RC<AstVariableDeclaration>> allStatics;
    allStatics.Reserve(staticVariables.Size() + staticFunctions.Size());
    allStatics.Concat(std::move(staticVariables));
    allStatics.Concat(std::move(staticFunctions));

    return RC<AstClass>(new AstClass(
        typeName,
        baseSpec,
        memberVariables,
        memberFunctions,
        allStatics,
        classFlags,
        location));

    return nullptr;
}

RC<AstStatement> Parser::ParseEnumDefinition()
{
    String enumName;

    const SourceLocation location = CurrentLocation();

    if (!ExpectKeyword(Keyword_enum, true))
    {
        return nullptr;
    }

    if (Token ident = Match(TK_IDENT, true))
    {
        enumName = ident.GetValue();
    }

    RC<AstTypeSpecifier> underlyingType;

    if (Match(TK_COLON, true))
    {
        // underlying type
        underlyingType = ParseTypeSpecifier();
    }

    SkipStatementTerminators();

    Array<RC<AstVariableDeclaration>> entries;

    if (!Expect(TK_OPEN_BRACE, true))
    {
        return nullptr;
    }

    while (!Match(TK_CLOSE_BRACE, true))
    {
        if (const Token ident = Expect(TK_IDENT, true))
        {
            RC<AstTypeSpecifier> typeSpec(new AstTypeSpecifier(
                RC<AstTypeRef>(new AstTypeRef(SymbolType::Placeholder("SelfType"), ident.GetLocation())),
                ident.GetLocation()));

            RC<AstExpression> assignment;

            if (const Token op = MatchOperator("=", true))
            {
                assignment = ParseExpression(/* overrideCommas */ true);
            }

            RC<AstVariableDeclaration> entry(new AstVariableDeclaration(
                ident.GetValue(),
                typeSpec,
                assignment,
                IdentifierFlags::STATIC_MEMBER
                    | IdentifierFlags::ENUM_MEMBER
                    | IdentifierFlags::CONST
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

        if (!Match(TK_CLOSE_BRACE, false))
        {
            Expect(TK_COMMA, true);
        }
    }

    return RC<AstClass>(new AstClass(
        enumName,
        underlyingType,
        {}, // no member variables
        {}, // no member functions
        entries,
        ClassFlags::CLASS_FLAG_IS_ENUM,
        location));
}

RC<AstImport> Parser::ParseImport()
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

RC<AstExportStatement> Parser::ParseExportStatement()
{
    if (Token exportToken = ExpectKeyword(Keyword_export, true))
    {
        if (auto stmt = ParseStatement())
        {
            return RC<AstExportStatement>(new AstExportStatement(
                stmt,
                exportToken.GetLocation()));
        }
    }

    return nullptr;
}

RC<AstFileImport> Parser::ParseFileImport()
{
    if (Token token = ExpectKeyword(Keyword_import, true))
    {
        if (Token file = Expect(TK_STRING, true))
        {
            RC<AstFileImport> result(new AstFileImport(
                file.GetValue(),
                token.GetLocation()));

            return result;
        }
    }

    return nullptr;
}

RC<AstModuleImportPart> Parser::ParseModuleImportPart(bool allowBraces)
{
    const SourceLocation location = CurrentLocation();

    Array<RC<AstModuleImportPart>> parts;

    if (Token ident = Expect(TK_IDENT, true))
    {
        if (Match(TK_DOUBLE_COLON, true))
        {
            if (Match(TK_OPEN_BRACE, true))
            {
                while (!Match(TK_CLOSE_BRACE, false))
                {
                    RC<AstModuleImportPart> part = ParseModuleImportPart(false);

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
                RC<AstModuleImportPart> part = ParseModuleImportPart(true);

                if (part == nullptr)
                {
                    return nullptr;
                }

                parts.PushBack(part);
            }
        }

        return RC<AstModuleImportPart>(new AstModuleImportPart(
            ident.GetValue(),
            parts,
            location));
    }

    return nullptr;
}

RC<AstModuleImport> Parser::ParseModuleImport()
{
    if (Token token = ExpectKeyword(Keyword_import, true))
    {
        Array<RC<AstModuleImportPart>> parts;

        if (auto part = ParseModuleImportPart(false))
        {
            parts.PushBack(part);

            return RC<AstModuleImport>(new AstModuleImport(
                parts,
                token.GetLocation()));
        }
    }

    return nullptr;
}

RC<AstReturnStatement> Parser::ParseReturnStatement()
{
    const SourceLocation location = CurrentLocation();

    if (Token token = ExpectKeyword(Keyword_return, true))
    {
        RC<AstExpression> expr;

        if (!Match(TK_SEMICOLON, true))
        {
            expr = ParseExpression();
        }

        return RC<AstReturnStatement>(new AstReturnStatement(
            expr,
            location));
    }

    return nullptr;
}

} // namespace hyperion
