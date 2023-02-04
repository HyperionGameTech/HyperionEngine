#include <script/compiler/Parser.hpp>
#include <script/compiler/Configuration.hpp>
#include <script/compiler/Keywords.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <util/UTF8.hpp>
#include <util/StringUtil.hpp>

#include <memory>
#include <cstdlib>
#include <cstdio>
#include <sstream>
#include <set>
#include <iostream>

namespace hyperion::compiler {

Parser::Parser(AstIterator *ast_iterator,
    TokenStream *token_stream,
    CompilationUnit *compilation_unit)
    : m_ast_iterator(ast_iterator),
      m_token_stream(token_stream),
      m_compilation_unit(compilation_unit)
{
}

Parser::Parser(const Parser &other)
    : m_ast_iterator(other.m_ast_iterator),
      m_token_stream(other.m_token_stream),
      m_compilation_unit(other.m_compilation_unit)
{
}

Token Parser::Match(TokenClass token_class, bool read)
{
    Token peek = m_token_stream->Peek();
    
    if (peek && peek.GetTokenClass() == token_class) {
        if (read && m_token_stream->HasNext()) {
            m_token_stream->Next();
        }
        
        return peek;
    }
    
    return Token::EMPTY;
}

Token Parser::MatchAhead(TokenClass token_class, int n)
{
    Token peek = m_token_stream->Peek(n);
    
    if (peek && peek.GetTokenClass() == token_class) {
        return peek;
    }
    
    return Token::EMPTY;
}

Token Parser::MatchKeyword(Keywords keyword, bool read)
{
    Token peek = m_token_stream->Peek();
    
    if (peek && peek.GetTokenClass() == TK_KEYWORD) {
        std::string str = Keyword::ToString(keyword);
        if (peek.GetValue() == str) {
            if (read && m_token_stream->HasNext()) {
                m_token_stream->Next();
            }
            
            return peek;
        }
    }
    
    return Token::EMPTY;
}

Token Parser::MatchKeywordAhead(Keywords keyword, int n)
{
    Token peek = m_token_stream->Peek(n);
    
    if (peek && peek.GetTokenClass() == TK_KEYWORD) {
        std::string str = Keyword::ToString(keyword);
        if (peek.GetValue() == str) {
            return peek;
        }
    }
    
    return Token::EMPTY;
}

Token Parser::MatchOperator(const std::string &op, bool read)
{
    Token peek = m_token_stream->Peek();
    
    if (peek && peek.GetTokenClass() == TK_OPERATOR) {
        if (peek.GetValue() == op) {
            if (read && m_token_stream->HasNext()) {
                m_token_stream->Next();
            }
            
            return peek;
        }
    }
    
    return Token::EMPTY;
}

Token Parser::MatchOperatorAhead(const std::string &op, int n)
{
    Token peek = m_token_stream->Peek(n);
    
    if (peek && peek.GetTokenClass() == TK_OPERATOR) {
        if (peek.GetValue() == op) {
            return peek;
        }
    }
    
    return Token::EMPTY;
}

Token Parser::Expect(TokenClass token_class, bool read)
{
    Token token = Match(token_class, read);
    
    if (!token) {
        const SourceLocation location = CurrentLocation();

        ErrorMessage error_msg;
        std::string error_str;

        switch (token_class) {
            case TK_IDENT:
                error_msg = Msg_expected_identifier;
                break;
            default:
                error_msg = Msg_expected_token;
                error_str = Token::TokenTypeToString(token_class);
        }

        m_compilation_unit->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            error_msg,
            location,
            error_str
        ));
    }

    return token;
}

Token Parser::ExpectKeyword(Keywords keyword, bool read)
{
    Token token = MatchKeyword(keyword, read);
    
    if (!token) {
        const SourceLocation location = CurrentLocation();

        if (read && m_token_stream->HasNext()) {
            m_token_stream->Next();
        }

        ErrorMessage error_msg;
        std::string error_str;

        switch (keyword) {
            case Keyword_module:
                error_msg = Msg_expected_module;
                break;
            default:
                error_msg = Msg_expected_token;
                error_str = Keyword::ToString(keyword);
        }

        m_compilation_unit->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            error_msg,
            location,
            error_str
        ));
    }

    return token;
}

Token Parser::ExpectOperator(const std::string &op, bool read)
{
    Token token = MatchOperator(op, read);

    if (!token) {
        const SourceLocation location = CurrentLocation();

        if (read && m_token_stream->HasNext()) {
            m_token_stream->Next();
        }

        m_compilation_unit->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_expected_token,
            location,
            op
        ));
    }

    return token;
}

Token Parser::MatchIdentifier(bool allow_keyword, bool read)
{
    Token ident = Match(TK_IDENT, read);

    if (!ident) {
        Token kw = Match(TK_KEYWORD, read);
        
        if (kw) {
            if (allow_keyword) {
                return kw;
            }

            // keyword may not be used as an identifier here.
            m_compilation_unit->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_keyword_cannot_be_used_as_identifier, 
                kw.GetLocation(),
                kw.GetValue()
            ));
        }

        return Token::EMPTY;
    }

    return ident;
}

Token Parser::ExpectIdentifier(bool allow_keyword, bool read)
{
    Token kw = Match(TK_KEYWORD, read);

    if (!kw) {
        // keyword not found, so must be identifier
        return Expect(TK_IDENT, read);
    }

    // handle ident as keyword
    if (allow_keyword) {
        return kw;
    }
    
    m_compilation_unit->GetErrorList().AddError(CompilerError(
        LEVEL_ERROR,
        Msg_keyword_cannot_be_used_as_identifier, 
        kw.GetLocation(),
        kw.GetValue()
    ));

    return Token::EMPTY;
}

bool Parser::ExpectEndOfStmt()
{
    const SourceLocation location = CurrentLocation();

    if (!Match(TK_NEWLINE, true) && !Match(TK_SEMICOLON, true) && !Match(TK_CLOSE_BRACE, false)) {
        m_compilation_unit->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_expected_end_of_statement,
            location
        ));

        return false;
    }

    return true;
}

SourceLocation Parser::CurrentLocation() const
{
    if (m_token_stream->GetSize() != 0 && !m_token_stream->HasNext()) {
        return m_token_stream->Last().GetLocation();
    }

    return m_token_stream->Peek().GetLocation();
}

void Parser::SkipStatementTerminators()
{
    // read past statement terminator tokens
    while (Match(TK_SEMICOLON, true) || Match(TK_NEWLINE, true));
}

void Parser::Parse(bool expect_module_decl)
{
    SkipStatementTerminators();

    if (expect_module_decl) {
        // create a module based upon the filename
        const std::string filepath = m_token_stream->GetInfo().filepath;
        const Array<std::string> split = StringUtil::SplitPath(filepath);

        std::string real_filename = split.Any()
            ? split.Back()
            : filepath;

        real_filename = StringUtil::StripExtension(real_filename);

        RC<AstModuleDeclaration> module_ast(new AstModuleDeclaration(
            real_filename,
            SourceLocation(0, 0, filepath)
        ));

        // build up the module declaration with statements
        while (m_token_stream->HasNext()) {
            // skip statement terminator tokens
            if (Match(TK_SEMICOLON, true) || Match(TK_NEWLINE, true)) {
                continue;
            }

            // parse at top level, to allow for nested modules
            if (auto stmt = ParseStatement(true)) {
                module_ast->AddChild(stmt);
            }
        }

        m_ast_iterator->Push(module_ast);
    } else {
        // build up the module declaration with statements
        while (m_token_stream->HasNext()) {
            // skip statement terminator tokens
            if (Match(TK_SEMICOLON, true) || Match(TK_NEWLINE, true)) {
                return;
            }

            // parse at top level, to allow for nested modules
            if (auto stmt = ParseStatement(true)) {
                m_ast_iterator->Push(stmt);
            }
        }
    }
}

int Parser::OperatorPrecedence(const Operator *&out)
{
    out = nullptr;
    
    Token token = m_token_stream->Peek();

    if (token && token.GetTokenClass() == TK_OPERATOR) {
        if (!Operator::IsBinaryOperator(token.GetValue(), out)) {
            // internal error: operator not defined
            m_compilation_unit->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_internal_error,
                token.GetLocation()
            ));
        }
    }

    if (out != nullptr) {
        return out->GetPrecedence();
    }
    
    return -1;
}

RC<AstStatement> Parser::ParseStatement(
    bool top_level,
    bool read_terminators
)
{
    RC<AstStatement> res;

    if (Match(TK_KEYWORD, false)) {
        if (MatchKeyword(Keyword_module, false) && !MatchAhead(TK_DOT, 1)) {
            auto module_decl = ParseModuleDeclaration();
            
            if (top_level) {
                res = module_decl;
            } else {
                // module may not be declared in a block
                CompilerError error(LEVEL_ERROR,
                    Msg_module_declared_in_block, m_token_stream->Next().GetLocation());
                m_compilation_unit->GetErrorList().AddError(error);

                res = nullptr;
            }
        } else if (MatchKeyword(Keyword_import, false)) {
            res = ParseImport();
        } else if (MatchKeyword(Keyword_export, false)) {
            res = ParseExportStatement();
        } else if (MatchKeyword(Keyword_let, false)
            || MatchKeyword(Keyword_const, false)
            || MatchKeyword(Keyword_ref, false))
        {
            res = ParseVariableDeclaration();
        } else if (MatchKeyword(Keyword_func, false)) {
            if (MatchAhead(TK_IDENT, 1)) {
                res = ParseFunctionDefinition();
            } else {
                res = ParseFunctionExpression();
            }
        } else if (MatchKeyword(Keyword_class, false) || MatchKeyword(Keyword_proxy, false)) {
            res = ParseTypeDefinition();
        } else if (MatchKeyword(Keyword_enum, false)) {
            res = ParseEnumDefinition();
        } else if (MatchKeyword(Keyword_if, false)) {
            res = ParseIfStatement();
        } else if (MatchKeyword(Keyword_while, false)) {
            res = ParseWhileLoop();
        } else if (MatchKeyword(Keyword_for, false)) {
            res = ParseForLoop();
        } else if (MatchKeyword(Keyword_try, false)) {
            res = ParseTryCatchStatement();
        } else if (MatchKeyword(Keyword_return, false)) {
            res = ParseReturnStatement();
        } else if (MatchKeyword(Keyword_syntax, false)) {
            res = ParseSyntaxDefinition();
        } else {
            res = ParseExpression();
        }
    } else if (Match(TK_DIRECTIVE, false)) {
        res = ParseDirective();
    } else if (Match(TK_OPEN_BRACE, false)) {
        res = ParseBlock();
    } else if (Match(TK_IDENT, false) && MatchAhead(TK_COLON, 1)) {
        res = ParseVariableDeclaration();
    } else {
        res = ParseExpression(false);
    }

    if (read_terminators && res != nullptr && m_token_stream->HasNext()) {
        ExpectEndOfStmt();
    }

    return res;
}

RC<AstModuleDeclaration> Parser::ParseModuleDeclaration()
{
    if (Token module_decl = ExpectKeyword(Keyword_module, true)) {
        if (Token module_name = Expect(TK_IDENT, true)) {
            // expect open brace
            if (Expect(TK_OPEN_BRACE, true)) {
                RC<AstModuleDeclaration> module_ast(new AstModuleDeclaration(
                    module_name.GetValue(),
                    module_decl.GetLocation()
                ));

                // build up the module declaration with statements
                while (m_token_stream->HasNext() && !Match(TK_CLOSE_BRACE, false)) {
                    // skip statement terminator tokens
                    if (!Match(TK_SEMICOLON, true) && !Match(TK_NEWLINE, true)) {

                        // parse at top level, to allow for nested modules
                        if (auto stmt = ParseStatement(true)) {
                            module_ast->AddChild(stmt);
                        }
                    }
                }

                // expect close brace
                if (Expect(TK_CLOSE_BRACE, true)) {
                    return module_ast;
                }
            }
        }
    }

    return nullptr;
}

RC<AstDirective> Parser::ParseDirective()
{
    if (Token token = Expect(TK_DIRECTIVE, true)) {
        // the arguments will be held in an array expression
        Array<std::string> args;

        while (m_token_stream->HasNext() && !(Match(TK_SEMICOLON, true) || Match(TK_NEWLINE, true))) {
            Token token = m_token_stream->Peek();

            args.PushBack(token.GetValue());
            m_token_stream->Next();
        }

        return RC<AstDirective>(new AstDirective(
            token.GetValue(),
            args,
            token.GetLocation()
        ));
    }

    return nullptr;
}

RC<AstExpression> Parser::ParseTerm(
    bool override_commas,
    bool override_fat_arrows,
    bool override_angle_brackets,
    bool override_square_brackets,
    bool override_parentheses,
    bool override_question_mark
)
{
    Token token = m_token_stream->Peek();
    
    if (!token) {
        m_compilation_unit->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_unexpected_eof,
            CurrentLocation()
        ));

        if (m_token_stream->HasNext()) {
            m_token_stream->Next();
        }

        return nullptr;
    }

    RC<AstExpression> expr;

    if (Match(TK_OPEN_PARENTH)) {
        expr = ParseParentheses();
    } else if (Match(TK_OPEN_BRACKET)) {
        expr = ParseArrayExpression();
    } else if (Match(TK_INTEGER)) {
        expr = ParseIntegerLiteral();
    } else if (Match(TK_FLOAT)) {
        expr = ParseFloatLiteral();
    } else if (Match(TK_STRING)) {
        expr = ParseStringLiteral();
    } else if (Match(TK_IDENT)) {
        if (MatchAhead(TK_DOUBLE_COLON, 1)) {
            expr = ParseModuleAccess();
        } else {
            auto identifier = ParseIdentifier();

            if (!override_angle_brackets && MatchOperator("<")) {
                expr = ParseAngleBrackets(identifier);
            } else {
                expr = std::move(identifier);
            }
        }
    } else if (Match(TK_DOUBLE_COLON)) {
        expr = ParseModuleAccess();
    } else if (MatchKeyword(Keyword_module)) {
        expr = ParseModuleProperty();
    } else if (MatchKeyword(Keyword_self)) {
        expr = ParseIdentifier(true);
    } else if (MatchKeyword(Keyword_true)) {
        expr = ParseTrue();
    } else if (MatchKeyword(Keyword_false)) {
        expr = ParseFalse();
    } else if (MatchKeyword(Keyword_null)) {
        expr = ParseNil();
    } else if (MatchKeyword(Keyword_new)) {
        expr = ParseNewExpression();
    } else if (MatchKeyword(Keyword_func)) {
        expr = ParseFunctionExpression();
    } else if (MatchKeyword(Keyword_valueof)) {
        expr = ParseValueOfExpression();
    } else if (MatchKeyword(Keyword_typeof)) {
        expr = ParseTypeOfExpression();
    } else if (MatchKeyword(Keyword_meta)) {
        expr = ParseMetaProperty();
    } else if (MatchKeyword(Keyword_class)) {
        expr = ParseTypeExpression();
    } else if (MatchKeyword(Keyword_enum)) {
        expr = ParseEnumExpression();
    } else if (MatchKeyword(Keyword_throw)) {
        expr = ParseThrowExpression();
    } else if (Match(TK_OPERATOR)) {
        expr = ParseUnaryExpressionPrefix();
    } else {
        if (token.GetTokenClass() == TK_NEWLINE) {
            m_compilation_unit->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_unexpected_eol,
                token.GetLocation()
            ));
        } else {
            m_compilation_unit->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_unexpected_token,
                token.GetLocation(),
                token.GetValue()
            ));
        }

        if (m_token_stream->HasNext()) {
            m_token_stream->Next();
        }

        return nullptr;
    }

    auto operator_token = Token::EMPTY;

    while (expr != nullptr && (
        Match(TK_DOT) ||
        (!override_parentheses && Match(TK_OPEN_PARENTH)) ||
        (!override_fat_arrows && Match(TK_FAT_ARROW)) ||
        (!override_square_brackets && Match(TK_OPEN_BRACKET)) ||
        // (!override_angle_brackets && MatchOperator("<")) ||
        MatchKeyword(Keyword_has) ||
        MatchKeyword(Keyword_is) ||
        ((operator_token = Match(TK_OPERATOR)) && Operator::IsUnaryOperator(operator_token.GetValue(), OperatorType::POSTFIX))
    ))
    {
        if (operator_token) {
            expr = ParseUnaryExpressionPostfix(expr);
            operator_token = Token::EMPTY;
        }

        if (Match(TK_DOT)) {
            expr = ParseMemberExpression(expr);
        }

        if (!override_square_brackets && Match(TK_OPEN_BRACKET)) {
            expr = ParseArrayAccess(expr);
        }

        // if (!override_angle_brackets && MatchOperator("<")) {
        //     expr = ParseAngleBrackets(expr);
        // }

        if (!override_parentheses && Match(TK_OPEN_PARENTH)) {
            expr = ParseCallExpression(expr);
        }

        if (MatchKeyword(Keyword_has)) {
            expr = ParseHasExpression(expr);
        }

        if (MatchKeyword(Keyword_is)) {
            expr = ParseIsExpression(expr);
        }
    }

    return expr;
}

RC<AstExpression> Parser::ParseParentheses()
{
    SourceLocation location = CurrentLocation();
    RC<AstExpression> expr;
    int before_pos = m_token_stream->GetPosition();

    Expect(TK_OPEN_PARENTH, true);

    if (!Match(TK_CLOSE_PARENTH) && !Match(TK_IDENT) && !Match(TK_KEYWORD)) {
        expr = ParseExpression(true);
        Expect(TK_CLOSE_PARENTH, true);
    } else {
        if (Match(TK_CLOSE_PARENTH, true)) {
            // if '()' found, it is a function with empty parameters
            // allow ParseFunctionParameters() to handle parentheses
            m_token_stream->SetPosition(before_pos);

            Array<RC<AstParameter>> params;
            
            if (Match(TK_OPEN_PARENTH, true)) {
                params = ParseFunctionParameters();
                Expect(TK_CLOSE_PARENTH, true);
            }

            expr = ParseFunctionExpression(false, params);
        } else {
            bool found_function_token = false;

            if (MatchKeyword(Keyword_const) || MatchKeyword(Keyword_let)) {
                found_function_token = true;
            } else {
                expr = ParseExpression(true);
            }

            if (Match(TK_COMMA) ||
                Match(TK_COLON) ||
                Match(TK_ELLIPSIS)) {
            
                found_function_token = true;
            } else if (Match(TK_CLOSE_PARENTH, false)) {
                int before = m_token_stream->GetPosition();
                m_token_stream->Next();

                // function return type
                if (Match(TK_COLON)) {
                    found_function_token = true;
                }
                
                // go back to where it was before reading the ')' token
                m_token_stream->SetPosition(before);
            }

            if (found_function_token) {
                // go back to before open '(' found, 
                // to allow ParseFunctionParameters() to handle it
                m_token_stream->SetPosition(before_pos);

                Array<RC<AstParameter>> params;
                
                if (Match(TK_OPEN_PARENTH, true)) {
                    params = ParseFunctionParameters();
                    Expect(TK_CLOSE_PARENTH, true);
                }

                // parse function parameters
                expr = ParseFunctionExpression(false, params);
            } else {
                Expect(TK_CLOSE_PARENTH, true);

                if (Match(TK_OPEN_BRACE, true)) {
                    // if '{' found after ')', it is a function
                    m_token_stream->SetPosition(before_pos);

                    Array<RC<AstParameter>> params;
                    
                    if (Match(TK_OPEN_PARENTH, true)) {
                        params = ParseFunctionParameters();
                        Expect(TK_CLOSE_PARENTH, true);
                    }

                    expr = ParseFunctionExpression(false, params);
                }
            }
        }
    }

    return expr;
}

RC<AstExpression> Parser::ParseAngleBrackets(RC<AstIdentifier> target)
{
    SourceLocation location = CurrentLocation();
    int before_pos = m_token_stream->GetPosition();

    Array<RC<AstArgument>> args;

    if (Token token = ExpectOperator("<", true)) {
        if (MatchOperator(">", true)) {
            return RC<AstTemplateInstantiation>(new AstTemplateInstantiation(
                target,
                args,
                token.GetLocation()
            ));
        }

        ++m_template_argument_depth;

        do {
            const SourceLocation arg_location = CurrentLocation();
            bool is_splat_arg = false;
            bool is_named_arg = false;
            std::string arg_name;

            if (Match(TK_ELLIPSIS, true)) {
                is_splat_arg = true;
            }

            // check for name: value expressions (named arguments)
            if (Match(TK_IDENT)) {
                if (MatchAhead(TK_COLON, 1)) {
                    // named argument
                    is_named_arg = true;
                    Token name_token = Expect(TK_IDENT, true);
                    arg_name = name_token.GetValue();

                    // read the colon
                    Expect(TK_COLON, true);
                }
            }

            if (auto term = ParseExpression(true, false, false)) { // override commas
                args.PushBack(RC<AstArgument>(new AstArgument(
                    term,
                    is_splat_arg,
                    is_named_arg,
                    arg_name,
                    arg_location
                )));
            } else {
                --m_template_argument_depth;

                // not an argument, revert to start.
                m_token_stream->SetPosition(before_pos);

                // return as comparison expression
                return ParseBinaryExpression(0, target);
            }
        } while (Match(TK_COMMA, true));

        --m_template_argument_depth;

        if (MatchOperator(">", true)) {
            return RC<AstTemplateInstantiation>(new AstTemplateInstantiation(
                target,
                args,
                token.GetLocation()
            ));
        } else {
            // no closing bracket found
            m_token_stream->SetPosition(before_pos);
            // return as comparison expression
            return ParseBinaryExpression(0, target);
        }
    }

    return nullptr;
}

RC<AstConstant> Parser::ParseIntegerLiteral()
{
    if (Token token = Expect(TK_INTEGER, true)) {
        std::istringstream ss(token.GetValue());

        if (token.GetFlags()[0] == '\0' || token.GetFlags()[0] == 'i') {
            hyperion::Int32 value;
            ss >> value;

            return RC<AstInteger>(new AstInteger(
                value,
                token.GetLocation()
            ));
        } else if (token.GetFlags()[0] == 'u') {
            hyperion::UInt32 value;
            ss >> value;

            return RC<AstUnsignedInteger>(new AstUnsignedInteger(
                value,
                token.GetLocation()
            ));
        } else if (token.GetFlags()[0] == 'f') {
            hyperion::Float32 value;
            ss >> value;

            return RC<AstFloat>(new AstFloat(
                value,
                token.GetLocation()
            ));
        } else {
            m_compilation_unit->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_illegal_expression,
                token.GetLocation()
            ));
        }
    }

    return nullptr;
}

RC<AstFloat> Parser::ParseFloatLiteral()
{
    if (Token token = Expect(TK_FLOAT, true)) {
        hyperion::Float32 value = std::atof(token.GetValue().c_str());

        return RC<AstFloat>(new AstFloat(
            value,
            token.GetLocation()
        ));
    }

    return nullptr;
}

RC<AstString> Parser::ParseStringLiteral()
{
    if (Token token = Expect(TK_STRING, true)) {
        return RC<AstString>(new AstString(
            token.GetValue(),
            token.GetLocation()
        ));
    }

    return nullptr;
}

RC<AstIdentifier> Parser::ParseIdentifier(bool allow_keyword)
{
    if (Token token = ExpectIdentifier(allow_keyword, false)) {
        // read identifier token
        if (m_token_stream->HasNext()) {
            m_token_stream->Next();
        }

        // return variable
        return RC<AstVariable>(new AstVariable(
            token.GetValue(),
            token.GetLocation()
        ));
    }

    return nullptr;
}

RC<AstArgument> Parser::ParseArgument(RC<AstExpression> expr)
{
    SourceLocation location = CurrentLocation();

    bool is_splat_arg = false;
    bool is_named_arg = false;
    std::string arg_name;

    if (expr == nullptr) {
        if (Match(TK_ELLIPSIS, true)) {
            is_splat_arg = true;
        } else if (Match(TK_IDENT)) { // check for name: value expressions (named arguments)
            if (MatchAhead(TK_COLON, 1)) {
                // named argument
                is_named_arg = true;
                Token name_token = Expect(TK_IDENT, true);
                arg_name = name_token.GetValue();

                // read the colon
                Expect(TK_COLON, true);
            }
        }

        expr = ParseExpression(true, true);
    }

    if (expr != nullptr) {
        return RC<AstArgument>(new AstArgument(
            expr,
            is_splat_arg,
            is_named_arg,
            arg_name,
            location
        ));
    }

    m_compilation_unit->GetErrorList().AddError(CompilerError(
        LEVEL_ERROR,
        Msg_illegal_expression,
        location
    ));

    return nullptr;
}

RC<AstArgumentList> Parser::ParseArguments(bool require_parentheses)
{
    const SourceLocation location = CurrentLocation();

    Array<RC<AstArgument>> args;

    if (require_parentheses) {
        Expect(TK_OPEN_PARENTH, true);
    }

    while (!require_parentheses || !Match(TK_CLOSE_PARENTH, false)) {
        if (auto arg = ParseArgument(nullptr)) {
            args.PushBack(arg);

            if (!Match(TK_COMMA, true)) {
                break;
            }
        } else {
            return nullptr;
        }
    }

    if (require_parentheses) {
        Expect(TK_CLOSE_PARENTH, true);
    }

    return RC<AstArgumentList>(new AstArgumentList(
        args,
        location
    ));
}

RC<AstCallExpression> Parser::ParseCallExpression(RC<AstExpression> target, bool require_parentheses)
{
    if (target != nullptr) {
        if (auto args = ParseArguments(require_parentheses)) {
            return RC<AstCallExpression>(new AstCallExpression(
                target,
                args->GetArguments(),
                true, // allow 'self' to be inserted
                target->GetLocation()
            ));
        }
    }

    return nullptr;
}

RC<AstModuleAccess> Parser::ParseModuleAccess()
{
    const SourceLocation location = CurrentLocation();

    Token token = Token::EMPTY;
    bool global_module_access = false;

    if (Match(TK_DOUBLE_COLON, true)) {
        // global module access for prepended double colon.
        global_module_access = true;
    } else {
        token = Expect(TK_IDENT, true);
        Expect(TK_DOUBLE_COLON, true);
    }

    if (token || global_module_access) {
        RC<AstExpression> expr;

        if (MatchAhead(TK_DOUBLE_COLON, 1)) {
            expr = ParseModuleAccess();
        } else {
            auto identifier = ParseIdentifier(true);

            if (MatchOperator("<")) {
                expr = ParseAngleBrackets(identifier);
            } else {
                expr = std::move(identifier);
            }
        }

        if (expr != nullptr) {
            return RC<AstModuleAccess>(new AstModuleAccess(
                global_module_access
                    ? Config::global_module_name
                    : token.GetValue(),
                expr,
                location
            ));
        }
    }

    return nullptr;
}

RC<AstModuleProperty> Parser::ParseModuleProperty()
{
    Token token = ExpectKeyword(Keyword_module, true);
    if (!token) {
        return nullptr;
    }

    Token dot = Expect(TK_DOT, true);
    if (!dot) {
        return nullptr;
    }

    Token ident = Expect(TK_IDENT, true);
    if (!ident) {
        return nullptr;
    }

    return RC<AstModuleProperty>(new AstModuleProperty(
        ident.GetValue(),
        token.GetLocation()
    ));
}

RC<AstMember> Parser::ParseMemberExpression(RC<AstExpression> target)
{
    Expect(TK_DOT, true);

    // allow quoted strings as data member names
    Token ident = Match(TK_STRING, false)
        ? m_token_stream->Next()
        : ExpectIdentifier(true, true);

    if (ident) {
        // if (Match(TK_OPEN_PARENTH)) {
        //     if (auto argument_list = ParseArguments()) {
        //         return RC<AstMemberCallExpression>(new AstMemberCallExpression(
        //             ident.GetValue(),
        //             target,
        //             argument_list,
        //             ident.GetLocation()
        //         ));
        //     } else {
        //         return nullptr;
        //     }
        // } else {
            return RC<AstMember>(new AstMember(
                ident.GetValue(),
                target,
                ident.GetLocation()
            ));
        // }
    }

    return nullptr;
}

RC<AstArrayAccess> Parser::ParseArrayAccess(RC<AstExpression> target)
{
    if (Token token = Expect(TK_OPEN_BRACKET, true)) {
        RC<AstExpression> expr;

        if (Match(TK_CLOSE_BRACKET, true)) {
            m_compilation_unit->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_illegal_expression,
                token.GetLocation()
            ));
        } else {
            expr = ParseExpression();
            Expect(TK_CLOSE_BRACKET, true);
        }

        if (expr != nullptr) {
            return RC<AstArrayAccess>(new AstArrayAccess(
                target,
                expr,
                token.GetLocation()
            ));
        }
    }

    return nullptr;
}

RC<AstHasExpression> Parser::ParseHasExpression(RC<AstExpression> target)
{
    if (Token token = ExpectKeyword(Keyword_has, true)) {
        if (Token field = Expect(TK_STRING, true)) {
            return RC<AstHasExpression>(new AstHasExpression(
                target,
                field.GetValue(),
                target->GetLocation()
            ));
        }
    }

    return nullptr;
}

RC<AstIsExpression> Parser::ParseIsExpression(RC<AstExpression> target)
{
    if (Token token = ExpectKeyword(Keyword_is, true)) {
        if (auto type_expression = ParsePrototypeSpecification()) {
            return RC<AstIsExpression>(new AstIsExpression(
                target,
                type_expression,
                target->GetLocation()
            ));
        }
    }

    return nullptr;
}

RC<AstNewExpression> Parser::ParseNewExpression()
{
    if (Token token = ExpectKeyword(Keyword_new, true)) {
        if (auto proto = ParsePrototypeSpecification()) {
            RC<AstArgumentList> arg_list;

            if (Match(TK_OPEN_PARENTH, false)) {
                // parse args
                arg_list = ParseArguments();
            }

            return RC<AstNewExpression>(new AstNewExpression(
                proto,
                arg_list,
                true, // enable construct call
                token.GetLocation()
            ));
        }
    }

    return nullptr;
}

RC<AstTrue> Parser::ParseTrue()
{
    if (Token token = ExpectKeyword(Keyword_true, true)) {
        return RC<AstTrue>(new AstTrue(
            token.GetLocation()
        ));
    }

    return nullptr;
}

RC<AstFalse> Parser::ParseFalse()
{
    if (Token token = ExpectKeyword(Keyword_false, true)) {
        return RC<AstFalse>(new AstFalse(
            token.GetLocation()
        ));
    }

    return nullptr;
}

RC<AstNil> Parser::ParseNil()
{
    if (Token token = ExpectKeyword(Keyword_null, true)) {
        return RC<AstNil>(new AstNil(
            token.GetLocation()
        ));
    }

    return nullptr;
}

RC<AstBlock> Parser::ParseBlock()
{
    if (Token token = Expect(TK_OPEN_BRACE, true)) {
        RC<AstBlock> block(new AstBlock(
            token.GetLocation()
        ));

        while (!Match(TK_CLOSE_BRACE, false)) {
            // skip statement terminator tokens
            if (!Match(TK_SEMICOLON, true) && !Match(TK_NEWLINE, true)) {
                if (auto stmt = ParseStatement()) {
                    block->AddChild(stmt);
                } else {
                    break;
                }
            }
        }

        Expect(TK_CLOSE_BRACE, true);

        return block;
    }

    return nullptr;
}

RC<AstIfStatement> Parser::ParseIfStatement()
{
    if (Token token = ExpectKeyword(Keyword_if, true)) {
        bool has_parentheses = false;

        if (Match(TK_OPEN_PARENTH, true)) {
            has_parentheses = true;
        }

        RC<AstExpression> conditional;
        if (!(conditional = ParseExpression())) {
            return nullptr;
        }

        if (has_parentheses) {
            if (!Match(TK_CLOSE_PARENTH, true)) {
                m_compilation_unit->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_unmatched_parentheses,
                    token.GetLocation()
                ));
                
                if (m_token_stream->HasNext()) {
                    m_token_stream->Next();
                }
            }
        }

        RC<AstBlock> block;
        if (!(block = ParseBlock())) {
            return nullptr;
        }

        RC<AstBlock> else_block = nullptr;
        // parse else statement if the "else" keyword is found
        if (Token else_token = MatchKeyword(Keyword_else, true)) {
            // check for "if" keyword for else-if
            if (MatchKeyword(Keyword_if, false)) {
                else_block = RC<AstBlock>(new AstBlock(
                    else_token.GetLocation()
                ));

                if (auto else_if_block = ParseIfStatement()) {
                    else_block->AddChild(else_if_block);
                }
            } else {
                // parse block after "else keyword
                if (!(else_block = ParseBlock())) {
                    return nullptr;
                }
            }
        }

        return RC<AstIfStatement>(new AstIfStatement(
            conditional,
            block,
            else_block,
            token.GetLocation()
        ));
    }

    return nullptr;
}

RC<AstWhileLoop> Parser::ParseWhileLoop()
{
    if (Token token = ExpectKeyword(Keyword_while, true)) {
        if (!Expect(TK_OPEN_PARENTH, true)) {
            return nullptr;
        }

        RC<AstExpression> conditional;
        if (!(conditional = ParseExpression())) {
            return nullptr;
        }

        if (!Expect(TK_CLOSE_PARENTH, true)) {
            return nullptr;
        }

        RC<AstBlock> block;
        if (!(block = ParseBlock())) {
            return nullptr;
        }

        return RC<AstWhileLoop>(new AstWhileLoop(
            conditional,
            block,
            token.GetLocation()
        ));
    }

    return nullptr;
}

RC<AstStatement> Parser::ParseForLoop()
{
    const auto position_before = m_token_stream->GetPosition();

    if (Token token = ExpectKeyword(Keyword_for, true)) {
        if (!Expect(TK_OPEN_PARENTH, true)) {
            return nullptr;
        }

        RC<AstStatement> decl_part;

        if (!Match(TK_SEMICOLON)) {
            if ((decl_part = ParseStatement(false, false))) { // do not eat ';'
                // if (!Match(TK_SEMICOLON)) {
                //     m_token_stream->SetPosition(position_before);

                //     return ParseForEachLoop();
                // }
            } else {
                return nullptr;
            }
        }

        if (!Expect(TK_SEMICOLON, true)) {
            return nullptr;
        }

        RC<AstExpression> condition_part;

        if (!Match(TK_SEMICOLON)) {
            if (!(condition_part = ParseExpression())) {
                return nullptr;
            }
        }

        if (!Expect(TK_SEMICOLON, true)) {
            return nullptr;
        }

        RC<AstExpression> increment_part;

        if (!Match(TK_CLOSE_PARENTH)) {
            if (!(increment_part = ParseExpression())) {
                return nullptr;
            }
        }

        if (!Expect(TK_CLOSE_PARENTH, true)) {
            return nullptr;
        }

        RC<AstBlock> block;
        if (!(block = ParseBlock())) {
            return nullptr;
        }

        return RC<AstForLoop>(new AstForLoop(
            decl_part,
            condition_part,
            increment_part,
            block,
            token.GetLocation()
        ));
    }

    return nullptr;
}

RC<AstTryCatch> Parser::ParseTryCatchStatement()
{
    if (Token token = ExpectKeyword(Keyword_try, true)) {
        RC<AstBlock> try_block = ParseBlock();
        RC<AstBlock> catch_block;

        if (ExpectKeyword(Keyword_catch, true)) {
            // TODO: Add exception argument
            catch_block = ParseBlock();
        }

        if (try_block != nullptr && catch_block != nullptr) {
            return RC<AstTryCatch>(new AstTryCatch(
                try_block,
                catch_block,
                token.GetLocation()
            ));
        }
    }

    return nullptr;
}

RC<AstThrowExpression> Parser::ParseThrowExpression()
{
    if (Token token = ExpectKeyword(Keyword_throw, true)) {
        if (auto expr = ParseExpression()) {
            return RC<AstThrowExpression>(new AstThrowExpression(
                expr,
                token.GetLocation()
            ));
        }
    }

    return nullptr;
}

RC<AstExpression> Parser::ParseBinaryExpression(
    int expr_prec,
    RC<AstExpression> left
)
{
    while (true) {
        // get precedence
        const Operator *op = nullptr;
        int precedence = OperatorPrecedence(op);
        if (precedence < expr_prec) {
            return left;
        }

        // read the operator token
        Token token = Expect(TK_OPERATOR, true);

        if (auto right = ParseTerm()) {
            // next part of expression's precedence
            const Operator *next_op = nullptr;
            
            int next_prec = OperatorPrecedence(next_op);
            if (precedence < next_prec) {
                right = ParseBinaryExpression(precedence + 1, right);
                if (right == nullptr) {
                    return nullptr;
                }
            }

            left = RC<AstBinaryExpression>(new AstBinaryExpression(
                left,
                right,
                op,
                token.GetLocation()
            ));
        }
    }

    return nullptr;
}

RC<AstExpression> Parser::ParseUnaryExpressionPrefix()
{
    // read the operator token
    if (Token token = Expect(TK_OPERATOR, true)) {
        const Operator *op = nullptr;
        if (Operator::IsUnaryOperator(token.GetValue(), /*OperatorType::PREFIX,*/ op)) {
            if (auto term = ParseTerm()) {
                return RC<AstUnaryExpression>(new AstUnaryExpression(
                    term,
                    op,
                    false, // postfix version
                    token.GetLocation()
                ));
            }

            return nullptr;
        } else {
            // internal error: operator not defined
            m_compilation_unit->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_illegal_operator,
                token.GetLocation(),
                token.GetValue()
            ));
        }
    }

    return nullptr;
}

RC<AstExpression> Parser::ParseUnaryExpressionPostfix(const RC<AstExpression> &expr)
{
    // read the operator token
    if (Token token = Expect(TK_OPERATOR, true)) {
        const Operator *op = nullptr;
        if (Operator::IsUnaryOperator(token.GetValue(), /*OperatorType::POSTFIX,*/ op)) {
            return RC<AstUnaryExpression>(new AstUnaryExpression(
                expr,
                op,
                true, // postfix version
                token.GetLocation()
            ));
        } else {
            // internal error: operator not defined
            m_compilation_unit->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_illegal_operator,
                token.GetLocation(),
                token.GetValue()
            ));
        }
    }

    return nullptr;
}

RC<AstExpression> Parser::ParseTernaryExpression(const RC<AstExpression> &conditional)
{
    if (Token token = Expect(TK_QUESTION_MARK, true)) {
        // parse next ('true' part)

        auto true_expr = ParseExpression();

        if (true_expr == nullptr) {
            return nullptr;
        }

        if (!Expect(TK_COLON, true)) {
            return nullptr;
        }

        auto false_expr = ParseExpression();

        if (false_expr == nullptr) {
            return nullptr;
        }

        return RC<AstTernaryExpression>(new AstTernaryExpression(
            conditional,
            true_expr,
            false_expr,
            conditional->GetLocation()
        ));
    }

    return nullptr;
}

RC<AstExpression> Parser::ParseExpression(
    bool override_commas,
    bool override_fat_arrows,
    bool override_angle_brackets,
    bool override_question_mark
)
{
    if (auto term = ParseTerm(override_commas, override_fat_arrows, override_angle_brackets, false, false, override_question_mark)) {
        if (Match(TK_OPERATOR, false)) {
            // drop out of expression, return to parent call
            if (MatchOperator(">", false) && m_template_argument_depth > 0) {
                return term;
            }

            if (auto bin_expr = ParseBinaryExpression(0, term)) {
                term = bin_expr;
            } else {
                return nullptr;
            }
        }

        if (Match(TK_QUESTION_MARK)) {
            if (auto ternary_expr = ParseTernaryExpression(term)) {
                term = ternary_expr;
            }
        }

        return term;
    }

    return nullptr;
}

RC<AstPrototypeSpecification> Parser::ParsePrototypeSpecification()
{
    const SourceLocation location = CurrentLocation();

    if (auto term = ParseTerm(
        true,  // override commas
        true,  // override =>
        false, // override <>
        true,  // override []
        true   // override ()
    )) {
        //while (Match(TK_OPEN_BRACKET) || Match(TK_QUESTION_MARK)) {
            // question mark at the end of a type is syntactical sugar for `Maybe(T)`
            /*if (Token token = Match(TK_QUESTION_MARK, true)) {
                term = RC<AstTemplateInstantiation>(new AstTemplateInstantiation(
                    RC<AstVariable>(new AstVariable(
                        "Maybe",
                        token.GetLocation()
                    )),
                    {
                        RC<AstArgument>(new AstArgument(
                            term,
                            false,
                            false,
                            "",
                            term->GetLocation()
                        ))
                    },
                    term->GetLocation()
                ));
            } else */if (Token token = Match(TK_OPEN_BRACKET, true)) {
                // array braces at the end of a type are syntactical sugar for `Array(T)`
                term = RC<AstTemplateInstantiation>(new AstTemplateInstantiation(
                    RC<AstVariable>(new AstVariable(
                        BuiltinTypes::ARRAY->GetName(),
                        token.GetLocation()
                    )),
                    {
                        RC<AstArgument>(new AstArgument(
                            term,
                            false,
                            false,
                            "",
                            term->GetLocation()
                        ))
                    },
                    term->GetLocation()
                ));

                if (!Expect(TK_CLOSE_BRACKET, true)) {
                    return nullptr;
                }
            }
        //}

        return RC<AstPrototypeSpecification>(new AstPrototypeSpecification(
            term,
            location
        ));
    }

    return nullptr;
}

RC<AstExpression> Parser::ParseAssignment()
{
    if (MatchOperator("=", true)) {
        // read assignment expression
        const SourceLocation expr_location = CurrentLocation();

        if (auto assignment = ParseExpression()) {
            return assignment;
        } else {
            m_compilation_unit->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_illegal_expression,
                expr_location
            ));
        }
    }

    return nullptr;
}

RC<AstVariableDeclaration> Parser::ParseVariableDeclaration(
    bool allow_keyword_names,
    bool allow_quoted_names,
    IdentifierFlagBits flags
)
{
    const SourceLocation location = CurrentLocation();

    static const Keywords prefix_keywords[] = {
        Keyword_let,
        Keyword_const,
        Keyword_ref
    };

    std::set<Keywords> used_specifiers;

    while (Match(TK_KEYWORD, false)) {
        bool found_keyword = false;

        for (const Keywords &keyword : prefix_keywords) {
            if (MatchKeyword(keyword, true)) {
                if (used_specifiers.find(keyword) == used_specifiers.end()) {
                    used_specifiers.insert(keyword);

                    found_keyword = true;

                    break;
                }
            }
        }

        if (!found_keyword) {
            Token token = m_token_stream->Next();

            m_compilation_unit->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_unexpected_token,
                token.GetLocation(),
                token.GetValue()
            ));

            break;
        }
    }

    if (used_specifiers.find(Keyword_const) != used_specifiers.end()) {
        flags |= IdentifierFlags::FLAG_CONST;
    }

    if (used_specifiers.find(Keyword_ref) != used_specifiers.end()) {
        flags |= IdentifierFlags::FLAG_REF;
    }

    Token identifier = Token::EMPTY;
    
    // an identifier name that is enquoted in strings is valid
    if (allow_quoted_names) {
        identifier = Match(TK_STRING, false)
            ? m_token_stream->Next()
            : ExpectIdentifier(allow_keyword_names, true);
    } else {
        identifier = ExpectIdentifier(allow_keyword_names, true);
    }

    if (!identifier.Empty()) {
        SourceLocation template_expr_location = CurrentLocation();

        Array<RC<AstParameter>> template_expr_params;

        if (Token lt = MatchOperator("<", false)) {
            flags |= IdentifierFlags::FLAG_GENERIC;

            template_expr_params = ParseGenericParameters();
            template_expr_location = lt.GetLocation();
        }

        RC<AstPrototypeSpecification> proto;
        RC<AstExpression> assignment;

        if (Match(TK_COLON, true)) {
            // read object type
            proto = ParsePrototypeSpecification();
        }

        if ((assignment = ParseAssignment())) {
            if (flags & IdentifierFlags::FLAG_GENERIC) {
                assignment = RC<AstTemplateExpression>(new AstTemplateExpression(
                    assignment,
                    template_expr_params,
                    proto,
                    assignment->GetLocation()
                ));
            }
        }

        return RC<AstVariableDeclaration>(new AstVariableDeclaration(
            identifier.GetValue(),
            (flags & IdentifierFlags::FLAG_GENERIC) ? nullptr : proto,
            assignment,
            {},
            flags,
            location
        ));
    }

    return nullptr;
}

RC<AstStatement> Parser::ParseFunctionDefinition(bool require_keyword)
{
    const SourceLocation location = CurrentLocation();

    IdentifierFlagBits flags = IdentifierFlags::FLAG_CONST;

    if (require_keyword) {
        if (!ExpectKeyword(Keyword_func, true)) {
            return nullptr;
        }
    } else {
        // match and read in the case that it is found
        MatchKeyword(Keyword_func, true);
    }

    if (Token identifier = Expect(TK_IDENT, true)) {
        bool is_generic = false;
        Array<RC<AstParameter>> generic_parameters;

        // check for generic
        if (MatchOperator("<", false)) {
            flags |= IdentifierFlags::FLAG_GENERIC;

            generic_parameters = ParseGenericParameters();
        }

        RC<AstExpression> assignment;
        Array<RC<AstParameter>> params;
        
        if (Match(TK_OPEN_PARENTH, true)) {
            params = ParseFunctionParameters();
            Expect(TK_CLOSE_PARENTH, true);
        }

        assignment = ParseFunctionExpression(false, params);

        if (assignment == nullptr) {
            return nullptr;
        }

        if (flags & IdentifierFlags::FLAG_GENERIC) {
            assignment = RC<AstTemplateExpression>(new AstTemplateExpression(
                assignment,
                generic_parameters,
                nullptr,
                assignment->GetLocation()
            ));
        }

        return RC<AstVariableDeclaration>(new AstVariableDeclaration(
            identifier.GetValue(),
            nullptr, // prototype specification
            assignment,
            {}, //template_expr_params,
            flags,
            location
        ));
    }

    return nullptr;
}

RC<AstFunctionExpression> Parser::ParseFunctionExpression(
    bool require_keyword,
    Array<RC<AstParameter>> params
)
{
    const Token token = require_keyword
        ? ExpectKeyword(Keyword_func, true)
        : Token::EMPTY;

    const SourceLocation location = token
        ? token.GetLocation()
        : CurrentLocation();

    if (require_keyword || !token) {
        if (require_keyword) {
            // read params
            if (Match(TK_OPEN_PARENTH, true)) {
                params = ParseFunctionParameters();
                Expect(TK_CLOSE_PARENTH, true);
            }
        }

        RC<AstPrototypeSpecification> type_spec;

        if (Match(TK_COLON, true)) {
            // read return type for functions
            type_spec = ParsePrototypeSpecification();
        }

        RC<AstBlock> block;

        if (Match(TK_FAT_ARROW, true)) {
            RC<AstReturnStatement> return_statement(new AstReturnStatement(
                ParseExpression(),
                location
            ));

            block.Reset(new AstBlock(
                { std::move(return_statement) },
                location
            ));
        } else {
            block = ParseBlock();
        }

        if (block != nullptr) {
            return RC<AstFunctionExpression>(new AstFunctionExpression(
                params,
                type_spec,
                block,
                location
            ));
        }
    }

    return nullptr;
}

RC<AstArrayExpression> Parser::ParseArrayExpression()
{
    if (Token token = Expect(TK_OPEN_BRACKET, true)) {
        Array<RC<AstExpression>> members;

        do {
            if (Match(TK_CLOSE_BRACKET, false)) {
                break;
            }

            if (auto expr = ParseExpression(true)) {
                members.PushBack(expr);
            }
        } while (Match(TK_COMMA, true));

        Expect(TK_CLOSE_BRACKET, true);

        return RC<AstArrayExpression>(new AstArrayExpression(
            members,
            token.GetLocation()
        ));
    }

    return nullptr;
}

RC<AstExpression> Parser::ParseValueOfExpression()
{
    if (Token token = ExpectKeyword(Keyword_valueof, true)) {
        RC<AstExpression> expr;

        if (!MatchAhead(TK_DOUBLE_COLON, 1)) {
            Token ident = Expect(TK_IDENT, true);
            expr.Reset(new AstVariable(
                ident.GetValue(),
                token.GetLocation()
            ));
        } else {
            do {
                Token ident = Expect(TK_IDENT, true);
                expr.Reset(new AstModuleAccess(
                    ident.GetValue(),
                    expr,
                    ident.GetLocation()
                ));
            } while (Match(TK_DOUBLE_COLON, true));
        }

        return expr;
    }

    return nullptr;
}

RC<AstTypeOfExpression> Parser::ParseTypeOfExpression()
{
    const SourceLocation location = CurrentLocation();
    
    if (Token token = ExpectKeyword(Keyword_typeof, true)) {
        SourceLocation expr_location = CurrentLocation();
        if (auto term = ParseTerm()) {
            return RC<AstTypeOfExpression>(new AstTypeOfExpression(
                term,
                location
            ));
        } else {
            m_compilation_unit->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_illegal_expression,
                expr_location
            ));
        }
    }

    return nullptr;
}

Array<RC<AstParameter>> Parser::ParseFunctionParameters()
{
    Array<RC<AstParameter>> parameters;

    bool found_variadic = false;
    bool keep_reading = true;

    while (keep_reading) {
        Token token = Token::EMPTY;

        if (Match(TK_CLOSE_PARENTH, false)) {
            keep_reading = false;
            break;
        }

        bool is_const = false,
            is_ref = false;

        if (MatchKeyword(Keyword_const, true)) {
            is_const = true;
        }

        if (MatchKeyword(Keyword_ref, true)) {
            is_ref = true;
        }
        
        if ((token = ExpectIdentifier(true, true))) {
            RC<AstPrototypeSpecification> type_spec;
            RC<AstExpression> default_param;

            // check if parameter type has been declared
            if (Match(TK_COLON, true)) {
                type_spec = ParsePrototypeSpecification();
            }

            if (found_variadic) {
                // found another parameter after variadic
                m_compilation_unit->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_argument_after_varargs,
                    token.GetLocation()
                ));
            }

            // if this parameter is variadic
            bool is_variadic = false;

            if (Match(TK_ELLIPSIS, true)) {
                is_variadic = true;
                found_variadic = true;
            }

            // check for default assignment
            if (MatchOperator("=", true)) {
                default_param = ParseExpression(true);
            }

            parameters.PushBack(RC<AstParameter>(new AstParameter(
                token.GetValue(),
                type_spec,
                default_param,
                is_variadic,
                is_const,
                is_ref,
                token.GetLocation()
            )));

            if (!Match(TK_COMMA, true)) {
                keep_reading = false;
            }
        } else {
            keep_reading = false;
        }
    }

    return parameters;
}

Array<RC<AstParameter>> Parser::ParseGenericParameters()
{
    Array<RC<AstParameter>> template_expr_params;

    if (Token lt = ExpectOperator("<", true)) {
        m_template_argument_depth++;

        template_expr_params = ParseFunctionParameters();

        ExpectOperator(">", true);

        m_template_argument_depth--;
    }

    return template_expr_params;
}

RC<AstStatement> Parser::ParseTypeDefinition()
{
    bool is_proxy_class = false;

    if (Token proxy_token = MatchKeyword(Keyword_proxy, true)) {
        is_proxy_class = true;
    }

    if (Token token = ExpectKeyword(Keyword_class, true)) {
        if (Token identifier = ExpectIdentifier(false, true)) {
            IdentifierFlagBits flags = IdentifierFlags::FLAG_CONST;

            Array<RC<AstParameter>> generic_parameters;
            RC<AstExpression> assignment;

            // check for generic
            if (MatchOperator("<", false)) {
                flags |= IdentifierFlags::FLAG_GENERIC;

                generic_parameters = ParseGenericParameters();
            }

            // check type alias
            if (MatchOperator("=", true)) {
                if (auto aliasee = ParsePrototypeSpecification()) {
                    return RC<AstTypeAlias>(new AstTypeAlias(
                        identifier.GetValue(), 
                        aliasee,
                        identifier.GetLocation()
                    ));
                } else {
                    return nullptr;
                }
            } else {
                assignment = ParseTypeExpression(false, false, is_proxy_class, identifier.GetValue());
            }

            if (assignment == nullptr) {
                // could not parse type expr
                return nullptr;
            }

            if (flags & IdentifierFlags::FLAG_GENERIC) {
                assignment = RC<AstTemplateExpression>(new AstTemplateExpression(
                    assignment,
                    generic_parameters,
                    nullptr,
                    assignment->GetLocation()
                ));
            }

            return RC<AstVariableDeclaration>(new AstVariableDeclaration(
                identifier.GetValue(),
                nullptr,
                assignment,
                {}, //template_expr_params,
                flags,
                token.GetLocation()
            ));
        }
    }

    return nullptr;
}

RC<AstTypeExpression> Parser::ParseTypeExpression(
    bool require_keyword,
    bool allow_identifier,
    bool is_proxy_class,
    std::string type_name
)
{
    const auto location = CurrentLocation();

    if (require_keyword && !ExpectKeyword(Keyword_class, true)) {
        return nullptr;
    }

    if (allow_identifier) {
        if (Token ident = Match(TK_IDENT, true)) {
            type_name = ident.GetValue();
        }
    }

    RC<AstPrototypeSpecification> base_specification;

    if (Match(TK_COLON, true)) {
        base_specification = ParsePrototypeSpecification();
    }

    // for hoisting, so functions can use later declared members
    Array<RC<AstVariableDeclaration>> member_functions;
    Array<RC<AstVariableDeclaration>> member_variables;

    Array<RC<AstVariableDeclaration>> static_functions;
    Array<RC<AstVariableDeclaration>> static_variables;

    auto current_access_specifier = Keyword::ToString(Keyword_private);

    if (Expect(TK_OPEN_BRACE, true)) {
        while (!Match(TK_CLOSE_BRACE, true)) {
            const SourceLocation location = CurrentLocation();
            Token specifier_token = Token::EMPTY;

            if ((specifier_token = MatchKeyword(Keyword_public, true))
                || (specifier_token = MatchKeyword(Keyword_private, true))
                || (specifier_token = MatchKeyword(Keyword_protected, true))) {

                // read ':'
                if (Expect(TK_COLON, true)) {
                    current_access_specifier = specifier_token.GetValue();
                }
            }

            IdentifierFlagBits flags = IdentifierFlags::FLAG_MEMBER;

            // read ident
            bool is_static = false,
                is_function = false,
                is_variable = false;

            if (MatchKeyword(Keyword_static, true)) {
                is_static = true;
            }

            // place rollback position here because ParseVariableDeclaration()
            // will handle everything. put keywords that ParseVariableDeclaration()
            // does /not/ handle, above.
            const SizeType position_before = m_token_stream->GetPosition();

            if (MatchKeyword(Keyword_let, true)) {
                is_variable = true;
            }

            if (MatchKeyword(Keyword_ref, true)) {
                is_variable = true;

                flags |= IdentifierFlags::FLAG_REF;
            }

            if (MatchKeyword(Keyword_const, true)) {
                is_variable = true;

                flags |= IdentifierFlags::FLAG_CONST;
            }
            
            if (MatchKeyword(Keyword_func, true)) {
                is_function = true;
            }

            //!Match(TK_STRING, false) || ExpectIdentifier(true, false);
            
            if (!MatchIdentifier(true, false) && !Match(TK_STRING, false)) {
                // error; unexpected token
                m_compilation_unit->GetErrorList().AddError(CompilerError(
                    LEVEL_ERROR,
                    Msg_unexpected_token,
                    m_token_stream->Peek().GetLocation(),
                    m_token_stream->Peek().GetValue()
                ));

                if (m_token_stream->HasNext()) {
                    m_token_stream->Next();
                }

                continue;
            }
            
            // read the identifier token
            Token identifier = Match(TK_STRING, false)
                ? m_token_stream->Next()
                : ExpectIdentifier(true, true);

            // read generic params after identifier

            RC<AstExpression> assignment;
            Array<RC<AstParameter>> generic_parameters;

            // check for generic
            if (MatchOperator("<", false)) {
                flags |= IdentifierFlags::FLAG_CONST | IdentifierFlags::FLAG_GENERIC;

                generic_parameters = ParseGenericParameters();
            }

            if (current_access_specifier == Keyword::ToString(Keyword_public)) {
                flags |= IdentifierFlags::FLAG_ACCESS_PUBLIC;
            } else if (current_access_specifier == Keyword::ToString(Keyword_private)) {
                flags |= IdentifierFlags::FLAG_ACCESS_PRIVATE;
            } else if (current_access_specifier == Keyword::ToString(Keyword_protected)) {
                flags |= IdentifierFlags::FLAG_ACCESS_PROTECTED;
            }

            // do not require declaration keyword for data members.
            // also, data members may be specifiers.
            // note: a variable may be declared with ANY name if it enquoted

            // if parentheses matched, it will be a function:
            /* type Whatever { 
                do_something() {
                // ...
                }
            } */
            if (!is_variable && (is_function || Match(TK_OPEN_PARENTH))) { // it is a member function
                Array<RC<AstParameter>> params;
                
                if (Match(TK_OPEN_PARENTH, true)) {
                    params = ParseFunctionParameters();
                    Expect(TK_CLOSE_PARENTH, true);
                }

                assignment = ParseFunctionExpression(false, params);

                if (assignment == nullptr) {
                    return nullptr;
                }

                if (flags & IdentifierFlags::FLAG_GENERIC) {
                    assignment = RC<AstTemplateExpression>(new AstTemplateExpression(
                        assignment,
                        generic_parameters,
                        nullptr,
                        assignment->GetLocation()
                    ));
                }

                RC<AstVariableDeclaration> member(new AstVariableDeclaration(
                    identifier.GetValue(),
                    nullptr, // prototype specification
                    assignment,
                    {}, //template_expr_params,
                    flags,
                    location
                ));

                if (is_static || is_proxy_class) { // <--- all methods for proxy classes are static
                    static_functions.PushBack(member);
                } else {
                    member_functions.PushBack(member);
                }
            } else {
                // rollback
                m_token_stream->SetPosition(position_before);

                if (auto member = ParseVariableDeclaration(
                    true, // allow keyword names
                    true, // allow quoted names
                    flags
                )) {
                    if (is_static) {
                        static_variables.PushBack(member);
                    } else {
                        member_variables.PushBack(member);
                    }
                } else {
                    break;
                }

                if (is_proxy_class) {
                    m_compilation_unit->GetErrorList().AddError(CompilerError(
                        LEVEL_ERROR,
                        Msg_proxy_class_may_only_contain_methods,
                        m_token_stream->Peek().GetLocation()
                    ));
                }
            }

            ExpectEndOfStmt();
            SkipStatementTerminators();
        }

        Array<RC<AstVariableDeclaration>> all_statics;
        all_statics.Reserve(static_variables.Size() + static_functions.Size());
        all_statics.Concat(std::move(static_variables));
        all_statics.Concat(std::move(static_functions));

        return RC<AstTypeExpression>(new AstTypeExpression(
            type_name,
            base_specification,
            member_variables,
            member_functions,
            all_statics,
            is_proxy_class,
            location
        ));
    }

    return nullptr;
}

RC<AstStatement> Parser::ParseEnumDefinition()
{
    if (Token token = ExpectKeyword(Keyword_enum, true)) {
        if (Token identifier = ExpectIdentifier(false, true)) {
            auto assignment = ParseEnumExpression(
                false,
                false,
                identifier.GetValue()
            );

            if (assignment == nullptr) {
                // could not parse type expr
                return nullptr;
            }

            return RC<AstVariableDeclaration>(new AstVariableDeclaration(
                identifier.GetValue(),
                nullptr, // prototype specification
                assignment,
                {}, //template_expr_params,
                IdentifierFlags::FLAG_CONST | IdentifierFlags::FLAG_ENUM,
                token.GetLocation()
            ));
        }
    }

    return nullptr;
}

RC<AstEnumExpression> Parser::ParseEnumExpression(
    bool require_keyword,
    bool allow_identifier,
    std::string enum_name
)
{
    const auto location = CurrentLocation();

    if (require_keyword && !ExpectKeyword(Keyword_enum, true)) {
        return nullptr;
    }

    if (allow_identifier) {
        if (Token ident = Match(TK_IDENT, true)) {
            enum_name = ident.GetValue();
        }
    }

    RC<AstPrototypeSpecification> underlying_type;

    if (Match(TK_COLON, true)) {
        // underlying type
        underlying_type = ParsePrototypeSpecification();
    }
    
    Array<EnumEntry> entries;

    if (Expect(TK_OPEN_BRACE, true)) {
        while (!Match(TK_CLOSE_BRACE, false)) {
            EnumEntry entry { };

            if (const Token ident = Expect(TK_IDENT, true)) {
                entry.name = ident.GetValue();
                entry.location = ident.GetLocation();
            } else {
                break;
            }

            if (const Token op = MatchOperator("=", true)) {
                entry.assignment = ParseExpression(true);
            }

            entries.PushBack(entry);

            while (Match(TK_NEWLINE, true));

            if (!Match(TK_CLOSE_BRACE, false)) {
                Expect(TK_COMMA, true);
            }
        }

        Expect(TK_CLOSE_BRACE, true);

        return RC<AstEnumExpression>(new AstEnumExpression(
            enum_name,
            entries,
            underlying_type,
            location
        ));
    }

    return nullptr;
}

RC<AstImport> Parser::ParseImport()
{
    if (ExpectKeyword(Keyword_import)) {
        if (MatchAhead(TK_STRING, 1)) {
            return ParseFileImport();
        } else if (MatchAhead(TK_IDENT, 1)) {
            return ParseModuleImport();
        }
    }

    return nullptr;
}

RC<AstExportStatement> Parser::ParseExportStatement()
{
    if (auto export_token = ExpectKeyword(Keyword_export, true)) {
        if (auto stmt = ParseStatement()) {
            return RC<AstExportStatement>(new AstExportStatement(
                stmt,
                export_token.GetLocation()
            ));
        }
    }

    return nullptr;
}

RC<AstFileImport> Parser::ParseFileImport()
{
    if (Token token = ExpectKeyword(Keyword_import, true)) {
        if (Token file = Expect(TK_STRING, true)) {
            RC<AstFileImport> result(new AstFileImport(
                file.GetValue(),
                token.GetLocation()
            ));

            return result;
        }
    }

    return nullptr;
}

RC<AstModuleImportPart> Parser::ParseModuleImportPart(bool allow_braces)
{
    const SourceLocation location = CurrentLocation();

    Array<RC<AstModuleImportPart>> parts;

    if (Token ident = Expect(TK_IDENT, true)) {
        if (Match(TK_DOUBLE_COLON, true)) {
            if (Match(TK_OPEN_BRACE, true)) {
                while (!Match(TK_CLOSE_BRACE, false)) {
                    RC<AstModuleImportPart> part = ParseModuleImportPart(false);

                    if (part == nullptr) {
                        return nullptr;
                    }

                    parts.PushBack(part);

                    if (!Match(TK_COMMA, true)) {
                        break;
                    }
                }

                Expect(TK_CLOSE_BRACE, true);
            } else {
                // match next
                RC<AstModuleImportPart> part = ParseModuleImportPart(true);

                if (part == nullptr) {
                    return nullptr;
                }

                parts.PushBack(part);
            }
        }

        return RC<AstModuleImportPart>(new AstModuleImportPart(
            ident.GetValue(),
            parts,
            location
        ));
    }

    return nullptr;
}

RC<AstModuleImport> Parser::ParseModuleImport()
{
    if (Token token = ExpectKeyword(Keyword_import, true)) {
        Array<RC<AstModuleImportPart>> parts;

        if (auto part = ParseModuleImportPart(false)) {
            parts.PushBack(part);

            return RC<AstModuleImport>(new AstModuleImport(
                parts,
                token.GetLocation()
            ));
        }
    }

    return nullptr;
}

RC<AstReturnStatement> Parser::ParseReturnStatement()
{
    const SourceLocation location = CurrentLocation();

    if (Token token = ExpectKeyword(Keyword_return, true)) {
        RC<AstExpression> expr;

        if (!Match(TK_SEMICOLON, true)) {
            expr = ParseExpression();
        }

        return RC<AstReturnStatement>(new AstReturnStatement(
            expr,
            location
        ));
    }

    return nullptr;
}

RC<AstExpression> Parser::ParseMetaProperty()
{
    const SourceLocation location = CurrentLocation();

    if (Token token = ExpectKeyword(Keyword_meta, true)) {
        Expect(TK_DOUBLE_COLON, true);

        Token ident = ExpectIdentifier(true, true);

        // @TODO refactor this to use ParseFunctionParameters() etc...
        // rather than having it hard-coded. makes it hard to add new features..

        if (ident.GetValue() == "query") {
            Expect(TK_OPEN_PARENTH, true);

            RC<AstString> query_command_name = ParseStringLiteral();
            if (query_command_name == nullptr) {
                return nullptr;
            }

            Expect(TK_COMMA, true);

            RC<AstExpression> term = ParseTerm();
            if (term == nullptr) {
                return nullptr;
            }

            Expect(TK_CLOSE_PARENTH, true);

            return RC<AstSymbolQuery>(new AstSymbolQuery(
                query_command_name->GetValue(),
                term,
                location
            ));
        }
    }

    return nullptr;
}

RC<AstSyntaxDefinition> Parser::ParseSyntaxDefinition()
{
    const SourceLocation location = CurrentLocation();

    if (Token token = ExpectKeyword(Keyword_syntax, true)) {
        const RC<AstString> syntax_string = ParseStringLiteral();
        
        if (!syntax_string) {
            return nullptr;
        }
        
        const Token op = ExpectOperator("=", true);
        
        if (!op) {
            return nullptr;
        }
        
        const RC<AstString> transform_string = ParseStringLiteral();

        if (!transform_string) {
            return nullptr;
        }

        return RC<AstSyntaxDefinition>(new AstSyntaxDefinition(
            syntax_string,
            transform_string,
            location
        ));
    }

    return nullptr;
}

} // namespace hyperion::compiler
