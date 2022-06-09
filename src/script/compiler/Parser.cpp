#include <script/compiler/Parser.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <util/utf8.hpp>
#include <util/string_util.h>

#include <memory>
#include <cstdlib>
#include <cstdio>
#include <sstream>
#include <iostream>

namespace hyperion {
namespace compiler {

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

    if (!Match(TK_NEWLINE, true) && !Match(TK_SEMICOLON, true)) {
        m_compilation_unit->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_expected_end_of_statement,
            location
        ));

        // skip until end of statement, end of line, or end of file.
        do {
            m_token_stream->Next();
        } while (m_token_stream->HasNext() &&
            !Match(TK_NEWLINE, true) &&
            !Match(TK_SEMICOLON, true));

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
        const std::vector<std::string> split = StringUtil::SplitPath(filepath);

        std::string real_filename = !split.empty()
            ? split.back()
            : filepath;

        real_filename = StringUtil::StripExtension(real_filename);

        std::shared_ptr<AstModuleDeclaration> module_ast(new AstModuleDeclaration(
            real_filename,
            SourceLocation(0, 0, filepath)
        ));

        // build up the module declaration with statements
        while (m_token_stream->HasNext() && !Match(TK_CLOSE_BRACE, false)) {
            // skip statement terminator tokens
            if (Match(TK_SEMICOLON, true) || Match(TK_NEWLINE, true)) {
                goto push_module;
            }

            // parse at top level, to allow for nested modules
            if (auto stmt = ParseStatement(true)) {
                module_ast->AddChild(stmt);
            }
        }

    push_module:
        m_ast_iterator->Push(module_ast);
    } else {
        // build up the module declaration with statements
        while (m_token_stream->HasNext() && !Match(TK_CLOSE_BRACE, false)) {
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

std::shared_ptr<AstStatement> Parser::ParseStatement(bool top_level)
{
    std::shared_ptr<AstStatement> res;

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
        } else if (MatchKeyword(Keyword_use, false)) {
            res = ParseDirective();
        } else if (MatchKeyword(Keyword_import, false)) {
            res = ParseImport();
        } else if (MatchKeyword(Keyword_export, false)) {
            res = ParseVariableDeclaration();
        } else if (MatchKeyword(Keyword_const, false)) {
            res = ParseVariableDeclaration();
        } else if (MatchKeyword(Keyword_func, false)) {
            if (MatchAhead(TK_IDENT, 1)) {
                res = ParseFunctionDefinition();
            } else {
                res = ParseFunctionExpression();
            }
        } else if (MatchKeyword(Keyword_type, false)) {
            res = ParseTypeDefinition();
        } else if (MatchKeyword(Keyword_alias, false)) {
            res = ParseAliasDeclaration();
        } else if (MatchKeyword(Keyword_mixin, false)) {
            res = ParseMixinDeclaration();
        } else if (MatchKeyword(Keyword_if, false)) {
            res = ParseIfStatement();
        } else if (MatchKeyword(Keyword_while, false)) {
            res = ParseWhileLoop();
        } else if (MatchKeyword(Keyword_for, false)) {
            res = ParseForLoop();
        } else if (MatchKeyword(Keyword_print, false)) {
            res = ParsePrintStatement();
        } else if (MatchKeyword(Keyword_try, false)) {
            res = ParseTryCatchStatement();
        } else if (MatchKeyword(Keyword_return, false)) {
            res = ParseReturnStatement();
        } else if (MatchKeyword(Keyword_yield, false)) {
            res = ParseYieldStatement();
        } else {
            res = ParseExpression();
        }
    } else if (Match(TK_IDENT, false) && (MatchAhead(TK_COLON, 1) || MatchAhead(TK_DEFINE, 1))) {
        res = ParseVariableDeclaration();
    } else if (Match(TK_OPEN_BRACE, false)) {
        res = ParseBlock();
    } else {
        std::shared_ptr<AstExpression> expr = ParseExpression(false);
        
        /*if (Match(TK_FAT_ARROW)) {
            expr = ParseActionExpression(expr);
        }*/

        res = expr;
    }

    if (res != nullptr && m_token_stream->HasNext()) {
        ExpectEndOfStmt();
    }

    return res;
}

std::shared_ptr<AstModuleDeclaration> Parser::ParseModuleDeclaration()
{
    if (Token module_decl = ExpectKeyword(Keyword_module, true)) {
        if (Token module_name = Expect(TK_IDENT, true)) {
            // expect open brace
            if (Expect(TK_OPEN_BRACE, true)) {
                std::shared_ptr<AstModuleDeclaration> module_ast(new AstModuleDeclaration(
                    module_name.GetValue(),
                    module_decl.GetLocation()
                ));

                // build up the module declaration with statements
                while (m_token_stream->HasNext() && !Match(TK_CLOSE_BRACE, false)) {
                    // skip statement terminator tokens
                    if (!Match(TK_SEMICOLON, true) && !Match(TK_NEWLINE, true)) {

                        // parse at top level, to allow for nested modules
                        module_ast->AddChild(ParseStatement(true));
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

std::shared_ptr<AstDirective> Parser::ParseDirective()
{
    if (Token token = ExpectKeyword(Keyword_use, true)) {
        if (Token ident = Expect(TK_IDENT, true)) {
            // the arguments will be held in an array expression
            std::vector<std::string> args;

            if (Match(TK_OPEN_BRACKET)) {
                if (Token token = Expect(TK_OPEN_BRACKET, true)) {
                    while (true) {
                        if (Match(TK_CLOSE_BRACKET, false)) {
                            break;
                        }

                        if (Token arg = Expect(TK_STRING, true)) {
                            args.push_back(arg.GetValue());
                        }

                        if (!Match(TK_COMMA, true)) {
                            break;
                        }
                    }

                    Expect(TK_CLOSE_BRACKET, true);
                }
            }

            return std::shared_ptr<AstDirective>(new AstDirective(
                ident.GetValue(),
                args,
                token.GetLocation()
            ));
        }
    }

    return nullptr;
}

std::shared_ptr<AstExpression> Parser::ParseTerm(bool override_commas,
    bool override_fat_arrows)
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

    std::shared_ptr<AstExpression> expr;

    if (Match(TK_OPEN_PARENTH)) {
        expr = ParseParentheses();
    } else if (Match(TK_OPEN_BRACKET)) {
        expr = ParseArrayExpression();
    }
#if ACE_ENABLE_BLOCK_EXPRESSIONS
    else if (Match(TK_OPEN_BRACE)) {
        expr = ParseBlockExpression();
    }
#endif
    else if (Match(TK_INTEGER)) {
        expr = ParseIntegerLiteral();
    } else if (Match(TK_FLOAT)) {
        expr = ParseFloatLiteral();
    } else if (Match(TK_STRING)) {
        expr = ParseStringLiteral();
    } else if (Match(TK_IDENT)) {
        if (MatchAhead(TK_DOUBLE_COLON, 1)) {
            expr = ParseModuleAccess();
        } else {
            expr = ParseIdentifier();
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
    } else if (Match(TK_OPERATOR)) {
        expr = ParseUnaryExpression();
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

    while (expr != nullptr &&
           (Match(TK_DOT) ||
            Match(TK_OPEN_BRACKET) ||
            Match(TK_OPEN_PARENTH) ||
            Match(TK_QUESTION_MARK) ||
            (!override_commas && Match(TK_COMMA)) ||
            (!override_fat_arrows && Match(TK_FAT_ARROW)) ||
            MatchKeyword(Keyword_has)))
    {
        if (Match(TK_DOT)) {
            expr = ParseMemberExpression(expr);
        }

        if (Match(TK_OPEN_BRACKET)) {
            expr = ParseArrayAccess(expr);
        }

        if (Match(TK_OPEN_PARENTH)) {
            expr = ParseCallExpression(expr);
        }

        if (Match(TK_QUESTION_MARK)) {
            expr = ParseTernaryExpression(expr);
        }

        if (!override_fat_arrows && Match(TK_FAT_ARROW)) {
            expr = ParseActionExpression(expr);
        }

        if (!override_commas && Match(TK_COMMA)) {
            expr = ParseTupleExpression(expr);
        }
        
        if (MatchKeyword(Keyword_has)) {
            expr = ParseHasExpression(expr);
        }
    }

    return expr;
}

std::shared_ptr<AstExpression> Parser::ParseParentheses()
{
    SourceLocation location = CurrentLocation();
    std::shared_ptr<AstExpression> expr;
    auto before_pos = m_token_stream->GetPosition();

    Expect(TK_OPEN_PARENTH, true);

    if (!Match(TK_CLOSE_PARENTH) && !Match(TK_IDENT) && !MatchKeyword(Keyword_self) && !MatchKeyword(Keyword_const)) {
        expr = ParseExpression(true);
        Expect(TK_CLOSE_PARENTH, true);
    } else {
        if (Match(TK_CLOSE_PARENTH, true)) {
            // if '()' found, it is a function with empty parameters
            // allow ParseFunctionParameters() to handle parentheses
            // RRrrrrrollback!
            m_token_stream->SetPosition(before_pos);
            expr = ParseFunctionExpression(false, ParseFunctionParameters());
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

                // function return type, or generator token
                if (Match(TK_RIGHT_ARROW) || MatchOperator("*")) {
                    found_function_token = true;
                }
                
                // go back to where it was before reading the ')' token
                m_token_stream->SetPosition(before);
            }

            if (found_function_token) {
                // go back to before open '(' found, 
                // to allow ParseFunctionParameters() to handle it
                m_token_stream->SetPosition(before_pos);

                // parse function parameters
                expr = ParseFunctionExpression(false, ParseFunctionParameters());
            } else {
                Expect(TK_CLOSE_PARENTH, true);

                if (Match(TK_OPEN_BRACE, true)) {
                    // if '{' found after ')', it is a function
                    m_token_stream->SetPosition(before_pos);
                    expr = ParseFunctionExpression(false, ParseFunctionParameters());
                }
            }
        }
    }

/*
    // if it's a function expression
    bool is_function_expr = false;

    std::vector<std::shared_ptr<AstParameter>> parameters;
    bool found_variadic = false;

    while (Match(Token::Token_comma, true)) {
        is_function_expr = true;

        Token param_token = Match(Token::Token_identifier, true);
        if (!param_token) {
            break;
        }

        // TODO make use of the type specification
        std::shared_ptr<AstTypeSpecification> type_spec;
        std::shared_ptr<AstTypeContractExpression> type_contract;

        // check if parameter type has been declared
        if (Match(Token::Token_colon, true)) {
            // type contracts are denoted by <angle brackets>
            if (MatchOperator(&Operator::operator_less, false)) {
                // parse type contracts
                type_contract = ParseTypeContract();
            } else {
                type_spec = ParseTypeSpecification();
            }
        }


        if (found_variadic) {
            // found another parameter after variadic
            m_compilation_unit->GetErrorList().AddError(
                CompilerError(LEVEL_ERROR, Msg_argument_after_varargs, param_token.GetLocation()));
        }

        bool is_variadic = false;
        if (Match(Token::Token_ellipsis, true)) {
            is_variadic = true;
            found_variadic = true;
        }

        auto param = std::shared_ptr<AstParameter>(
            new AstParameter(param_token.GetValue(), is_variadic, param_token.GetLocation()));

        if (type_contract) {
            param->SetTypeContract(type_contract);
        }

        parameters.push_back(param);
    }*/

    /*if (is_function_expr) {
        std::shared_ptr<AstTypeSpecification> return_type_spec;

        // return type specification
        if (Match(Token::Token_colon, true)) {
            // read return type for functions
            return_type_spec = ParseTypeSpecification();
        }

        if (auto block = ParseBlock()) {
            return std::shared_ptr<AstFunctionExpression>(
                new AstFunctionExpression(parameters, return_type_spec, block, location));
        }
    }*/

    return expr;
}

std::shared_ptr<AstInteger> Parser::ParseIntegerLiteral()
{
    if (Token token = Expect(TK_INTEGER, true)) {
        std::istringstream ss(token.GetValue());
        hyperion::aint32 value;
        ss >> value;

        return std::shared_ptr<AstInteger>(new AstInteger(
            value,
            token.GetLocation()
        ));
    }

    return nullptr;
}

std::shared_ptr<AstFloat> Parser::ParseFloatLiteral()
{
    if (Token token = Expect(TK_FLOAT, true)) {
        hyperion::afloat32 value = std::atof(token.GetValue().c_str());

        return std::shared_ptr<AstFloat>(new AstFloat(
            value,
            token.GetLocation()
        ));
    }

    return nullptr;
}

std::shared_ptr<AstString> Parser::ParseStringLiteral()
{
    if (Token token = Expect(TK_STRING, true)) {
        return std::shared_ptr<AstString>(new AstString(
            token.GetValue(),
            token.GetLocation()
        ));
    }

    return nullptr;
}

std::shared_ptr<AstIdentifier> Parser::ParseIdentifier(bool allow_keyword)
{
    if (Token token = ExpectIdentifier(allow_keyword, false)) {
        /*if (MatchAhead(TK_OPEN_PARENTH, 1)) {
            // function call
            return ParseFunctionCall(allow_keyword);
        }
        // allow identifiers with identifiers directly after to be used as functions
        // i.e: select x from y would be parsed as:
        //      select(x).from(y)
        else if (MatchAhead(TK_IDENT, 1) || MatchAhead(TK_STRING, 1)) {
            return ParseFunctionCallNoParams(allow_keyword);
        }
        // return a variable
        else {*/
            // read identifier token
            if (m_token_stream->HasNext()) {
                m_token_stream->Next();
            }

            // return variable
            return std::shared_ptr<AstVariable>(new AstVariable(
                token.GetValue(),
                token.GetLocation()
            ));
        //}
    }

    return nullptr;
}

std::shared_ptr<AstArgument> Parser::ParseArgument(std::shared_ptr<AstExpression> expr)
{
    SourceLocation location = CurrentLocation();

    bool is_named_arg = false;
    std::string arg_name;

    if (expr == nullptr) {
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

        expr = ParseExpression(true, true);
    }

    if (expr != nullptr) {
        return std::shared_ptr<AstArgument>(new AstArgument(
            expr,
            is_named_arg,
            arg_name,
            location
        ));
    } else {
        m_compilation_unit->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_illegal_expression,
            location
        ));

        return nullptr;
    }
}

std::shared_ptr<AstArgumentList> Parser::ParseArguments(bool require_parentheses,
    const std::shared_ptr<AstExpression> &expr)
{
    const SourceLocation location = CurrentLocation();

    std::vector<std::shared_ptr<AstArgument>> args;

    // if expr is not null, then add it as first argument
    if (expr != nullptr) {
        require_parentheses = false;

        args.push_back(ParseArgument(expr));

        // read any commas after
        if (!Match(TK_COMMA, true)) {
            // if no commas matched, return what we have now
            return std::shared_ptr<AstArgumentList>(new AstArgumentList(
                args,
                location
            ));
        }
    }

    if (require_parentheses) {
        Expect(TK_OPEN_PARENTH, true);
    }

    while (!require_parentheses || !Match(TK_CLOSE_PARENTH, false)) {
        if (auto arg = ParseArgument(nullptr)) {
            args.push_back(arg);

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

    return std::shared_ptr<AstArgumentList>(new AstArgumentList(
        args,
        location
    ));
}

std::shared_ptr<AstCallExpression> Parser::ParseCallExpression(std::shared_ptr<AstExpression> target)
{
    if (target != nullptr) {
        if (auto args = ParseArguments()) {
            return std::shared_ptr<AstCallExpression>(new AstCallExpression(
                target,
                args->GetArguments(),
                true, // allow 'self'
                target->GetLocation()
            ));
        }
    }

    return nullptr;
}

std::shared_ptr<AstModuleAccess> Parser::ParseModuleAccess()
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
        std::shared_ptr<AstExpression> expr;

        if (MatchAhead(TK_DOUBLE_COLON, 1)) {
            expr = ParseModuleAccess();
        } else {
            expr = ParseIdentifier(true);
        }

        if (expr != nullptr) {
            return std::shared_ptr<AstModuleAccess>(new AstModuleAccess(
                global_module_access
                    ? hyperion::compiler::Config::global_module_name
                    : token.GetValue(),
                expr,
                location
            ));
        }
    }

    return nullptr;
}

std::shared_ptr<AstModuleProperty> Parser::ParseModuleProperty()
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

    return std::shared_ptr<AstModuleProperty>(new AstModuleProperty(
        ident.GetValue(),
        token.GetLocation()
    ));
}

std::shared_ptr<AstMember> Parser::ParseMemberExpression(std::shared_ptr<AstExpression> target)
{
    Expect(TK_DOT, true);

    // allow quoted strings as data member names
    Token ident = Match(TK_STRING, false)
        ? m_token_stream->Next()
        : ExpectIdentifier(true, true);

    if (ident) {
        return std::shared_ptr<AstMember>(new AstMember(
            ident.GetValue(),
            target,
            ident.GetLocation()
        ));
    }

    return nullptr;
}

std::shared_ptr<AstArrayAccess> Parser::ParseArrayAccess(std::shared_ptr<AstExpression> target)
{
    if (Token token = Expect(TK_OPEN_BRACKET, true)) {
        auto expr = ParseExpression();
        Expect(TK_CLOSE_BRACKET, true);

        if (expr != nullptr) {
            return std::shared_ptr<AstArrayAccess>(new AstArrayAccess(
                target,
                expr,
                token.GetLocation()
            ));
        }
    }

    return nullptr;
}

std::shared_ptr<AstHasExpression> Parser::ParseHasExpression(std::shared_ptr<AstExpression> target)
{
    if (Token token = ExpectKeyword(Keyword_has, true)) {
        if (Token field = Expect(TK_STRING, true)) {
            return std::shared_ptr<AstHasExpression>(new AstHasExpression(
                target,
                field.GetValue(),
                target->GetLocation()
            ));
        }
    }

    return nullptr;
}

std::shared_ptr<AstActionExpression> Parser::ParseActionExpression(std::shared_ptr<AstExpression> expr)
{
    std::vector<std::shared_ptr<AstArgument>> actions;

    if (auto action = ParseArgument(expr)) {
        actions.push_back(action);

        // parse more actions
        while (Match(TK_COMMA, true)) {
            if (auto action = ParseArgument(nullptr)) {
                actions.push_back(action);
            } else {
                break;
            }
        }

        if (Expect(TK_FAT_ARROW, true)) {
            if (auto target = ParseExpression(true, true)) {
                return std::shared_ptr<AstActionExpression>(new AstActionExpression(
                    actions,
                    target,
                    target->GetLocation()
                ));
            }
        }
    }

    return nullptr;
}

std::shared_ptr<AstNewExpression> Parser::ParseNewExpression()
{
    if (Token token = ExpectKeyword(Keyword_new, true)) {
        if (auto type_spec = ParseTypeSpecification()) {
            std::shared_ptr<AstArgumentList> arg_list;

            if (Match(TK_OPEN_PARENTH, false)) {
                // parse args
                arg_list = ParseArguments();
            }

            return std::shared_ptr<AstNewExpression>(new AstNewExpression(
                type_spec,
                arg_list,
                token.GetLocation()
            ));
        }
    }

    return nullptr;
}

std::shared_ptr<AstTrue> Parser::ParseTrue()
{
    if (Token token = ExpectKeyword(Keyword_true, true)) {
        return std::shared_ptr<AstTrue>(new AstTrue(
            token.GetLocation()
        ));
    }

    return nullptr;
}

std::shared_ptr<AstFalse> Parser::ParseFalse()
{
    if (Token token = ExpectKeyword(Keyword_false, true)) {
        return std::shared_ptr<AstFalse>(new AstFalse(
            token.GetLocation()
        ));
    }

    return nullptr;
}

std::shared_ptr<AstNil> Parser::ParseNil()
{
    if (Token token = ExpectKeyword(Keyword_null, true)) {
        return std::shared_ptr<AstNil>(new AstNil(
            token.GetLocation()
        ));
    }

    return nullptr;
}

std::shared_ptr<AstBlock> Parser::ParseBlock()
{
    if (Token token = Expect(TK_OPEN_BRACE, true)) {
        std::shared_ptr<AstBlock> block(new AstBlock(
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

std::shared_ptr<AstBlockExpression> Parser::ParseBlockExpression()
{
    if (auto block = ParseBlock()) {
        return std::shared_ptr<AstBlockExpression>(new AstBlockExpression(
            block,
            block->GetLocation()
        ));
    }

    return nullptr;
}

std::shared_ptr<AstIfStatement> Parser::ParseIfStatement()
{
    if (Token token = ExpectKeyword(Keyword_if, true)) {
        std::shared_ptr<AstExpression> conditional;
        if (!(conditional = ParseExpression())) {
            return nullptr;
        }

        std::shared_ptr<AstBlock> block;
        if (!(block = ParseBlock())) {
            return nullptr;
        }

        std::shared_ptr<AstBlock> else_block = nullptr;
        // parse else statement if the "else" keyword is found
        if (Token else_token = MatchKeyword(Keyword_else, true)) {
            // check for "if" keyword for else-if
            if (MatchKeyword(Keyword_if, false)) {
                else_block = std::shared_ptr<AstBlock>(new AstBlock(
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

        return std::shared_ptr<AstIfStatement>(new AstIfStatement(
            conditional,
            block,
            else_block,
            token.GetLocation()
        ));
    }

    return nullptr;
}

std::shared_ptr<AstWhileLoop> Parser::ParseWhileLoop()
{
    if (Token token = ExpectKeyword(Keyword_while, true)) {
        std::shared_ptr<AstExpression> conditional;
        if (!(conditional = ParseExpression())) {
            return nullptr;
        }

        std::shared_ptr<AstBlock> block;
        if (!(block = ParseBlock())) {
            return nullptr;
        }

        return std::shared_ptr<AstWhileLoop>(new AstWhileLoop(
            conditional,
            block,
            token.GetLocation()
        ));
    }

    return nullptr;
}

std::shared_ptr<AstStatement> Parser::ParseForLoop()
{
    SourceLocation location = CurrentLocation();
    const auto before_pos = m_token_stream->GetPosition();

    // use contextual hints (`in` vs `;` to determine if it is a range loop, or c-style for loop)
    if (Token token = ExpectKeyword(Keyword_for, true)) {
        if (!ParseVariableDeclaration()) { // needs a variable declaration
            return nullptr;
        }

        if (Match(TK_SEMICOLON)) {
            // not implemented (yet)
            m_compilation_unit->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_unexpected_token,
                m_token_stream->Peek().GetLocation(),
                m_token_stream->Peek().GetValue()
            ));

            return nullptr;
        }

        // rollback
        m_token_stream->SetPosition(before_pos);

        // parse as for-range loop
        return ParseForRangeLoop();
    }
}

std::shared_ptr<AstForRangeLoop> Parser::ParseForRangeLoop()
{
    SourceLocation location = CurrentLocation();
    const auto before_pos = m_token_stream->GetPosition();

    // use contextual hints (`in` vs `;` to determine if it is a range loop, or c-style for loop)
    if (Token token = ExpectKeyword(Keyword_for, true)) {
        auto decl = ParseVariableDeclaration();

        if (decl == nullptr) {
            return nullptr;
        }

        // declaration cannot have an assignment in for-range loops
        if (decl->GetAssignment() != nullptr) {
            m_compilation_unit->GetErrorList().AddError(CompilerError(
                LEVEL_ERROR,
                Msg_for_range_loop_illegal_assignment,
                m_token_stream->Peek().GetLocation(),
                decl->GetName()
            ));

            return nullptr;
        }

        // expect 'in' keyword

        if (!ExpectKeyword(Keyword_in, true)) {
            return nullptr;
        }

        // the Visit() call of AstForRangeLoop overwrites the assignment to Begin() call

        // parse the expression, what we will be looping over
        auto expr = ParseExpression();

        if (expr == nullptr) {
            return nullptr;
        }

        // parse the block
        auto block = ParseBlock();

        if (block == nullptr) {
            return nullptr;
        }

        return std::make_shared<AstForRangeLoop>(
            decl,
            expr,
            block,
            location
        );
    }
}

std::shared_ptr<AstPrintStatement> Parser::ParsePrintStatement()
{
    if (Token token = ExpectKeyword(Keyword_print, true)) {
        if (auto arg_list = ParseArguments(false)) {
            return std::shared_ptr<AstPrintStatement>(new AstPrintStatement(
                arg_list,
                token.GetLocation()
            ));
        }
    }

    return nullptr;
}

std::shared_ptr<AstTryCatch> Parser::ParseTryCatchStatement()
{
    if (Token token = ExpectKeyword(Keyword_try, true)) {
        std::shared_ptr<AstBlock> try_block(ParseBlock());
        std::shared_ptr<AstBlock> catch_block(nullptr);

        if (ExpectKeyword(Keyword_catch, true)) {
            // TODO: Add exception argument
            catch_block = ParseBlock();
        }

        if (try_block != nullptr && catch_block != nullptr) {
            return std::shared_ptr<AstTryCatch>(new AstTryCatch(
                try_block,
                catch_block,
                token.GetLocation()
            ));
        }
    }

    return nullptr;
}

std::shared_ptr<AstExpression> Parser::ParseBinaryExpression(int expr_prec,
    std::shared_ptr<AstExpression> left)
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

            left = std::shared_ptr<AstBinaryExpression>(new AstBinaryExpression(
                left,
                right,
                op,
                token.GetLocation()
            ));
        }
    }

    return nullptr;
}

std::shared_ptr<AstExpression> Parser::ParseUnaryExpression()
{
    // read the operator token
    if (Token token = Expect(TK_OPERATOR, true)) {
        const Operator *op = nullptr;
        if (Operator::IsUnaryOperator(token.GetValue(), op)) {
            if (auto term = ParseTerm()) {
                return std::shared_ptr<AstUnaryExpression>(new AstUnaryExpression(
                    term,
                    op,
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

std::shared_ptr<AstExpression> Parser::ParseTernaryExpression(std::shared_ptr<AstExpression> conditional)
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

        return std::shared_ptr<AstTernaryExpression>(new AstTernaryExpression(
            conditional,
            true_expr,
            false_expr,
            conditional->GetLocation()
        ));
    }

    return nullptr;
}

std::shared_ptr<AstExpression> Parser::ParseExpression(bool override_commas,
    bool override_fat_arrows)
{
    if (auto term = ParseTerm(override_commas, override_fat_arrows)) {
        if (Match(TK_OPERATOR, false)) {
            if (auto bin_expr = ParseBinaryExpression(0, term)) {
                term = bin_expr;
            } else {
                return nullptr;
            }
        }

        return term;
    }

    return nullptr;
}

std::shared_ptr<AstTypeSpecification> Parser::ParseTypeSpecification()
{
    if (Token left = Expect(TK_IDENT, true)) {
        std::string left_name = left.GetValue();

        std::shared_ptr<AstTypeSpecification> right;
        // module access of type
        if (Match(TK_DOUBLE_COLON, true)) {
            // read next part
            right = ParseTypeSpecification();
        }

        // check generics
        std::vector<std::shared_ptr<AstTypeSpecification>> generic_params;
        if (MatchOperator("<", true)) {
            do {
                if (std::shared_ptr<AstTypeSpecification> generic_param = ParseTypeSpecification()) {
                    generic_params.push_back(generic_param);
                }
            } while (Match(TK_COMMA, true));

            ExpectOperator(">", true);
        }

        while (Match(TK_OPEN_BRACKET) || Match(TK_QUESTION_MARK)) {
            // question mark at the end of a type is syntactical sugar for `Maybe(T)`
            if (Match(TK_QUESTION_MARK, true)) {
                std::shared_ptr<AstTypeSpecification> inner(new AstTypeSpecification(
                    left_name,
                    generic_params,
                    right,
                    left.GetLocation()
                ));

                left_name = BuiltinTypes::MAYBE->GetName();
                generic_params = { inner };
                right = nullptr;
            } else if (Match(TK_OPEN_BRACKET, true)) {
                // array braces at the end of a type are syntactical sugar for `Array(T)`
                std::shared_ptr<AstTypeSpecification> inner(new AstTypeSpecification(
                    left_name,
                    generic_params,
                    right,
                    left.GetLocation()
                ));

                left_name = BuiltinTypes::ARRAY->GetName();
                generic_params = { inner };
                right = nullptr;

                Expect(TK_CLOSE_BRACKET, true);
            }
        }

        return std::shared_ptr<AstTypeSpecification>(new AstTypeSpecification(
            left_name,
            generic_params,
            right,
            left.GetLocation()
        ));
    }

    return nullptr;
}

#if 0
std::shared_ptr<AstTypeContractExpression> Parser::ParseTypeContract()
{
    std::shared_ptr<AstTypeContractExpression> expr;

    // we have to use ExpectOperator for angle brackets
    if (Token token = ExpectOperator(&Operator::operator_less, true)) {
        expr = ParseTypeContractExpression();
    }

    ExpectOperator(&Operator::operator_greater, true);

    return expr;
}

std::shared_ptr<AstTypeContractExpression> Parser::ParseTypeContractExpression()
{
    if (auto term = ParseTypeContractTerm()) {
        if (Match(TK_OPERATOR, false)) {
            if (auto expr = ParseTypeContractBinaryExpression(0, term)) {
                term = expr;
            } else {
                return nullptr;
            }
        }

        return term;
    }

    return nullptr;
}


std::shared_ptr<AstTypeContractExpression> Parser::ParseTypeContractTerm()
{
    if (Token contract_prop = Expect(TK_IDENT, true)) {
        // read the contract parameter here
        return std::shared_ptr<AstTypeContractTerm>(new AstTypeContractTerm(
            contract_prop.GetValue(), ParseTypeSpecification(), contract_prop.GetLocation()));
    }

    return nullptr;
}

std::shared_ptr<AstTypeContractExpression> Parser::ParseTypeContractBinaryExpression(int expr_prec,
    std::shared_ptr<AstTypeContractExpression> left)
{
    while (true) {
        // check for end of contract
        if (MatchOperator(&Operator::operator_greater, false)) {
            return left;
        }

        // get precedence
        const Operator *op = nullptr;
        int precedence = OperatorPrecedence(op);
        if (precedence < expr_prec) {
            return left;
        }

        // read the operator token
        Token token = Expect(TK_OPERATOR, false);
        Token token_op = MatchOperator(&Operator::operator_bitwise_or, true);
        if (!token) {
            return nullptr;
        }
        if (!token_op) {
            // try and operator
            token_op = MatchOperator(&Operator::operator_bitwise_and, true);
        }
        if (!token_op) {
            CompilerError error(LEVEL_ERROR, Msg_invalid_type_contract_operator, token.GetLocation(), token.GetValue());
            m_compilation_unit->GetErrorList().AddError(error);
            return nullptr;
        }

        auto right = ParseTypeContractTerm();
        if (!right) {
            return nullptr;
        }

        // next part of expression's precedence
        const Operator *next_op = nullptr;
        int next_prec = OperatorPrecedence(next_op);
        if (precedence < next_prec) {
            right = ParseTypeContractBinaryExpression(precedence + 1, right);
            if (!right) {
                return nullptr;
            }
        }

        left = std::shared_ptr<AstTypeContractBinaryExpression>(
            new AstTypeContractBinaryExpression(left, right, op, token.GetLocation()));
    }

    return nullptr;
}

#endif

std::shared_ptr<AstExpression> Parser::ParseAssignment()
{
    if (MatchOperator("=", true) || Match(TK_DEFINE, true)) {
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

std::shared_ptr<AstVariableDeclaration> Parser::ParseVariableDeclaration(bool allow_keyword_names,
    bool allow_quoted_names)
{
    const SourceLocation location = CurrentLocation();

    IdentifierFlagBits flags = IdentifierFlags::FLAG_NONE;

    while (Match(TK_KEYWORD)) {
        if (MatchKeyword(Keyword_const, true)) {
            flags |= IdentifierFlags::FLAG_CONST;

            continue;
        }

        if (MatchKeyword(Keyword_export, true)) {
            flags |= IdentifierFlags::FLAG_EXPORT;

            continue;
        }

        // unexpected keyword
        m_compilation_unit->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_unexpected_token,
            m_token_stream->Peek().GetLocation(),
            m_token_stream->Peek().GetValue()
        ));

        if (m_token_stream->HasNext()) {
            m_token_stream->Next();
        }
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
        std::shared_ptr<AstTypeSpecification> type_spec;

        if (Match(TK_COLON, true)) {
            // read object type
            type_spec = ParseTypeSpecification();
        }

        const std::shared_ptr<AstExpression> assignment = ParseAssignment();

        return std::shared_ptr<AstVariableDeclaration>(new AstVariableDeclaration(
            identifier.GetValue(),
            type_spec,
            assignment,
            flags,
            location
        ));
    }

    return nullptr;
}

std::shared_ptr<AstFunctionDefinition> Parser::ParseFunctionDefinition(bool require_keyword)
{
    const SourceLocation location = CurrentLocation();

    if (require_keyword) {
        if (!ExpectKeyword(Keyword_func, true)) {
            return nullptr;
        }
    } else {
        // match and read in the case that it is found
        MatchKeyword(Keyword_func, true);
    }

    if (Token identifier = Expect(TK_IDENT, true)) {
        if (auto expr = ParseFunctionExpression(false, ParseFunctionParameters())) {
            return std::shared_ptr<AstFunctionDefinition>(new AstFunctionDefinition(
                identifier.GetValue(),
                expr,
                location
            ));
        }
    }

    return nullptr;
}

std::shared_ptr<AstFunctionExpression> Parser::ParseFunctionExpression(
    bool require_keyword,
    std::vector<std::shared_ptr<AstParameter>> params,
    bool is_async,
    bool is_pure)
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
            params = ParseFunctionParameters();
        }

        std::shared_ptr<AstTypeSpecification> type_spec;

        if (Match(TK_RIGHT_ARROW, true)) {
            // read return type for functions
            type_spec = ParseTypeSpecification();
        }

        bool is_generator = false;

        // TEMPORARY: for now, use an asterisk before curly braces to signify
        // generator - i'd like to just use 'yield' to automatically do this,
        // but without some hassle or hacking it's not an easy fix right now
        if (MatchOperator("*", true)) {
            is_generator = true;
        }

        // parse function block
        if (auto block = ParseBlock()) {
            return std::shared_ptr<AstFunctionExpression>(new AstFunctionExpression(
                params,
                type_spec,
                block,
                is_async,
                is_pure,
                is_generator,
                location
            ));
        }
    }

    return nullptr;
}

std::shared_ptr<AstArrayExpression> Parser::ParseArrayExpression()
{
    if (Token token = Expect(TK_OPEN_BRACKET, true)) {
        std::vector<std::shared_ptr<AstExpression>> members;

        do {
            if (Match(TK_CLOSE_BRACKET, false)) {
                break;
            }

            if (auto expr = ParseExpression(true)) {
                members.push_back(expr);
            }
        } while (Match(TK_COMMA, true));

        Expect(TK_CLOSE_BRACKET, true);

        return std::shared_ptr<AstArrayExpression>(new AstArrayExpression(
            members,
            token.GetLocation()
        ));
    }

    return nullptr;
}

std::shared_ptr<AstTupleExpression> Parser::ParseTupleExpression(
    const std::shared_ptr<AstExpression> &expr)
{
    const SourceLocation location = CurrentLocation();

    std::vector<std::shared_ptr<AstArgument>> args;

    // if expr is not null, then add it as first argument
    if (expr != nullptr) {
        args.push_back(ParseArgument(expr));

        // read any commas after
        if (!Match(TK_COMMA, true)) {
            // if no commas matched, return what we have now
            return std::shared_ptr<AstTupleExpression>(new AstTupleExpression(
                args,
                location
            ));
        }
    }

    do {
        if (auto arg = ParseArgument(nullptr)) {
            args.push_back(arg);
        } else {
            return nullptr;
        }
    } while (Match(TK_COMMA, true));

    return std::shared_ptr<AstTupleExpression>(new AstTupleExpression(
        args,
        location
    ));
}

std::shared_ptr<AstExpression> Parser::ParseAsyncExpression()
{
    if (Token token = ExpectKeyword(Keyword_async, true)) {
        MatchKeyword(Keyword_func, true); // skip 'function' keyword if found
        // for now, only functions are supported.
        return ParseFunctionExpression(
            false,
            ParseFunctionParameters(),
            true,
            false
        );
    }

    return nullptr;
}

std::shared_ptr<AstExpression> Parser::ParsePureExpression()
{
    if (Token token = ExpectKeyword(Keyword_pure, true)) {
        MatchKeyword(Keyword_func, true); // skip 'function' keyword if found
        return ParseFunctionExpression(
            false,
            ParseFunctionParameters(),
            false,
            true
        );
    }

    return nullptr;
}

std::shared_ptr<AstExpression> Parser::ParseImpureExpression()
{
    if (Token token = ExpectKeyword(Keyword_impure, true)) {
        MatchKeyword(Keyword_func, true); // skip 'function' keyword if found
        return ParseFunctionExpression(
            false,
            ParseFunctionParameters(),
            false,
            false
        );
    }

    return nullptr;
}

std::shared_ptr<AstExpression> Parser::ParseValueOfExpression()
{
    if (Token token = ExpectKeyword(Keyword_valueof, true)) {
        std::shared_ptr<AstExpression> expr;

        if (!MatchAhead(TK_DOUBLE_COLON, 1)) {
            Token ident = Expect(TK_IDENT, true);
            expr.reset(new AstVariable(
                ident.GetValue(),
                token.GetLocation()
            ));
        } else {
            do {
                Token ident = Expect(TK_IDENT, true);
                expr.reset(new AstModuleAccess(
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

std::shared_ptr<AstTypeOfExpression> Parser::ParseTypeOfExpression()
{
    const SourceLocation location = CurrentLocation();
    
    if (Token token = ExpectKeyword(Keyword_typeof, true)) {
        SourceLocation expr_location = CurrentLocation();
        if (auto term = ParseTerm()) {
            return std::shared_ptr<AstTypeOfExpression>(new AstTypeOfExpression(
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

std::vector<std::shared_ptr<AstParameter>> Parser::ParseFunctionParameters()
{
    std::vector<std::shared_ptr<AstParameter>> parameters;

    if (Match(TK_OPEN_PARENTH, true)) {
        bool found_variadic = false;
        bool keep_reading = true;

        while (keep_reading) {
            Token token = Token::EMPTY;

            if (Match(TK_CLOSE_PARENTH, false)) {
                keep_reading = false;
                break;
            }

            bool is_const = false;

            if (MatchKeyword(Keyword_const, true)) {
                is_const = true;
            }
            
            if ((token = MatchKeyword(Keyword_self, true)) || (token = Expect(TK_IDENT, true)))
            {
                std::shared_ptr<AstTypeSpecification> type_spec;
                std::shared_ptr<AstExpression> default_param;

                // check if parameter type has been declared
                if (Match(TK_COLON, true)) {
                    type_spec = ParseTypeSpecification();
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

                parameters.push_back(std::shared_ptr<AstParameter>(new AstParameter(
                    token.GetValue(),
                    type_spec,
                    default_param,
                    is_variadic,
                    is_const,
                    token.GetLocation()
                )));

                if (!Match(TK_COMMA, true)) {
                    keep_reading = false;
                }
            } else {
                keep_reading = false;
            }
        }

        Expect(TK_CLOSE_PARENTH, true);
    }

    return parameters;
}

std::shared_ptr<AstStatement> Parser::ParseTypeDefinition()
{
    if (Token token = ExpectKeyword(Keyword_type, true)) {

        // type names may not be a keyword
        if (Token identifier = ExpectIdentifier(false, true)) {
            std::vector<std::string> generic_params;
            std::vector<std::shared_ptr<AstVariableDeclaration>> members;
            std::vector<std::shared_ptr<AstEvent>> events;

            // parse generic parameters after '<'
            if (MatchOperator("<", true)) {
                while (Token ident = ExpectIdentifier(false, true)) {
                    generic_params.push_back(ident.GetValue());
                    if (!Match(TK_COMMA, true)) {
                        break;
                    }
                }
                ExpectOperator(">", true);
            }

            // check type alias
            if (MatchOperator("=", true)) {
                if (auto aliasee = ParseTypeSpecification()) {
                    return std::shared_ptr<AstTypeAlias>(new AstTypeAlias(
                        identifier.GetValue(), 
                        aliasee,
                        identifier.GetLocation()
                    ));
                } else {
                    return nullptr;
                }
            }

            if (Expect(TK_OPEN_BRACE, true)) {
                while (!Match(TK_CLOSE_BRACE, true)) {
                    // check for events
                    if (Token on_token = MatchKeyword(Keyword_on, true)) {
                        std::shared_ptr<AstConstant> key_constant;

                        if (MatchKeyword(Keyword_true)) {
                            key_constant = ParseTrue();
                        } else if (MatchKeyword(Keyword_false)) {
                            key_constant = ParseFalse();
                        } else if (MatchKeyword(Keyword_null)) {
                            key_constant = ParseNil();
                        } else if (Match(TK_STRING)) {
                            key_constant = ParseStringLiteral();
                        } else if (Match(TK_INTEGER)) {
                            key_constant = ParseIntegerLiteral();
                        } else if (Match(TK_FLOAT)) {
                            key_constant = ParseFloatLiteral();
                        } 

                        if (key_constant != nullptr) {
                            // expect =>
                            if (Expect(TK_FAT_ARROW, true)) {
                                // read function expression
                                MatchKeyword(Keyword_func, true); // skip 'function' keyword if found
                                if (auto event_trigger = ParseFunctionExpression(false, ParseFunctionParameters())) {
                                    events.push_back(std::shared_ptr<AstConstantEvent>(new AstConstantEvent(
                                        key_constant,
                                        event_trigger,
                                        on_token.GetLocation()
                                    )));
                                }
                            }
                        }
                    } else if (MatchIdentifier(true, false) || Match(TK_STRING, false)) {
                        // do not require declaration keyword for data members.
                        // also, data members may be keywords.
                        // note: a variable may be declared with ANY name if it enquoted

                        // if parentheses matched, it will be a function:
                        /* type Whatever { 
                             do_something() {
                               // ...
                             }
                           } */
                        if (MatchAhead(TK_OPEN_PARENTH, 1)) {
                            // read the identifier token
                            Token identifier = Match(TK_STRING, false)
                                ? m_token_stream->Next()
                                : ExpectIdentifier(true, true);

                            MatchKeyword(Keyword_func, true); // skip 'function' keyword if found
                            if (auto expr = ParseFunctionExpression(false, ParseFunctionParameters())) {
                                // first, read the identifier
                                std::shared_ptr<AstVariableDeclaration> member(new AstVariableDeclaration(
                                    identifier.GetValue(),
                                    nullptr,
                                    expr,
                                    IdentifierFlags::FLAG_CONST, // (free functions are also implicitly const,
                                                                 // so set the member to be const as well for consistency)
                                    identifier.GetLocation()
                                ));

                                members.push_back(member);
                            }
                        } else {
                            if (auto member = ParseVariableDeclaration(
                                true, // allow keyword names
                                true // allow quoted names
                            )) {
                                members.push_back(member);
                            } else {
                                break;
                            }
                        }
                    } else {
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
                    }

                    ExpectEndOfStmt();
                    SkipStatementTerminators();
                }

                return std::shared_ptr<AstTypeDefinition>(new AstTypeDefinition(
                    identifier.GetValue(), 
                    generic_params,
                    members,
                    events,
                    token.GetLocation()
                ));
            }
        }
    }

    return nullptr;
}

std::shared_ptr<AstAliasDeclaration> Parser::ParseAliasDeclaration()
{
    const Token token = ExpectKeyword(Keyword_alias, true);
    if (!token) {
        return nullptr;
    }

    const Token ident = Expect(TK_IDENT, true);
    if (!ident) {
        return nullptr;
    }

    const Token op = ExpectOperator("=", true);
    if (!op) {
        return nullptr;
    }

    auto expr = ParseExpression();
    if (!expr) {
        return nullptr;
    }

    return std::shared_ptr<AstAliasDeclaration>(new AstAliasDeclaration(
        ident.GetValue(),
        expr,
        token.GetLocation()
    ));
}

std::shared_ptr<AstMixinDeclaration> Parser::ParseMixinDeclaration()
{
    const Token token = ExpectKeyword(Keyword_mixin, true);
    if (!token) {
        return nullptr;
    }

    const Token ident = ExpectIdentifier(true, true);
    if (!ident) {
        return nullptr;
    }

    const Token op = ExpectOperator("=", true);
    if (!op) {
        return nullptr;
    }

    // parse the mixin-expr string
    const Token mixin_expr = Expect(TK_STRING, true);
    if (!mixin_expr) {
        return nullptr;
    }

    return std::shared_ptr<AstMixinDeclaration>(new AstMixinDeclaration(
        ident.GetValue(),
        mixin_expr.GetValue(),
        ident.GetLocation()
    ));
}

std::shared_ptr<AstImport> Parser::ParseImport()
{
    if (ExpectKeyword(Keyword_import, true)) {
        if (Match(TK_STRING, false)) {
            return ParseFileImport();
        } else if (Match(TK_IDENT, false)) {
            return ParseModuleImport();
        }
    }

    return nullptr;
}

std::shared_ptr<AstFileImport> Parser::ParseFileImport()
{
    const SourceLocation location = CurrentLocation();

    if (Token file = Expect(TK_STRING, true)) {
        std::shared_ptr<AstFileImport> result(new AstFileImport(
            file.GetValue(),
            location
        ));

        return result;
    }

    return nullptr;
}

std::shared_ptr<AstModuleImportPart> Parser::ParseModuleImportPart(bool allow_braces)
{
    const SourceLocation location = CurrentLocation();

    std::vector<std::shared_ptr<AstModuleImportPart>> parts;

    if (Token ident = Expect(TK_IDENT, true)) {
        if (Match(TK_DOUBLE_COLON, true)) {
            if (Match(TK_OPEN_BRACE, true)) {
                while (!Match(TK_CLOSE_BRACE, false)) {
                    std::shared_ptr<AstModuleImportPart> part = ParseModuleImportPart(false);

                    if (part == nullptr) {
                        return nullptr;
                    }

                    parts.push_back(part);

                    if (!Match(TK_COMMA, true)) {
                        break;
                    }
                }

                Expect(TK_CLOSE_BRACE, true);
            } else {
                // match next
                std::shared_ptr<AstModuleImportPart> part = ParseModuleImportPart(true);

                if (part == nullptr) {
                    return nullptr;
                }

                parts.push_back(part);
            }
        }

        return std::shared_ptr<AstModuleImportPart>(new AstModuleImportPart(
            ident.GetValue(),
            parts,
            location
        ));
    }

    return nullptr;
}

std::shared_ptr<AstModuleImport> Parser::ParseModuleImport()
{
    const SourceLocation location = CurrentLocation();

    std::vector<std::shared_ptr<AstModuleImportPart>> parts;

    if (auto part = ParseModuleImportPart(false)) {
        parts.push_back(part);

        return std::shared_ptr<AstModuleImport>(new AstModuleImport(
            parts,
            location
        ));
    }

    return nullptr;
}

std::shared_ptr<AstReturnStatement> Parser::ParseReturnStatement()
{
    const SourceLocation location = CurrentLocation();

    if (Token token = ExpectKeyword(Keyword_return, true)) {
        if (auto expr = ParseExpression()) {
            return std::shared_ptr<AstReturnStatement>(new AstReturnStatement(
                expr,
                location
            ));
        }
    }

    return nullptr;
}

std::shared_ptr<AstYieldStatement> Parser::ParseYieldStatement()
{
    const SourceLocation location = CurrentLocation();

    if (Token token = ExpectKeyword(Keyword_yield, true)) {
        if (auto expr = ParseExpression()) {
            return std::shared_ptr<AstYieldStatement>(new AstYieldStatement(
                expr,
                location
            ));
        }
    }

    return nullptr;
}

} // namespace compiler
} // namespace hyperion
