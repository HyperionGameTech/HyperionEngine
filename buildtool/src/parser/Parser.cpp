#include <parser/Parser.hpp>
#include <parser/Lexer.hpp>

#include <core/utilities/Result.hpp>

#include <core/json/JSON.hpp>
#include <core/utilities/StringUtil.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion::buildtool {

HYP_DECLARE_LOG_CHANNEL(BuildTool);
HYP_DEFINE_LOG_SUBCHANNEL(Parser, BuildTool);

#pragma region CSharp type mapping

const ASTType *ExtractInnerType(const ASTType *type)
{
    if (type->is_pointer) {
        return ExtractInnerType(type->ptr_to.Get());
    } else if (type->is_lvalue_reference || type->is_rvalue_reference) {
        return ExtractInnerType(type->ref_to.Get());
    } else {
        return type;
    }
}

TResult<String> MapToCSharpType(const ASTType *type)
{
    if (type->is_pointer && type->ptr_to->IsVoid()) {
        return String("IntPtr");
    }

    if (type->is_array) {
        if (!type->array_of) {
            return HYP_MAKE_ERROR(Error, "Array type has no inner type");
        }

        if (auto res = MapToCSharpType(type->array_of.Get()); res.HasError()) {
            return res.GetError();
        } else {
            return res.GetValue() + "[]";
        }
    }

    type = ExtractInnerType(type);
    AssertThrow(type != nullptr);

    if (type->type_name.HasValue()) {
        if (type->type_name->parts.Empty()) {
            return HYP_MAKE_ERROR(Error, "Type name has no parts");
        }
        
        static const HashMap<String, String> mapping {
            { "int", "int" },
            { "float", "float" },
            { "double", "double" },
            { "bool", "bool" },
            { "void", "void" },
            { "char", "char" },
            { "uint8", "byte" },
            { "uint16", "ushort" },
            { "uint32", "uint" },
            { "uint64", "ulong" },
            { "int8", "sbyte" },
            { "int16", "short" },
            { "int32", "int" },
            { "int64", "long" },
            { "string", "string" },
            { "String", "string" },
            { "ANSIString", "string" },
            { "UTF8StringView", "string" },
            { "ANSIStringView", "string" },
            { "FilePath", "string" },
            { "ByteBuffer", "byte[]" },
            { "ID", "IDBase" },
            { "NodeProxy", "Node" },
            { "Name", "Name" },
            { "WeakName", "Name" },
            { "AnyHandle", "object" },
            { "AnyRef", "object" },
            { "ConstAnyRef", "object" }
        };

        const String type_name_string = type->type_name->ToString(/* include_namespace */ false);

        auto it = mapping.Find(type_name_string);

        if (it != mapping.End()) {
            return it->second;
        }

        if (type->is_template) {
            const String template_name = type->type_name->parts.Back();

            if (template_name == "Array") {
                return String("Array");
            }

            if (template_name == "RC"
                || template_name == "Handle"
                || template_name == "EnumFlags")
            {
                if (type->template_arguments.Empty()) {
                    return HYP_MAKE_ERROR(Error, "Type missing template argument");
                }

                if (!type->template_arguments[0]->type) {
                    return HYP_MAKE_ERROR(Error, "Type template argument is not a type");
                }

                return MapToCSharpType(type->template_arguments[0]->type.Get());
            }

            HYP_LOG(Parser, Error, "Template type is unable to be mapped to a C# type: {}", type->Format());

            return HYP_MAKE_ERROR(Error, "Template type is unable to be mapped to a C# type");
        }

        return type_name_string;
    }

    HYP_LOG(Parser, Error, "Type is unable to be mapped to a C# type: {}", type->Format());

    return HYP_MAKE_ERROR(Error, "Type is unable to be mapped to a C# type");
}

#pragma endregion CSharp type mapping

#pragma region QualifiedName

String QualifiedName::ToString(bool include_namespace) const
{
    if (parts.Empty()) {
        return "";
    }

    if (include_namespace) {
        String result = parts[0];

        for (SizeType i = 1; i < parts.Size(); i++) {
            result += "::" + parts[i];
        }

        return result;
    } else {
        return parts.Back();
    }
}

#pragma endregion QualifiedName

#pragma region JSON conversion

void ASTUnaryExpr::ToJSON(json::JSONValue &out) const
{
    json::JSONObject object;
    object["node_type"] = "ASTUnaryExpr";

    json::JSONValue expr_json;
    expr->ToJSON(expr_json);
    object["expr"] = std::move(expr_json);

    object["op"] = op->LookupStringValue();
    object["is_prefix"] = is_prefix;

    out = std::move(object);
}

void ASTBinExpr::ToJSON(json::JSONValue &out) const
{
    json::JSONObject object;
    object["node_type"] = "ASTBinExpr";

    json::JSONValue left_json;
    left->ToJSON(left_json);
    object["left"] = std::move(left_json);

    json::JSONValue right_json;
    right->ToJSON(right_json);
    object["right"] = std::move(right_json);

    object["op"] = op->LookupStringValue();

    out = std::move(object);
}

void ASTTernaryExpr::ToJSON(json::JSONValue &out) const
{
    json::JSONObject object;
    object["node_type"] = "ASTTernaryExpr";

    json::JSONValue true_expr_json;
    true_expr->ToJSON(true_expr_json);
    object["true_expr"] = std::move(true_expr_json);

    json::JSONValue false_expr_json;
    false_expr->ToJSON(false_expr_json);
    object["false_expr"] = std::move(false_expr_json);

    json::JSONValue conditional_json;
    conditional->ToJSON(conditional_json);
    object["conditional"] = std::move(conditional_json);

    out = std::move(object);
}

void ASTLiteralString::ToJSON(json::JSONValue &out) const
{
    json::JSONObject object;
    object["node_type"] = "ASTLiteralString";
    object["value"] = json::JSONString(value);

    out = std::move(object);
}

void ASTLiteralInt::ToJSON(json::JSONValue &out) const
{
    json::JSONObject object;
    object["node_type"] = "ASTLiteralInt";
    object["value"] = value;

    out = std::move(object);
}

void ASTLiteralFloat::ToJSON(json::JSONValue &out) const
{
    json::JSONObject object;
    object["node_type"] = "ASTLiteralFloat";
    object["value"] = value;

    out = std::move(object);
}

void ASTLiteralBool::ToJSON(json::JSONValue &out) const
{
    json::JSONObject object;
    object["node_type"] = "ASTLiteralBool";
    object["value"] = value;

    out = std::move(object);
}

void ASTIdentifier::ToJSON(json::JSONValue &out) const
{
    json::JSONObject object;
    object["node_type"] = "ASTIdentifier";

    json::JSONObject type_name_json;
    type_name_json["is_global"] = name.is_global;

    json::JSONArray type_name_parts_array;

    for (const String &part : name.parts) {
        type_name_parts_array.PushBack(json::JSONString(part));
    }

    type_name_json["parts"] = std::move(type_name_parts_array);

    object["name"] = std::move(type_name_json);

    out = std::move(object);
}

void ASTInitializerExpr::ToJSON(json::JSONValue &out) const
{
    json::JSONObject object;
    object["node_type"] = "ASTInitializerExpr";

    json::JSONArray values_array;

    for (const RC<ASTExpr> &value : values) {
        json::JSONValue value_json;
        value->ToJSON(value_json);
        values_array.PushBack(std::move(value_json));
    }

    object["values"] = std::move(values_array);

    out = std::move(object);
}

void ASTTemplateArgument::ToJSON(json::JSONValue &out) const
{
    json::JSONObject object;
    object["node_type"] = "ASTTemplateArgument";

    if (type) {
        json::JSONValue type_json;
        type->ToJSON(type_json);
        object["type"] = std::move(type_json);
    } else if (expr) {
        json::JSONValue expr_json;
        expr->ToJSON(expr_json);
        object["expr"] = std::move(expr_json);
    }

    out = std::move(object);
}

String ASTType::Format(bool use_csharp_syntax) const
{
    if (use_csharp_syntax) {
        String csharp_type = "object";
        if (type_name.HasValue() && !type_name->parts.Empty()) {
            csharp_type = type_name->parts[type_name->parts.Size() - 1];
        }
        if (is_array) {
            csharp_type += "[]";
        }
        // Simple generic handling (only surface-level):
        if (is_template && template_arguments.Any()) {
            csharp_type += "<";
            for (int i = 0; i < template_arguments.Size(); ++i) {
                if (i > 0) {
                    csharp_type += ", ";
                }
                csharp_type += "object"; // Or produce a more robust mapping here
            }
            csharp_type += ">";
        }

        return csharp_type;
    }

    String prefix;
    if (is_const) prefix += "const ";
    if (is_volatile) prefix += "volatile ";
    if (is_inline) prefix += "inline ";
    if (is_static) prefix += "static ";
    if (is_thread_local) prefix += "thread_local ";
    if (is_virtual) prefix += "virtual ";
    if (is_constexpr) prefix += "constexpr ";

    // Build base name (e.g. "int", "MyClass", etc.)
    String base;
    if (type_name.HasValue()) {
        if (type_name->is_global) {
            base = "::";
        }
        for (SizeType i = 0; i < type_name->parts.Size(); i++) {
            if (i > 0) base += "::";
            base += type_name->parts[i];
        }
    } else {
        base = "/*unnamed_type*/";
    }

    // Handle arrays
    if (is_array) {
        base += "[]";
    }

    // Handle template parameters
    if (is_template && template_arguments.Any()) {
        base += "<";

        for (int i = 0; i < template_arguments.Size(); ++i) {
            if (i > 0) {
                base += ", ";
            }
            if (template_arguments[i]->type) {
                base += template_arguments[i]->type->Format(use_csharp_syntax);
            } else if (template_arguments[i]->expr) {
                base += "<expr>";
            }
        }
        
        base += ">";
    }

    // If we reference another type (pointer/reference), recursively build
    if (ptr_to) {
        // Combine pointer syntax with any qualifiers
        String ptr_qual;
        
        if (is_const) ptr_qual += " const";

        if (is_volatile) ptr_qual += " volatile";

        return ptr_to->FormatDecl("*" + ptr_qual, use_csharp_syntax);
    }
    else if (ref_to) {
        // Combine reference syntax with any qualifiers
        String ref_qual;
        
        if (is_const) ref_qual += " const";

        if (is_volatile) ref_qual += " volatile";

        return ref_to->FormatDecl("&" + ref_qual, use_csharp_syntax);
    }

    // Fall back to just prefix + base + decl_name
    return (prefix.Trimmed() + " " + base).Trimmed();
}

String ASTType::FormatDecl(const String &decl_name, bool use_csharp_syntax) const
{
    if (use_csharp_syntax) {
        return Format(true) + " " + decl_name;
    }

    String prefix;
    if (is_const) prefix += "const ";
    if (is_volatile) prefix += "volatile ";
    if (is_inline) prefix += "inline ";
    if (is_static) prefix += "static ";
    if (is_thread_local) prefix += "thread_local ";
    if (is_virtual) prefix += "virtual ";
    if (is_constexpr) prefix += "constexpr ";

    // Build base name (e.g. "int", "MyClass", etc.)
    String base;
    if (type_name.HasValue()) {
        if (type_name->is_global) {
            base = "::";
        }
        for (SizeType i = 0; i < type_name->parts.Size(); i++) {
            if (i > 0) base += "::";
            base += type_name->parts[i];
        }
    } else {
        base = "/*unnamed_type*/";
    }

    // Handle arrays
    if (is_array) {
        base += "[]";
    }

    // Handle template parameters
    if (is_template && template_arguments.Any()) {
        base += "<";

        for (int i = 0; i < template_arguments.Size(); ++i) {
            if (i > 0) {
                base += ", ";
            }
            if (template_arguments[i]->type) {
                base += template_arguments[i]->type->Format(use_csharp_syntax);
            } else if (template_arguments[i]->expr) {
                base += "<expr>";
            }
        }
        
        base += ">";
    }

    // If we reference another type (pointer/reference), recursively build
    if (ptr_to) {
        // Combine pointer syntax with any qualifiers
        String ptr_qual;
        
        if (is_const) ptr_qual += " const";

        if (is_volatile) ptr_qual += " volatile";

        return ptr_to->FormatDecl("*" + ptr_qual + " " + decl_name, use_csharp_syntax);
    }
    else if (ref_to) {
        // Combine reference syntax with any qualifiers
        String ref_qual;
        
        if (is_const) ref_qual += " const";

        if (is_volatile) ref_qual += " volatile";

        return ref_to->FormatDecl("&" + ref_qual + " " + decl_name, use_csharp_syntax);
    }

    // Fall back to just prefix + base + decl_name
    return (prefix.Trimmed() + " " + base + " " + decl_name).Trimmed();
}

void ASTType::ToJSON(json::JSONValue &out) const
{
    json::JSONObject object;
    object["node_type"] = "ASTType";

    object["is_const"] = is_const;
    object["is_volatile"] = is_volatile;
    object["is_virtual"] = is_virtual;
    object["is_inline"] = is_inline;
    object["is_static"] = is_static;
    object["is_thread_local"] = is_thread_local;
    object["is_constexpr"] = is_constexpr;
    object["is_lvalue_reference"] = is_lvalue_reference;
    object["is_rvalue_reference"] = is_rvalue_reference;
    object["is_pointer"] = is_pointer;
    object["is_array"] = is_array;
    object["is_template"] = is_template;
    object["is_function_pointer"] = is_function_pointer;
    object["is_function"] = is_function;

    if (is_array) {
        json::JSONValue array_expr_json;

        if (array_expr) {
            array_expr->ToJSON(array_expr_json);
        } else {
            array_expr_json = json::JSONNull();
        }

        object["array_expr"] = std::move(array_expr_json);
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

        for (const RC<ASTTemplateArgument> &template_argument : template_arguments) {
            json::JSONValue template_argument_json;
            template_argument->ToJSON(template_argument_json);
            template_arguments_array.PushBack(std::move(template_argument_json));
        }

        object["template_arguments"] = std::move(template_arguments_array);
    }

    out = std::move(object);
}

void ASTMemberDecl::ToJSON(json::JSONValue &out) const
{
    json::JSONObject object;
    object["node_type"] = "ASTMemberDecl";

    object["name"] = name;

    json::JSONValue type_json;
    type->ToJSON(type_json);
    object["type"] = std::move(type_json);

    if (value) {
        json::JSONValue value_json;
        value->ToJSON(value_json);
        object["value"] = std::move(value_json);
    } else {
        object["value"] = json::JSONNull();
    }

    out = std::move(object);
}

String ASTFunctionType::Format(bool use_csharp_syntax) const
{
    if (use_csharp_syntax) {
        String params;

        for (SizeType i = 0; i < parameters.Size(); ++i) {
            if (i > 0) {
                params += ", ";
            }
            params += parameters[i]->type->Format(use_csharp_syntax);
        }

        String return_type_string = return_type->Format(use_csharp_syntax);

        String result = return_type_string + " " + "(" + params + ")";

        return result;
    }

    String params;

    for (SizeType i = 0; i < parameters.Size(); ++i) {
        if (i > 0) {
            params += ", ";
        }
        params += parameters[i]->type->FormatDecl(parameters[i]->name, use_csharp_syntax);
    }

    String return_type_string = return_type->Format(use_csharp_syntax);

    String result = return_type_string + "(" + params + ")";

    if (is_const_method) result += " const";

    if (is_noexcept_method) result += " noexcept";

    if (is_rvalue_method) result += " &&";
    else if (is_lvalue_method) result += " &";

    if (is_override_method) result += " override";

    if (is_pure_virtual_method) result += " = 0";
    else if (is_defaulted_method) result += " = default";
    else if (is_deleted_method) result += " = delete";

    return result;
}

String ASTFunctionType::FormatDecl(const String &decl_name, bool use_csharp_syntax) const
{
    if (use_csharp_syntax) {
        String params;

        for (SizeType i = 0; i < parameters.Size(); ++i) {
            if (i > 0) {
                params += ", ";
            }
            params += parameters[i]->type->Format(use_csharp_syntax);
        }

        String return_type_string = return_type->Format(use_csharp_syntax);

        String result = return_type_string + " " + decl_name + "(" + params + ")";

        return result;
    }

    String params;

    for (SizeType i = 0; i < parameters.Size(); ++i) {
        if (i > 0) {
            params += ", ";
        }
        params += parameters[i]->type->FormatDecl(parameters[i]->name, use_csharp_syntax);
    }

    String return_type_string = return_type->Format(use_csharp_syntax);

    String result = return_type_string + " " + decl_name + "(" + params + ")";

    if (is_const_method) result += " const";

    if (is_noexcept_method) result += " noexcept";

    if (is_rvalue_method) result += " &&";
    else if (is_lvalue_method) result += " &";

    if (is_override_method) result += " override";

    if (is_pure_virtual_method) result += " = 0";
    else if (is_defaulted_method) result += " = default";
    else if (is_deleted_method) result += " = delete";

    return result;
}

void ASTFunctionType::ToJSON(json::JSONValue &out) const
{
    json::JSONValue type_json;
    ASTType::ToJSON(type_json);

    json::JSONObject object;
    object["node_type"] = "ASTFunctionType";

    object["is_const_method"] = is_const_method;
    object["is_override_method"] = is_override_method;
    object["is_noexcept_method"] = is_noexcept_method;
    object["is_defaulted_method"] = is_defaulted_method;
    object["is_deleted_method"] = is_deleted_method;
    object["is_pure_virtual_method"] = is_pure_virtual_method;
    object["is_rvalue_method"] = is_rvalue_method;
    object["is_lvalue_method"] = is_lvalue_method;

    json::JSONValue return_type_json;
    return_type->ToJSON(return_type_json);
    object["return_type"] = std::move(return_type_json);

    json::JSONArray parameters_array;

    for (const RC<ASTMemberDecl> &parameter : parameters) {
        json::JSONValue parameter_json;
        parameter->ToJSON(parameter_json);
        parameters_array.PushBack(std::move(parameter_json));
    }

    object["parameters"] = std::move(parameters_array);

    out = type_json.ToObject().Merge(std::move(object));
}

#pragma endregion JSON conversion

Parser::Parser(
    TokenStream *token_stream,
    CompilationUnit *compilation_unit
) : m_token_stream(token_stream),
    m_compilation_unit(compilation_unit),
    m_template_argument_depth(0)
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
        m_token_stream->SetPosition(0);
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

QualifiedName Parser::ReadQualifiedName()
{
    QualifiedName qualified_name;

    if (Match(TK_DOUBLE_COLON, true)) {
        qualified_name.is_global = true;
    }

    while (true) {
        Token ident = Expect(TK_IDENT, true);
        qualified_name.parts.PushBack(ident.GetValue());

        if (!Match(TK_DOUBLE_COLON, true)) {
            break;
        }
    }

    return qualified_name;
}

RC<ASTExpr> Parser::ParseExpr()
{
    RC<ASTExpr> term = ParseTerm();

    if (Match(TK_OPERATOR, false)) {
        // drop out of expression, return to parent call
        if (m_template_argument_depth > 0) {
            if (MatchOperator(">", false) || MatchOperator(">>", false)) {
                return term;
            }
        }

        term = ParseBinaryExpr(0, term);
    }

    if (Match(TK_QUESTION_MARK)) {
        term = ParseTernaryExpr(term);
    }

    return term;
}

RC<ASTExpr> Parser::ParseTerm()
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

    RC<ASTExpr> expr;

    if (Match(TK_OPEN_PARENTH)) {
        expr = ParseParentheses();
    } else if (Match(TK_STRING)) {
        expr = ParseLiteralString();
    } else if (Match(TK_INTEGER)) {
        expr = ParseLiteralInt();
    } else if (Match(TK_FLOAT)) {
        expr = ParseLiteralFloat();
    } else if (MatchIdentifier("true", true)) {
        auto value = MakeRefCountedPtr<ASTLiteralBool>();
        value->value = true;
        expr = value;
    } else if (MatchIdentifier("false", true)) {
        auto value = MakeRefCountedPtr<ASTLiteralBool>();
        value->value = false;
        expr = value;
    } else if (Match(TK_IDENT) || Match(TK_DOUBLE_COLON)) {
        expr = ParseIdentifier();
    } else if (Match(TK_OPERATOR)) {
        expr = ParseUnaryExprPrefix();
    } else if (Match(TK_OPEN_BRACE)) {
        expr = ParseInitializerExpr();
    } else {
        m_compilation_unit->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_unexpected_token,
            token.GetLocation(),
            token.GetValue()
        ));

        if (m_token_stream->HasNext()) {
            m_token_stream->Next();
        }

        return nullptr;
    }

    Token operator_token = Token::EMPTY;

    while (expr != nullptr && (
        ((operator_token = Match(TK_OPERATOR)) && Operator::IsUnaryOperator(operator_token.GetValue(), OperatorType::POSTFIX))
    ))
    {
        if (operator_token) {
            expr = ParseUnaryExprPostfix(expr);
            operator_token = Token::EMPTY;
        }
    }

    return expr;
}

RC<ASTExpr> Parser::ParseUnaryExprPrefix()
{
    // read the operator token
    if (Token token = Expect(TK_OPERATOR, true)) {
        const Operator *op = nullptr;
        if (Operator::IsUnaryOperator(token.GetValue(), op)) {
            if (RC<ASTExpr> term = ParseTerm()) {
                RC<ASTUnaryExpr> expr = MakeRefCountedPtr<ASTUnaryExpr>();
                expr->expr = term;
                expr->op = op;
                expr->is_prefix = true;

                return expr;
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

RC<ASTExpr> Parser::ParseUnaryExprPostfix(const RC<ASTExpr> &inner_expr)
{
    // read the operator token
    if (Token token = Expect(TK_OPERATOR, true)) {
        const Operator *op = nullptr;
        if (Operator::IsUnaryOperator(token.GetValue(), op)) {
            RC<ASTUnaryExpr> expr = MakeRefCountedPtr<ASTUnaryExpr>();
            expr->expr = inner_expr;
            expr->op = op;
            expr->is_prefix = false;

            return expr;
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

RC<ASTExpr> Parser::ParseBinaryExpr(int expr_precedence, RC<ASTExpr> left)
{
    while (true) {
        // get precedence
        const Operator *op = nullptr;
        int precedence = OperatorPrecedence(op);
        if (precedence < expr_precedence) {
            return left;
        }

        // read the operator token
        Token token = Expect(TK_OPERATOR, true);

        if (auto right = ParseTerm()) {
            // next part of expression's precedence
            const Operator *next_op = nullptr;
            
            int next_precedence = OperatorPrecedence(next_op);
            if (precedence < next_precedence) {
                right = ParseBinaryExpr(precedence + 1, right);
                if (right == nullptr) {
                    return nullptr;
                }
            }

            RC<ASTBinExpr> bin_expr = MakeRefCountedPtr<ASTBinExpr>();
            bin_expr->left = left;
            bin_expr->right = right;
            bin_expr->op = op;

            left = std::move(bin_expr);
        }
    }

    return nullptr;
}

RC<ASTExpr> Parser::ParseTernaryExpr(const RC<ASTExpr> &conditional)
{
    if (Token token = Expect(TK_QUESTION_MARK, true)) {
        // parse next ('true' part)

        RC<ASTExpr> true_expr = ParseExpr();

        if (true_expr == nullptr) {
            return nullptr;
        }

        if (!Expect(TK_COLON, true)) {
            return nullptr;
        }

        RC<ASTExpr> false_expr = ParseExpr();

        if (false_expr == nullptr) {
            return nullptr;
        }

        RC<ASTTernaryExpr> ternary = MakeRefCountedPtr<ASTTernaryExpr>();
        ternary->conditional = conditional;
        ternary->true_expr = true_expr;
        ternary->false_expr = false_expr;

        return ternary;
    }

    return nullptr;
}

RC<ASTExpr> Parser::ParseParentheses()
{
    SourceLocation location = CurrentLocation();

    Expect(TK_OPEN_PARENTH, true);

    RC<ASTExpr> expr = ParseExpr();

    Expect(TK_CLOSE_PARENTH, true);

    return expr;
}

RC<ASTExpr> Parser::ParseLiteralString()
{
    if (Token token = Expect(TK_STRING, true)) {
        RC<ASTLiteralString> value = MakeRefCountedPtr<ASTLiteralString>();
        value->value = token.GetValue();
    }

    return nullptr;
}

RC<ASTExpr> Parser::ParseLiteralInt()
{
    if (Token token = Expect(TK_INTEGER, true)) {
        RC<ASTLiteralInt> value = MakeRefCountedPtr<ASTLiteralInt>();
        value->value = StringUtil::Parse<int>(token.GetValue());
    }

    return nullptr;
}

RC<ASTExpr> Parser::ParseLiteralFloat()
{
    if (Token token = Expect(TK_FLOAT, true)) {
        RC<ASTLiteralFloat> value = MakeRefCountedPtr<ASTLiteralFloat>();
        value->value = StringUtil::Parse<double>(token.GetValue());
    }

    return nullptr;
}

RC<ASTIdentifier> Parser::ParseIdentifier()
{
    RC<ASTIdentifier> identifier = MakeRefCountedPtr<ASTIdentifier>();
    identifier->name = ReadQualifiedName();

    return identifier;
}

RC<ASTInitializerExpr> Parser::ParseInitializerExpr()
{
    RC<ASTInitializerExpr> initializer_expr = MakeRefCountedPtr<ASTInitializerExpr>();

    if (Expect(TK_OPEN_BRACE, true)) {
        if (!Match(TK_CLOSE_BRACE, false)) {
            while (true) {
                initializer_expr->values.PushBack(ParseExpr());

                if (!Match(TK_COMMA, true)) {
                    break;
                }
            }
        }
        
        Expect(TK_CLOSE_BRACE, true);
    }

    return initializer_expr;
}

RC<ASTMemberDecl> Parser::ParseMemberDecl()
{
    RC<ASTMemberDecl> member_decl = MakeRefCountedPtr<ASTMemberDecl>();

    bool is_inline = false;
    bool is_virtual = false;
    bool is_static = false;
    bool is_thread_local = false;
    bool is_constexpr = false;
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
        } else if (MatchIdentifier("thread_local", true)) {
            is_thread_local = true;
        } else if (MatchIdentifier("constexpr", true)) {
            is_constexpr = true;
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
        RC<ASTType> array_type = MakeRefCountedPtr<ASTType>();
        array_type->array_of = member_decl->type;
        array_type->is_array = true;

        if (!Match(TK_CLOSE_BRACKET, false)) {
            array_type->array_expr = ParseExpr();
        }

        member_decl->type = array_type;

        Expect(TK_CLOSE_BRACKET, true);
    }

    if (!member_decl->type->is_function) {
        if (MatchOperator("=", true)) {
            member_decl->value = ParseExpr();
        } else if (Match(TK_OPEN_BRACE, false)) {
            member_decl->value = ParseInitializerExpr();
        }
    }

    member_decl->type->is_virtual = is_virtual;
    member_decl->type->is_inline = is_inline;
    member_decl->type->is_static = is_static;
    member_decl->type->is_thread_local = is_thread_local;
    member_decl->type->is_constexpr = is_constexpr;

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
        } if (MatchIdentifier("constexpr", true)) {
            root_type->is_constexpr = true;
        } else {
            keep_reading = false;
        }
    }

    root_type->type_name = ReadQualifiedName();

    if (MatchOperator("<<", false)) {
        m_token_stream->Pop();

        for (int i = 0; i < 2; i++) {
            m_token_stream->Push(Token(TK_OPERATOR, "<", m_token_stream->Peek().GetLocation()), /* insert_at_position */ true);
        }
    }

    if (MatchOperator("<", true)) {
        ++m_template_argument_depth;

        root_type->is_template = true;

        if (MatchOperator(">>", false)) {
            m_token_stream->Pop();

            for (int i = 0; i < 2; i++) {
                m_token_stream->Push(Token(TK_OPERATOR, ">", m_token_stream->Peek().GetLocation()), /* insert_at_position */ true);
            }
        }

        if (!MatchOperator(">", false)) {
            while (true) {
                RC<ASTTemplateArgument> template_argument = MakeRefCountedPtr<ASTTemplateArgument>();

                if (Match(TK_INTEGER, false)) {
                    template_argument->expr = ParseLiteralInt();
                } else if (Match(TK_FLOAT, false)) {
                    template_argument->expr = ParseLiteralFloat();
                } else if (Match(TK_STRING, false)) {
                    template_argument->expr = ParseLiteralString();
                } else if (Match(TK_OPEN_PARENTH, false)) {
                    template_argument->expr = ParseExpr();
                } else {
                    template_argument->type = ParseType();
                }

                root_type->template_arguments.PushBack(template_argument);

                if (!Match(TK_COMMA, true)) {
                    break;
                }
            }
        }

        if (MatchOperator(">>", false)) {
            m_token_stream->Pop();

            for (int i = 0; i < 2; i++) {
                m_token_stream->Push(Token(TK_OPERATOR, ">", m_token_stream->Peek().GetLocation()), /* insert_at_position */ true);
            }
        }

        if (ExpectOperator(">", true)) {
            --m_template_argument_depth;
        }
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
                func_type->parameters.PushBack(ParseMemberDecl());

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

        // Read = default, = delete or = 0
        if (MatchOperator("=", true)) {
            if (MatchIdentifier("default", true)) {
                func_type->is_defaulted_method = true;
            } else if (MatchIdentifier("delete", true)) {
                func_type->is_deleted_method = true;
            } else if (Token integer_token = Match(TK_INTEGER, true)) {
                if (integer_token.GetValue() == "0") {
                    func_type->is_pure_virtual_method = true;
                } else {
                    m_compilation_unit->GetErrorList().AddError(CompilerError(
                        LEVEL_ERROR,
                        Msg_unexpected_character,
                        integer_token.GetLocation(),
                        integer_token.GetValue()
                    ));
                }
            }
        } else if (Match(TK_OPEN_BRACE, false)) {
            // @TODO Implement function body parsing
        }
    }

    return func_type;
}


} // namespace hyperion::buildtool