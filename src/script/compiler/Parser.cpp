#include <script/compiler/Parser.hpp>
#include <script/compiler/Configuration.hpp>
#include <script/compiler/Keywords.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <util/utf8.hpp>
#include <util/string_util.h>

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

        // skip until end of statement, end of line, or end of file.
        // do {
        //     m_token_stream->Next();
        // } while (m_token_stream->HasNext() &&
        //     !Match(TK_NEWLINE, true) &&
        //     !Match(TK_SEMICOLON, true));

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

std::shared_ptr<AstStatement> Parser::ParseStatement(
    bool top_level,
    bool read_terminators
)
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
        } else if (MatchKeyword(Keyword_let, false) || MatchKeyword(Keyword_const, false)) {
            res = ParseVariableDeclaration();
        } else if (MatchKeyword(Keyword_func, false)) {
            if (MatchAhead(TK_IDENT, 1)) {
                res = ParseFunctionDefinition();
            } else {
                res = ParseFunctionExpression();
            }
        } else if (MatchKeyword(Keyword_async, false)) {
            res = ParseAsyncExpression();
        } else if (MatchKeyword(Keyword_pure, false)) {
            res = ParsePureExpression();
        } else if (MatchKeyword(Keyword_impure)) {
            res = ParseImpureExpression();
        } else if (MatchKeyword(Keyword_class, false)) {
            res = ParseTypeDefinition();
        } else if (MatchKeyword(Keyword_enum, false)) {
            res = ParseEnumDefinition();
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
        } else if (MatchKeyword(Keyword_meta, false)) {
            res = ParseMetaBlock();
        } else if (MatchKeyword(Keyword_syntax, false)) {
            res = ParseSyntaxDefinition();
        } else {
            res = ParseExpression();
        }
    } else if (Match(TK_OPEN_BRACE, false)) {
        res = ParseBlock();
    } else {
        std::shared_ptr<AstExpression> expr = ParseExpression(false);
        
        /*if (Match(TK_FAT_ARROW)) {
            expr = ParseActionExpression(expr);
        }*/

        res = expr;
    }

    if (read_terminators && res != nullptr && m_token_stream->HasNext()) {
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
    bool override_fat_arrows,
    bool override_angle_brackets,
    bool override_square_brackets)
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

        if (!override_angle_brackets && MatchOperator("<")) {
            expr = ParseAngleBrackets(expr);
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
    } else if (MatchKeyword(Keyword_async)) {
        expr = ParseAsyncExpression();
    } else if (MatchKeyword(Keyword_pure)) {
        expr = ParsePureExpression();
    } else if (MatchKeyword(Keyword_impure)) {
        expr = ParseImpureExpression();
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
            Match(TK_OPEN_PARENTH) ||
            // (!override_commas && Match(TK_COMMA)) ||
            (!override_fat_arrows && Match(TK_FAT_ARROW)) ||
            (!override_square_brackets && Match(TK_OPEN_BRACKET)) ||
            //(!override_angle_brackets && MatchOperator("<")) ||
            MatchKeyword(Keyword_has)))
    {
        if (Match(TK_DOT)) {
            expr = ParseMemberExpression(expr);
        }

        if (!override_square_brackets && Match(TK_OPEN_BRACKET)) {
            expr = ParseArrayAccess(expr);
        }

        if (Match(TK_OPEN_PARENTH)) {
            expr = ParseCallExpression(expr);
        }

        if (!override_fat_arrows && Match(TK_FAT_ARROW)) {
            expr = ParseActionExpression(expr);
        }

        // if (!override_commas && Match(TK_COMMA)) {
        //     expr = ParseTupleExpression(expr);
        // }

        if (!override_angle_brackets && MatchOperator("<")) {
            expr = ParseAngleBrackets(expr);
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

            std::vector<std::shared_ptr<AstParameter>> params;
            
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

                // function return type, or generator token
                if (Match(TK_COLON) || MatchOperator("*")) {
                    found_function_token = true;
                }
                
                // go back to where it was before reading the ')' token
                m_token_stream->SetPosition(before);
            }

            if (found_function_token) {
                // go back to before open '(' found, 
                // to allow ParseFunctionParameters() to handle it
                m_token_stream->SetPosition(before_pos);

                std::vector<std::shared_ptr<AstParameter>> params;
                
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

                    std::vector<std::shared_ptr<AstParameter>> params;
                    
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

std::shared_ptr<AstExpression> Parser::ParseAngleBrackets(std::shared_ptr<AstExpression> target)
{
    SourceLocation location = CurrentLocation();
    int before_pos = m_token_stream->GetPosition();

    std::vector<std::shared_ptr<AstArgument>> args;

    if (Token token = ExpectOperator("<", true)) {
        if (MatchOperator(">", true)) {
            return std::shared_ptr<AstTemplateInstantiation>(new AstTemplateInstantiation(
                target,
                args,
                token.GetLocation()
            ));
        }

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

            if (auto term = ParseTerm(true)) { // override commas
                args.push_back(std::shared_ptr<AstArgument>(new AstArgument(
                    term,
                    is_splat_arg,
                    is_named_arg,
                    arg_name,
                    arg_location
                )));
            } else {
                // not an argument, revert to start.
                m_token_stream->SetPosition(before_pos);
                // return as comparison expression
                return ParseBinaryExpression(0, target);
            }
        } while (Match(TK_COMMA, true));

        if (MatchOperator(">", true)) {
            return std::shared_ptr<AstTemplateInstantiation>(new AstTemplateInstantiation(
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
        return std::shared_ptr<AstArgument>(new AstArgument(
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

std::shared_ptr<AstArgumentList> Parser::ParseArguments(
    bool require_parentheses,
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

std::shared_ptr<AstCallExpression> Parser::ParseCallExpression(std::shared_ptr<AstExpression> target, bool require_parentheses)
{
    if (target != nullptr) {
        if (auto args = ParseArguments(require_parentheses)) {
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
                    ? Config::global_module_name
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
        std::shared_ptr<AstExpression> expr;

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

std::shared_ptr<AstTemplateInstantiation> Parser::ParseTemplateInstantiation(std::shared_ptr<AstIdentifier> expr)
{
    if (Token token = ExpectOperator("!", true)) {
        std::shared_ptr<AstArgumentList> arg_list;

        if (Match(TK_OPEN_PARENTH, false)) {
            // parse args
            arg_list = ParseArguments();
        }

        std::vector<std::shared_ptr<AstArgument>> args;

        if (arg_list != nullptr) {
            args = arg_list->GetArguments();
        }

        return std::shared_ptr<AstTemplateInstantiation>(new AstTemplateInstantiation(
            expr,
            args,
            token.GetLocation()
        ));
    }

    return nullptr;
}

std::shared_ptr<AstNewExpression> Parser::ParseNewExpression()
{
    if (Token token = ExpectKeyword(Keyword_new, true)) {
        if (auto proto = ParsePrototypeSpecification()) {
            std::shared_ptr<AstArgumentList> arg_list;

            if (Match(TK_OPEN_PARENTH, false)) {
                // parse args
                arg_list = ParseArguments();
            }

            return std::shared_ptr<AstNewExpression>(new AstNewExpression(
                proto,
                //type_spec,
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
        if (!Expect(TK_OPEN_PARENTH, true)) {
            return nullptr;
        }

        std::shared_ptr<AstExpression> conditional;
        if (!(conditional = ParseExpression())) {
            return nullptr;
        }

        if (!Expect(TK_CLOSE_PARENTH, true)) {
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
        if (!Expect(TK_OPEN_PARENTH, true)) {
            return nullptr;
        }

        std::shared_ptr<AstExpression> conditional;
        if (!(conditional = ParseExpression())) {
            return nullptr;
        }

        if (!Expect(TK_CLOSE_PARENTH, true)) {
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
    const auto position_before = m_token_stream->GetPosition();

    if (Token token = ExpectKeyword(Keyword_for, true)) {
        if (!Expect(TK_OPEN_PARENTH, true)) {
            return nullptr;
        }

        std::shared_ptr<AstStatement> decl_part;

        if (!Match(TK_SEMICOLON)) {
            if ((decl_part = ParseStatement(false, false))) { // do not eat ';'
                if (!Match(TK_SEMICOLON)) {
                    m_token_stream->SetPosition(position_before);

                    return ParseForEachLoop();
                }
            } else {
                return nullptr;
            }
        }

        if (!Expect(TK_SEMICOLON, true)) {
            return nullptr;
        }

        std::shared_ptr<AstExpression> condition_part;

        if (!Match(TK_SEMICOLON)) {
            if (!(condition_part = ParseExpression())) {
                return nullptr;
            }
        }

        if (!Expect(TK_SEMICOLON, true)) {
            return nullptr;
        }

        std::shared_ptr<AstExpression> increment_part;

        if (!Match(TK_CLOSE_PARENTH)) {
            if (!(increment_part = ParseExpression())) {
                return nullptr;
            }
        }

        if (!Expect(TK_CLOSE_PARENTH, true)) {
            return nullptr;
        }

        std::shared_ptr<AstBlock> block;
        if (!(block = ParseBlock())) {
            return nullptr;
        }

        return std::shared_ptr<AstForLoop>(new AstForLoop(
            decl_part,
            condition_part,
            increment_part,
            block,
            token.GetLocation()
        ));
    }

    return nullptr;
}

std::shared_ptr<AstForEachLoop> Parser::ParseForEachLoop()
{
    if (Token token = ExpectKeyword(Keyword_for, true)) {
        if (!Expect(TK_OPEN_PARENTH, true)) {
            return nullptr;
        }

        std::vector<std::shared_ptr<AstParameter>> params = ParseFunctionParameters();

        ExpectKeyword(Keyword_in, true);

        std::shared_ptr<AstExpression> iteree;
        if ((iteree = ParseExpression()) == nullptr) {
            return nullptr;
        }
        
        if (!Expect(TK_CLOSE_PARENTH, true)) {
            return nullptr;
        }

        std::shared_ptr<AstBlock> block;
        if ((block = ParseBlock()) == nullptr) {
            return nullptr;
        }

        return std::shared_ptr<AstForEachLoop>(new AstForEachLoop(
            params,
            iteree,
            block,
            token.GetLocation()
        ));
    }

    return nullptr;
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

std::shared_ptr<AstExpression> Parser::ParseExpression(bool override_commas,
    bool override_fat_arrows,
    bool override_angle_brackets)
{
    if (auto term = ParseTerm(override_commas, override_fat_arrows, override_angle_brackets)) {
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
        return term;
    }

    return nullptr;
}

std::shared_ptr<AstPrototypeSpecification> Parser::ParsePrototypeSpecification()
{
    const SourceLocation location = CurrentLocation();

    if (auto term = ParseTerm(true, true, false, true)) {
        //while (Match(TK_OPEN_BRACKET) || Match(TK_QUESTION_MARK)) {
            // question mark at the end of a type is syntactical sugar for `Maybe(T)`
            /*if (Token token = Match(TK_QUESTION_MARK, true)) {
                term = std::shared_ptr<AstTemplateInstantiation>(new AstTemplateInstantiation(
                    std::shared_ptr<AstVariable>(new AstVariable(
                        "Maybe",
                        token.GetLocation()
                    )),
                    {
                        std::shared_ptr<AstArgument>(new AstArgument(
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
                term = std::shared_ptr<AstTemplateInstantiation>(new AstTemplateInstantiation(
                    std::shared_ptr<AstVariable>(new AstVariable(
                        BuiltinTypes::ARRAY->GetName(),
                        token.GetLocation()
                    )),
                    {
                        std::shared_ptr<AstArgument>(new AstArgument(
                            term,
                            false,
                            false,
                            "",
                            term->GetLocation()
                        ))
                    },
                    term->GetLocation()
                ));

                Expect(TK_CLOSE_BRACKET, true);
            }
        //}

        return std::shared_ptr<AstPrototypeSpecification>(new AstPrototypeSpecification(
            term,
            location
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

std::shared_ptr<AstVariableDeclaration> Parser::ParseVariableDeclaration(bool allow_keyword_names,
    bool allow_quoted_names)
{
    const SourceLocation location = CurrentLocation();

    std::set<Keywords> used_specifiers;

    const Keywords prefix_keywords[] = {
        Keyword_let,
        Keyword_const
    };

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

    const bool is_const = used_specifiers.find(Keyword_const) != used_specifiers.end();
    bool is_generic = false;

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

        std::vector<std::shared_ptr<AstParameter>> template_expr_params;

        if (Token lt = MatchOperator("<", false)) {
            is_generic = true;

            template_expr_params = ParseGenericParameters();
            template_expr_location = lt.GetLocation();
        }

        std::shared_ptr<AstPrototypeSpecification> proto;
        std::shared_ptr<AstExpression> assignment;

        if (Match(TK_COLON, true)) {
            // read object type
            //type_spec = ParseTypeSpecification();
            proto = ParsePrototypeSpecification();
        }

        if ((assignment = ParseAssignment())) {
            if (is_generic) {
                assignment = std::shared_ptr<AstTemplateExpression>(new AstTemplateExpression(
                    assignment,
                    template_expr_params,
                    proto,
                    assignment->GetLocation()
                ));
            }
        }

        return std::shared_ptr<AstVariableDeclaration>(new AstVariableDeclaration(
            identifier.GetValue(),
            is_generic ? nullptr : proto,
            //type_spec,
            assignment,
            {}, //template_expr_params,
            is_const,
            is_generic,
            location
        ));
    }

    return nullptr;
}

std::shared_ptr<AstStatement> Parser::ParseFunctionDefinition(bool require_keyword)
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
        bool is_generic = false;
        std::vector<std::shared_ptr<AstParameter>> generic_parameters;

        // check for generic
        if (MatchOperator("<", false)) {
            is_generic = true;
            generic_parameters = ParseGenericParameters();
        }

        std::shared_ptr<AstExpression> assignment;

        std::vector<std::shared_ptr<AstParameter>> params;
        
        if (Match(TK_OPEN_PARENTH, true)) {
            params = ParseFunctionParameters();
            Expect(TK_CLOSE_PARENTH, true);
        }

        // if (auto expr = ParseFunctionExpression(false, params)) {
        //     return std::shared_ptr<AstFunctionDefinition>(new AstFunctionDefinition(
        //         identifier.GetValue(),
        //         expr,
        //         location
        //     ));
        //}

        assignment = ParseFunctionExpression(false, params);

        if (assignment == nullptr) {
            return nullptr;
        }

        if (is_generic) {
            assignment = std::shared_ptr<AstTemplateExpression>(new AstTemplateExpression(
                assignment,
                generic_parameters,
                nullptr,
                assignment->GetLocation()
            ));
        }

        return std::shared_ptr<AstVariableDeclaration>(new AstVariableDeclaration(
            identifier.GetValue(),
            nullptr, // prototype specification
            assignment,
            {}, //template_expr_params,
            true, // it is constant
            is_generic, // is generic
            location
        ));
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
            if (Match(TK_OPEN_PARENTH, true)) {
                params = ParseFunctionParameters();
                Expect(TK_CLOSE_PARENTH, true);
            }
        }

        std::shared_ptr<AstPrototypeSpecification> type_spec;

        if (Match(TK_COLON, true)) {
            // read return type for functions
            type_spec = ParsePrototypeSpecification();
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

        std::vector<std::shared_ptr<AstParameter>> params;
        
        if (Match(TK_OPEN_PARENTH, true)) {
            params = ParseFunctionParameters();
            Expect(TK_CLOSE_PARENTH, true);
        }

        return ParseFunctionExpression(
            false,
            params,
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

        std::vector<std::shared_ptr<AstParameter>> params;
        
        if (Match(TK_OPEN_PARENTH, true)) {
            params = ParseFunctionParameters();
            Expect(TK_CLOSE_PARENTH, true);
        }

        return ParseFunctionExpression(
            false,
            params,
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

        std::vector<std::shared_ptr<AstParameter>> params;
        
        if (Match(TK_OPEN_PARENTH, true)) {
            params = ParseFunctionParameters();
            Expect(TK_CLOSE_PARENTH, true);
        }

        return ParseFunctionExpression(
            false,
            params,
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

    bool found_variadic = false;
    bool keep_reading = true;

    while (keep_reading) {
        Token token = Token::EMPTY;

        if (Match(TK_CLOSE_PARENTH, false)) {
            keep_reading = false;
            break;
        }

        bool is_const = false;
        if (MatchKeyword(Keyword_const, false)) {
            is_const = true;
        }
        
        if (token = ExpectIdentifier(true, true)) {
            std::shared_ptr<AstPrototypeSpecification> type_spec;
            std::shared_ptr<AstExpression> default_param;

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

    return parameters;
}

std::vector<std::shared_ptr<AstParameter>> Parser::ParseGenericParameters()
{
    std::vector<std::shared_ptr<AstParameter>> template_expr_params;

    if (Token lt = ExpectOperator("<", true)) {
        m_template_argument_depth++;

        template_expr_params = ParseFunctionParameters();

        ExpectOperator(">", true);

        m_template_argument_depth--;
    }

    return template_expr_params;
}

std::shared_ptr<AstStatement> Parser::ParseTypeDefinition()
{
    if (Token token = ExpectKeyword(Keyword_class, true)) {
        if (Token identifier = ExpectIdentifier(false, true)) {
            bool is_generic = false;
            std::vector<std::shared_ptr<AstParameter>> generic_parameters;
            std::shared_ptr<AstExpression> assignment;

            // check for generic
            if (MatchOperator("<", false)) {
                is_generic = true;
                generic_parameters = ParseGenericParameters();
            }

            // check type alias
            if (MatchOperator("=", true)) {
                if (auto aliasee = ParsePrototypeSpecification()) {
                    return std::shared_ptr<AstTypeAlias>(new AstTypeAlias(
                        identifier.GetValue(), 
                        aliasee,
                        identifier.GetLocation()
                    ));
                } else {
                    return nullptr;
                }
            } else {
                assignment = ParseTypeExpression(false, false, identifier.GetValue());
            }

            if (assignment == nullptr) {
                // could not parse type expr
                return nullptr;
            }

            if (is_generic) {
                assignment = std::shared_ptr<AstTemplateExpression>(new AstTemplateExpression(
                    assignment,
                    generic_parameters,
                    nullptr,
                    assignment->GetLocation()
                ));
            }

            return std::shared_ptr<AstVariableDeclaration>(new AstVariableDeclaration(
                identifier.GetValue(),
                nullptr, // prototype specification
                assignment,
                {}, //template_expr_params,
                true, // it is constant
                is_generic, // is generic
                token.GetLocation()
            ));
        }
    }

    return nullptr;
}

std::shared_ptr<AstTypeExpression> Parser::ParseTypeExpression(
    bool require_keyword,
    bool allow_identifier,
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

    std::vector<std::shared_ptr<AstVariableDeclaration>> members;
    std::vector<std::shared_ptr<AstVariableDeclaration>> static_members;

    if (Expect(TK_OPEN_BRACE, true)) {
        while (!Match(TK_CLOSE_BRACE, true)) {
            // read ident
            bool is_static = false,
                 is_const = false,
                 is_function = false,
                 is_variable = false;

            if (MatchKeyword(Keyword_static, true)) {
                is_static = true;
            }

            // place rollback position here because ParseVariableDeclaration()
            // will handle everything. put keywords that ParseVariableDeclaration()
            // does /not/ handle, above.
            const auto position_before = m_token_stream->GetPosition();

            if (MatchKeyword(Keyword_let, true)) {
                is_variable = true;
            }

            if (MatchKeyword(Keyword_const, true)) {
                is_variable = true;
                is_const = true;
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

            std::shared_ptr<AstExpression> assignment;

            bool is_generic = false;
            std::vector<std::shared_ptr<AstParameter>> generic_parameters;

            // check for generic
            if (MatchOperator("<", false)) {
                is_generic = true;
                is_const = true;
                generic_parameters = ParseGenericParameters();
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
                std::vector<std::shared_ptr<AstParameter>> params;
                
                if (Match(TK_OPEN_PARENTH, true)) {
                    params = ParseFunctionParameters();
                    Expect(TK_CLOSE_PARENTH, true);
                }

                assignment = ParseFunctionExpression(false, params);

                if (assignment == nullptr) {
                    return nullptr;
                }

                if (is_generic) {
                    assignment = std::shared_ptr<AstTemplateExpression>(new AstTemplateExpression(
                        assignment,
                        generic_parameters,
                        nullptr,
                        assignment->GetLocation()
                    ));
                }

                std::shared_ptr<AstVariableDeclaration> member(new AstVariableDeclaration(
                    identifier.GetValue(),
                    nullptr, // prototype specification
                    assignment,
                    {}, //template_expr_params,
                    is_const, // is constant
                    is_generic, // is generic
                    location
                ));

                if (is_static) {
                    static_members.push_back(member);
                } else {
                    members.push_back(member);
                }
            } else {
                // rollback
                m_token_stream->SetPosition(position_before);

                if (auto member = ParseVariableDeclaration(
                    true, // allow keyword names
                    true // allow quoted names
                )) {
                    if (is_static) {
                        static_members.push_back(member);
                    } else {
                        members.push_back(member);
                    }
                } else {
                    break;
                }
            }

            ExpectEndOfStmt();
            SkipStatementTerminators();
        }

        return std::shared_ptr<AstTypeExpression>(new AstTypeExpression(
            type_name,
            nullptr, // TODO
            members,
            static_members,
            location
        ));
    }

    return nullptr;
}

std::shared_ptr<AstStatement> Parser::ParseEnumDefinition()
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

            return std::shared_ptr<AstVariableDeclaration>(new AstVariableDeclaration(
                identifier.GetValue(),
                nullptr, // prototype specification
                assignment,
                {}, //template_expr_params,
                true, // it is constant
                false, // is generic
                token.GetLocation()
            ));
        }
    }

    return nullptr;
}

std::shared_ptr<AstEnumExpression> Parser::ParseEnumExpression(
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

    std::shared_ptr<AstPrototypeSpecification> underlying_type;

    if (Match(TK_COLON, true)) {
        // underlying type
        underlying_type = ParsePrototypeSpecification();
    }
    
    std::vector<EnumEntry> entries;

    if (Expect(TK_OPEN_BRACE, true)) {
        while (!Match(TK_CLOSE_BRACE, false)) {
            EnumEntry entry{};

            if (const Token ident = Expect(TK_IDENT, true)) {
                entry.name = ident.GetValue();
                entry.location = ident.GetLocation();
            } else {
                break;
            }

            if (const Token op = MatchOperator("=", true)) {
                entry.assignment = ParseExpression(true);
            }

            entries.push_back(entry);

            while (Match(TK_NEWLINE, true));

            if (!Match(TK_CLOSE_BRACE, false)) {
                Expect(TK_COMMA, true);
            }
        }

        Expect(TK_CLOSE_BRACE, true);

        return std::shared_ptr<AstEnumExpression>(new AstEnumExpression(
            enum_name,
            entries,
            underlying_type,
            location
        ));
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

    auto aliasee = ParseIdentifier();
    if (aliasee == nullptr) {
        return nullptr;
    }

    return std::shared_ptr<AstAliasDeclaration>(new AstAliasDeclaration(
        ident.GetValue(),
        aliasee,
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

    if (const auto mixin_expr = ParseMixinExpression(ident.GetValue())) {
        return std::shared_ptr<AstMixinDeclaration>(new AstMixinDeclaration(
            ident.GetValue(),
            mixin_expr,
            ident.GetLocation()
        ));
    }

    return nullptr;
}

std::shared_ptr<AstMixin> Parser::ParseMixinExpression(const std::string &name)
{
    const Token str_expr = Expect(TK_STRING, true);
    if (!str_expr) {
        return nullptr;
    }

    return std::shared_ptr<AstMixin>(new AstMixin(
        name,
        str_expr.GetValue(),
        str_expr.GetLocation()
    ));
}

std::shared_ptr<AstImport> Parser::ParseImport()
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

std::shared_ptr<AstFileImport> Parser::ParseFileImport()
{
    if (Token token = ExpectKeyword(Keyword_import, true)) {
        if (Token file = Expect(TK_STRING, true)) {
            std::shared_ptr<AstFileImport> result(new AstFileImport(
                file.GetValue(),
                token.GetLocation()
            ));

            return result;
        }
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
    if (Token token = ExpectKeyword(Keyword_import, true)) {
        std::vector<std::shared_ptr<AstModuleImportPart>> parts;

        if (auto part = ParseModuleImportPart(false)) {
            parts.push_back(part);

            return std::shared_ptr<AstModuleImport>(new AstModuleImport(
                parts,
                token.GetLocation()
            ));
        }
    }

    return nullptr;
}

std::shared_ptr<AstReturnStatement> Parser::ParseReturnStatement()
{
    const SourceLocation location = CurrentLocation();

    if (Token token = ExpectKeyword(Keyword_return, true)) {
        std::shared_ptr<AstExpression> expr;

        if (!Match(TK_SEMICOLON, true)) {
            expr = ParseExpression();
        }

        return std::shared_ptr<AstReturnStatement>(new AstReturnStatement(
            expr,
            location
        ));
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

std::shared_ptr<AstMetaBlock> Parser::ParseMetaBlock()
{
    const SourceLocation location = CurrentLocation();

    if (Token token = ExpectKeyword(Keyword_meta, true)) {
        if (auto block = ParseBlock()) {
            return std::shared_ptr<AstMetaBlock>(new AstMetaBlock(
                block->GetChildren(),
                location
            ));
        }
    }

    return nullptr;
}

std::shared_ptr<AstExpression> Parser::ParseMetaProperty()
{
    const SourceLocation location = CurrentLocation();

    if (Token token = ExpectKeyword(Keyword_meta, true)) {
        Expect(TK_DOUBLE_COLON, true);

        Token ident = ExpectIdentifier(true, true);

        // @TODO refactor this to use ParseFunctionParameters() etc...
        // rather than having it hard-coded. makes it hard to add new features..

        if (ident.GetValue() == "query") {
            Expect(TK_OPEN_PARENTH, true);

            std::shared_ptr<AstString> query_command_name = ParseStringLiteral();
            if (query_command_name == nullptr) {
                return nullptr;
            }

            Expect(TK_COMMA, true);

            std::shared_ptr<AstExpression> term = ParseTerm();
            if (term == nullptr) {
                return nullptr;
            }

            Expect(TK_CLOSE_PARENTH, true);

            return std::shared_ptr<AstSymbolQuery>(new AstSymbolQuery(
                query_command_name->GetValue(),
                term,
                location
            ));
        }
    }

    return nullptr;
}

std::shared_ptr<AstSyntaxDefinition> Parser::ParseSyntaxDefinition()
{
    const SourceLocation location = CurrentLocation();

    if (Token token = ExpectKeyword(Keyword_syntax, true)) {
        const std::shared_ptr<AstString> syntax_string = ParseStringLiteral();
        
        if (!syntax_string) {
            return nullptr;
        }
        
        const Token op = ExpectOperator("=", true);
        
        if (!op) {
            return nullptr;
        }
        
        const std::shared_ptr<AstString> transform_string = ParseStringLiteral();

        if (!transform_string) {
            return nullptr;
        }

        return std::shared_ptr<AstSyntaxDefinition>(new AstSyntaxDefinition(
            syntax_string,
            transform_string,
            location
        ));
    }

    return nullptr;
}

} // namespace hyperion::compiler
