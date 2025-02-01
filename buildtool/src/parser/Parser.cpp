#include <parser/Parser.hpp>
#include <parser/Lexer.hpp>

#include <util/json/JSON.hpp>
#include <util/StringUtil.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion::buildtool {

HYP_DECLARE_LOG_CHANNEL(BuildTool);
HYP_DEFINE_LOG_SUBCHANNEL(Parser, BuildTool);

#pragma region JSON conversion

void ASTType::ToJSON(json::JSONValue &out) const
{
    json::JSONObject object;

    object["is_const"] = is_const;
    object["is_volatile"] = is_volatile;
    object["is_static"] = is_static;
    object["is_inline"] = is_inline;
    object["is_virtual"] = is_virtual;
    object["is_lvalue_reference"] = is_lvalue_reference;
    object["is_rvalue_reference"] = is_rvalue_reference;
    object["is_pointer"] = is_pointer;
    object["is_array"] = is_array;
    object["is_template"] = is_template;
    object["is_function_pointer"] = is_function_pointer;
    object["is_function"] = is_function;

    if (is_array) {
        object["array_size"] = array_size;
    }

    if (ptr_to) {
        json::JSONValue ptr_to_json;
        ptr_to->ToJSON(ptr_to_json);
        object["ptr_to"] = std::move(ptr_to_json);
    }

    if (ref_to) {
        json::JSONValue ref_to_json;
        ref_to->ToJSON(ref_to_json);
        object["ref_to"] = std::move(ref_to_json);
    }

    if (type_name.HasValue()) {
        json::JSONObject type_name_json;
        type_name_json["is_global"] = type_name->is_global;

        json::JSONArray type_name_parts_array;

        for (const String &part : type_name->parts) {
            type_name_parts_array.PushBack(json::JSONString(part));
        }

        type_name_json["parts"] = std::move(type_name_parts_array);

        object["type_name"] = std::move(type_name_json);
    }

    if (is_template) {
        json::JSONArray template_arguments_array;

        for (const RC<ASTType> &template_argument : template_arguments) {
            json::JSONValue template_argument_json;
            template_argument->ToJSON(template_argument_json);
            template_arguments_array.PushBack(std::move(template_argument_json));
        }

        object["template_arguments"] = std::move(template_arguments_array);
    }

    out = std::move(object);
}

void ASTFunctionType::ToJSON(json::JSONValue &out) const
{
    json::JSONValue type_json;
    ASTType::ToJSON(type_json);

    json::JSONObject object;

    object["is_const_method"] = is_const_method;
    object["is_rvalue_method"] = is_rvalue_method;
    object["is_lvalue_method"] = is_lvalue_method;

    json::JSONValue return_type_json;
    return_type->ToJSON(return_type_json);
    object["return_type"] = std::move(return_type_json);

    json::JSONArray parameters_array;

    for (const Pair<String, RC<ASTType>> &parameter : parameters) {
        json::JSONObject parameter_json;
        parameter_json["name"] = parameter.first;

        json::JSONValue parameter_type_json;
        parameter.second->ToJSON(parameter_type_json);
        parameter_json["type"] = std::move(parameter_type_json);

        parameters_array.PushBack(std::move(parameter_json));
    }

    object["parameters"] = std::move(parameters_array);

    out = type_json.ToObject().Merge(std::move(object));
}

void ASTMemberDecl::ToJSON(json::JSONValue &out) const
{
    json::JSONObject object;

    object["name"] = name;

    json::JSONValue type_json;
    type->ToJSON(type_json);
    object["type"] = std::move(type_json);

    out = std::move(object);
}

#pragma endregion JSON conversion

Parser::Parser(
    TokenStream *token_stream,
    CompilationUnit *compilation_unit
) : m_token_stream(token_stream),
    m_compilation_unit(compilation_unit)
{
    if (m_compilation_unit->GetPreprocessorDefinitions().Any()) {
        TokenStream new_token_stream { m_token_stream->GetInfo() };

        while (m_token_stream->HasNext()) {
            Token token = m_token_stream->Next();

            if (token.GetTokenClass() == TokenClass::TK_IDENT) {
                auto preprocessor_definitions_it = m_compilation_unit->GetPreprocessorDefinitions().Find(token.GetValue());

                if (preprocessor_definitions_it != m_compilation_unit->GetPreprocessorDefinitions().End()) {
                    SourceFile macro_source_file("<macro>", preprocessor_definitions_it->second.Size());

                    ByteBuffer temp(preprocessor_definitions_it->second.Size(), preprocessor_definitions_it->second.Data());
                    macro_source_file.ReadIntoBuffer(temp);
                    
                    TokenStream macro_token_stream { TokenStreamInfo { "<input>" } };
                    CompilationUnit macro_compilation_unit;

                    Lexer macro_lexer(SourceStream(&macro_source_file), &macro_token_stream, &macro_compilation_unit);
                    macro_lexer.Analyze();

                    while (macro_token_stream.HasNext()) {
                        Token macro_token = macro_token_stream.Next();

                        new_token_stream.Push(macro_token);
                    }

                    continue;
                }
            }

            new_token_stream.Push(token);
        }

        *m_token_stream = std::move(new_token_stream);
    }
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

Token Parser::MatchOperator(const String &op, bool read)
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

Token Parser::MatchOperatorAhead(const String &op, int n)
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
        String error_str;

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

Token Parser::ExpectOperator(const String &op, bool read)
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

Token Parser::MatchIdentifier(const UTF8StringView &value, bool read)
{
    Token ident = Match(TK_IDENT, false);

    if (value.Size() != 0) {
        if (ident && ident.GetValue() == value) {
            if (read && m_token_stream->HasNext()) {
                m_token_stream->Next();
            }

            return ident;
        }
    } else {
        if (read && m_token_stream->HasNext()) {
            m_token_stream->Next();
        }
    }

    return Token::EMPTY;
}

Token Parser::ExpectIdentifier(const UTF8StringView &value, bool read)
{
    Token ident = MatchIdentifier(value, read);

    if (!ident) {
        const SourceLocation location = CurrentLocation();

        m_compilation_unit->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_expected_identifier,
            location
        ));
    }

    return ident;
}

bool Parser::ExpectEndOfStmt()
{
    const SourceLocation location = CurrentLocation();

    if (!Match(TK_SEMICOLON, true) && !Match(TK_CLOSE_BRACE, false)) {
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
    while (Match(TK_SEMICOLON, true));
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

TypeName Parser::ParseTypeName()
{
    TypeName type_name;

    if (Match(TK_DOUBLE_COLON, true)) {
        type_name.is_global = true;
    }

    while (true) {
        Token ident = Expect(TK_IDENT, true);
        type_name.parts.PushBack(ident.GetValue());

        if (!Match(TK_DOUBLE_COLON, true)) {
            break;
        }
    }

    return type_name;
}

RC<ASTMemberDecl> Parser::ParseMemberDecl()
{
    RC<ASTMemberDecl> member_decl = MakeRefCountedPtr<ASTMemberDecl>();

    bool is_inline = false;
    bool is_virtual = false;
    bool is_static = false;
    bool is_function = false;

    bool keep_reading = true;
    while (keep_reading) {
        if (MatchIdentifier("inline", true)) {
            is_inline = true;
            is_function = true;
        } else if (MatchIdentifier("virtual", true)) {
            is_virtual = true;
            is_function = true;
        } else if (MatchIdentifier("static", true)) {
            is_static = true;
        } else {
            keep_reading = false;
        }
    }

    member_decl->type = ParseType();

    if (member_decl->type->is_function_pointer) {
        // Need to implement function pointer parsing
        HYP_NOT_IMPLEMENTED();
    }

    if (Token name_token = Expect(TK_IDENT, true)) {
        member_decl->name = name_token.GetValue();
    } else {
        json::JSONValue json;
        member_decl->ToJSON(json);
        HYP_LOG(Parser, Error, "Expected identifier in member declaration\t{}", json.ToString());
        HYP_BREAKPOINT;
    }

    Token open_parenth_token = Token::EMPTY;

    if (is_function) {
        open_parenth_token = Expect(TK_OPEN_PARENTH, false);
    } else {
        open_parenth_token = Match(TK_OPEN_PARENTH, false);
    }

    if (open_parenth_token) {
        member_decl->type = ParseFunctionType(member_decl->type);
    } else if (Match(TK_OPEN_BRACKET, true)) {
        member_decl->type->is_array = true;
        member_decl->type->array_size = -1;

        if (Token size_token = Match(TK_INTEGER, true)) {
            member_decl->type->array_size = StringUtil::Parse<int>(size_token.GetValue(), -1);
        } else {
            // @TODO Eat expression
        }

        Expect(TK_CLOSE_BRACKET, true);
    }

    member_decl->type->is_virtual = is_virtual;
    member_decl->type->is_inline = is_inline;
    member_decl->type->is_static = is_static;

    return member_decl;
}

RC<ASTType> Parser::ParseType()
{
    // Start by creating our root type instance
    RC<ASTType> root_type = MakeRefCountedPtr<ASTType>();

    bool keep_reading = true;
    while (keep_reading) {
        if (MatchIdentifier("const", true)) {
            root_type->is_const = true;
        } else if (MatchIdentifier("volatile", true)) {
            root_type->is_volatile = true;
        } else {
            keep_reading = false;
        }
    }

    root_type->type_name = ParseTypeName();

    if (MatchOperator("<", true)) {
        root_type->is_template = true;

        while (true) {
            root_type->template_arguments.PushBack(ParseType());

            if (!Match(TK_COMMA, true)) {
                break;
            }
        }

        ExpectOperator(">", true);
    }

    while (true) {
        // Reference
        if (MatchOperator("&", true)) {
            RC<ASTType> ref_type = MakeRefCountedPtr<ASTType>();
            ref_type->is_lvalue_reference = true;
            ref_type->ref_to = root_type;
            // Optional cv-qualifiers after '&'
            bool more_cv = true;
            while (more_cv) {
                if (MatchIdentifier("const", true)) {
                    ref_type->is_const = true;
                } else if (MatchIdentifier("volatile", true)) {
                    ref_type->is_volatile = true;
                } else {
                    more_cv = false;
                }
            }
            root_type = ref_type;
        } else if (MatchOperator("&&", true)) {
            RC<ASTType> ref_type = MakeRefCountedPtr<ASTType>();
            ref_type->is_rvalue_reference = true;
            ref_type->ref_to = root_type;
            // Optional cv-qualifiers after '&&'
            bool more_cv = true;
            while (more_cv) {
                if (MatchIdentifier("const", true)) {
                    ref_type->is_const = true;
                } else if (MatchIdentifier("volatile", true)) {
                    ref_type->is_volatile = true;
                } else {
                    more_cv = false;
                }
            }
            root_type = ref_type;
        } else if (MatchOperator("*", true)) {
            RC<ASTType> ptr_type = MakeRefCountedPtr<ASTType>();
            ptr_type->is_pointer = true;
            ptr_type->ptr_to = root_type;
            // Optional cv-qualifiers after '*'
            bool more_cv = true;
            while (more_cv) {
                if (MatchIdentifier("const", true)) {
                    ptr_type->is_const = true;
                } else if (MatchIdentifier("volatile", true)) {
                    ptr_type->is_volatile = true;
                } else {
                    more_cv = false;
                }
            }
            root_type = ptr_type;
        } else {
            break;
        }
    }

    // if (Match(TK_OPEN_PARENTH, false)) {
    //     if (MatchOperatorAhead("*", 1)) {
    //         // Function pointer
    //         RC<ASTType> func_ptr_type = MakeRefCountedPtr<ASTType>();
    //         func_ptr_type->is_function_pointer = true;
    //         func_ptr_type->ptr_to = root_type;
    //         root_type = func_ptr_type;

    //         // Need to implement function pointer parsing
    //         HYP_NOT_IMPLEMENTED();
    //     }
    // }

    return root_type;
}

RC<ASTFunctionType> Parser::ParseFunctionType(const RC<ASTType> &return_type)
{
    AssertThrow(return_type != nullptr);

    RC<ASTFunctionType> func_type = MakeRefCountedPtr<ASTFunctionType>();
    func_type->return_type = return_type;

    if (Expect(TK_OPEN_PARENTH, true)) {
        if (!Match(TK_CLOSE_PARENTH, false)) {
            while (true) {
                RC<ASTMemberDecl> param = ParseMemberDecl();
                func_type->parameters.PushBack({ param->name, param->type });

                if (!Match(TK_COMMA, true)) {
                    break;
                }
            }
        }

        Expect(TK_CLOSE_PARENTH, true);

        bool keep_reading = true;

        while (keep_reading) {
            if (MatchIdentifier("const", true)) {
                func_type->is_const_method = true;
            } else if (MatchIdentifier("override", true)) {
                func_type->is_override_method = true;
            } else if (MatchIdentifier("noexcept", true)) {
                func_type->is_noexcept_method = true;
            } else if (MatchOperator("&&", true)) {
                func_type->is_rvalue_method = true;
            } else if (MatchOperator("&", true)) {
                func_type->is_lvalue_method = true;
            } else {
                keep_reading = false;
            }
        }
    }

    return func_type;
}


} // namespace hyperion::buildtool