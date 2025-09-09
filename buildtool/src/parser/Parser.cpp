#include <parser/Parser.hpp>
#include <parser/Lexer.hpp>

#include <analyzer/Analyzer.hpp>
#include <analyzer/Definitions.hpp>

#include <core/utilities/Result.hpp>

#include <core/json/JSON.hpp>
#include <core/utilities/StringUtil.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion::buildtool {

HYP_DECLARE_LOG_CHANNEL(BuildTool);
HYP_DEFINE_LOG_SUBCHANNEL(Parser, BuildTool);

#pragma region CSharp type mapping

const ASTType* ExtractInnerType(const ASTType* type)
{
    if (type->isPointer)
    {
        return ExtractInnerType(type->ptrTo.Get());
    }
    else if (type->isLvalueReference || type->isRvalueReference)
    {
        return ExtractInnerType(type->refTo.Get());
    }
    else
    {
        return type;
    }
}

TResult<CSharpTypeMapping> MapToCSharpType(const Analyzer& analyzer, const ASTType* type)
{
    if (type->isPointer && type->ptrTo->IsVoid())
    {
        return CSharpTypeMapping { "IntPtr", "ReadIntPtr" };
    }

    if (type->isArray)
    {
        if (!type->arrayOf)
        {
            return HYP_MAKE_ERROR(Error, "Array type has no inner type");
        }

        if (auto res = MapToCSharpType(analyzer, type->arrayOf.Get()); res.HasError())
        {
            return res.GetError();
        }
        else
        {
            return CSharpTypeMapping { res.GetValue().typeName + "[]" };
        }
    }

    type = ExtractInnerType(type);
    Assert(type != nullptr);

    if (type->typeName.HasValue())
    {
        if (type->typeName->parts.Empty())
        {
            return HYP_MAKE_ERROR(Error, "Type name has no parts");
        }

        static const HashMap<String, CSharpTypeMapping> mapping {
            { "int", { "int", "ReadInt32" } },
            { "float", { "float", "ReadFloat" } },
            { "double", { "double", "ReadDouble" } },
            { "bool", { "bool", "ReadBool" } },
            { "void", { "void" } },
            { "char", { "char" } },
            { "uint8", { "byte", "ReadUInt8" } },
            { "uint16", { "ushort", "ReadUInt16" } },
            { "uint32", { "uint", "ReadUInt32" } },
            { "uint64", { "ulong", "ReadUInt64" } },
            { "int8", { "sbyte", "ReadInt8" } },
            { "int16", { "short", "ReadInt16" } },
            { "int32", { "int", "ReadInt32" } },
            { "int64", { "long", "ReadInt64" } },
            { "uintptr_t", { "IntPtr", "ReadIntPtr" } },
            { "string", { "string", "ReadString" } },
            { "String", { "string", "ReadString" } },
            { "ANSIString", { "string", "ReadString" } },
            { "UTF8StringView", { "string", "ReadString" } },
            { "ANSIStringView", { "string", "ReadString" } },
            { "FilePath", { "string", "ReadString" } },
            { "ByteBuffer", { "byte[]", "ReadByteBuffer" } },
            { "ObjId", { "ObjIdBase", "ReadId" } },
            { "Name", { "Name", "ReadName" } },
            { "WeakName", { "Name", "ReadName" } },
            { "HypObjectBase", { "HypObject" } }, // Base object class - C# uses HypObject.
            { "AnyHandle", { "object" } },
            { "AnyRef", { "object" } },
            { "ConstAnyRef", { "object" } }
        };

        const String typeNameString = type->typeName->ToString(/* includeNamespace */ false);

        auto it = mapping.Find(typeNameString);

        if (it != mapping.End())
        {
            return it->second;
        }

        if (type->isTemplate)
        {
            const String templateName = type->typeName->parts.Back();

            if (templateName == "Array")
            {
                return CSharpTypeMapping { "Array" };
            }

            // if (templateName == "ObjId")
            // {
            //     return
            // }

            if (templateName == "RC"
                || templateName == "Handle"
                || templateName == "EnumFlags")
            {
                if (type->templateArguments.Empty())
                {
                    return HYP_MAKE_ERROR(Error, "Type missing template argument");
                }

                if (!type->templateArguments[0]->type)
                {
                    return HYP_MAKE_ERROR(Error, "Type template argument is not a type");
                }

                return MapToCSharpType(analyzer, type->templateArguments[0]->type.Get());
            }

            HYP_LOG(Parser, Error, "Template type is unable to be mapped to a C# type: {}  (type name string = {})", type->Format(), typeNameString);

            return HYP_MAKE_ERROR(Error, "Template type is unable to be mapped to a C# type");
        }

        // Find a HypClass with the same name. HypObjects (classes deriving HypObject.cs) and structs with HypClassBinding
        // attribute can use custom overloads to try and get a specific method for reading the value.
        const HypClassDefinition* definition = analyzer.FindHypClassDefinition(typeNameString);

        if (definition)
        {
            if (definition->type == HypClassDefinitionType::CLASS)
            {
                return CSharpTypeMapping { typeNameString, HYP_FORMAT("ReadObject<{}>", definition->name) };
            }
            else if (definition->type == HypClassDefinitionType::STRUCT)
            {
                return CSharpTypeMapping { typeNameString, HYP_FORMAT("ReadStruct<{}>", definition->name) };
            }
        }

        return CSharpTypeMapping { typeNameString };
    }

    HYP_LOG(Parser, Error, "Type is unable to be mapped to a C# type: {}", type->Format());

    return HYP_MAKE_ERROR(Error, "Type is unable to be mapped to a C# type");
}

#pragma endregion CSharp type mapping

#pragma region QualifiedName

String QualifiedName::ToString(bool includeNamespace) const
{
    if (parts.Empty())
    {
        return "";
    }

    if (includeNamespace)
    {
        String result = parts[0];

        for (SizeType i = 1; i < parts.Size(); i++)
        {
            result += "::" + parts[i];
        }

        return result;
    }
    else
    {
        return parts.Back();
    }
}

#pragma endregion QualifiedName

#pragma region JSON conversion

void ASTUnaryExpr::ToJSON(json::JSONValue& out) const
{
    json::JSONObject object;
    object["node_type"] = "ASTUnaryExpr";

    json::JSONValue exprJson;
    expr->ToJSON(exprJson);
    object["expr"] = std::move(exprJson);

    object["op"] = op->LookupStringValue();
    object["is_prefix"] = isPrefix;

    out = std::move(object);
}

void ASTBinExpr::ToJSON(json::JSONValue& out) const
{
    json::JSONObject object;
    object["node_type"] = "ASTBinExpr";

    json::JSONValue leftJson;
    left->ToJSON(leftJson);
    object["left"] = std::move(leftJson);

    json::JSONValue rightJson;
    right->ToJSON(rightJson);
    object["right"] = std::move(rightJson);

    object["op"] = op->LookupStringValue();

    out = std::move(object);
}

void ASTTernaryExpr::ToJSON(json::JSONValue& out) const
{
    json::JSONObject object;
    object["node_type"] = "ASTTernaryExpr";

    json::JSONValue trueExprJson;
    trueExpr->ToJSON(trueExprJson);
    object["true_expr"] = std::move(trueExprJson);

    json::JSONValue falseExprJson;
    falseExpr->ToJSON(falseExprJson);
    object["false_expr"] = std::move(falseExprJson);

    json::JSONValue conditionalJson;
    conditional->ToJSON(conditionalJson);
    object["conditional"] = std::move(conditionalJson);

    out = std::move(object);
}

void ASTLiteralString::ToJSON(json::JSONValue& out) const
{
    json::JSONObject object;
    object["node_type"] = "ASTLiteralString";
    object["value"] = json::JSONString(value);

    out = std::move(object);
}

void ASTLiteralInt::ToJSON(json::JSONValue& out) const
{
    json::JSONObject object;
    object["node_type"] = "ASTLiteralInt";
    object["value"] = value;

    out = std::move(object);
}

void ASTLiteralFloat::ToJSON(json::JSONValue& out) const
{
    json::JSONObject object;
    object["node_type"] = "ASTLiteralFloat";
    object["value"] = value;

    out = std::move(object);
}

void ASTLiteralBool::ToJSON(json::JSONValue& out) const
{
    json::JSONObject object;
    object["node_type"] = "ASTLiteralBool";
    object["value"] = value;

    out = std::move(object);
}

void ASTIdentifier::ToJSON(json::JSONValue& out) const
{
    json::JSONObject object;
    object["node_type"] = "ASTIdentifier";

    json::JSONObject typeNameJson;
    typeNameJson["is_global"] = name.isGlobal;

    json::JSONArray typeNamePartsArray;

    for (const String& part : name.parts)
    {
        typeNamePartsArray.PushBack(json::JSONString(part));
    }

    typeNameJson["parts"] = std::move(typeNamePartsArray);

    object["name"] = std::move(typeNameJson);

    out = std::move(object);
}

void ASTInitializerExpr::ToJSON(json::JSONValue& out) const
{
    json::JSONObject object;
    object["node_type"] = "ASTInitializerExpr";

    json::JSONArray valuesArray;

    for (const RC<ASTExpr>& value : values)
    {
        json::JSONValue valueJson;
        value->ToJSON(valueJson);
        valuesArray.PushBack(std::move(valueJson));
    }

    object["values"] = std::move(valuesArray);

    out = std::move(object);
}

void ASTTemplateArgument::ToJSON(json::JSONValue& out) const
{
    json::JSONObject object;
    object["node_type"] = "ASTTemplateArgument";

    if (type)
    {
        json::JSONValue typeJson;
        type->ToJSON(typeJson);
        object["type"] = std::move(typeJson);
    }
    else if (expr)
    {
        json::JSONValue exprJson;
        expr->ToJSON(exprJson);
        object["expr"] = std::move(exprJson);
    }

    out = std::move(object);
}

String ASTType::Format(bool useCsharpSyntax) const
{
    if (useCsharpSyntax)
    {
        String csharpType = "object";
        if (typeName.HasValue() && !typeName->parts.Empty())
        {
            csharpType = typeName->parts[typeName->parts.Size() - 1];
        }
        if (isArray)
        {
            csharpType += "[]";
        }
        // Simple generic handling (only surface-level):
        if (isTemplate && templateArguments.Any())
        {
            csharpType += "<";
            for (int i = 0; i < templateArguments.Size(); ++i)
            {
                if (i > 0)
                {
                    csharpType += ", ";
                }
                csharpType += "object"; // Or produce a more robust mapping here
            }
            csharpType += ">";
        }

        return csharpType;
    }

    String prefix;
    if (isConst)
        prefix += "const ";
    if (isVolatile)
        prefix += "volatile ";
    if (isInline)
        prefix += "inline ";
    if (isStatic)
        prefix += "static ";
    if (isThreadLocal)
        prefix += "thread_local ";
    if (isVirtual)
        prefix += "virtual ";
    if (isConstexpr)
        prefix += "constexpr ";

    // Build base name (e.g. "int", "MyClass", etc.)
    String base;
    if (typeName.HasValue())
    {
        if (typeName->isGlobal)
        {
            base = "::";
        }
        for (SizeType i = 0; i < typeName->parts.Size(); i++)
        {
            if (i > 0)
                base += "::";
            base += typeName->parts[i];
        }
    }
    else
    {
        base = "/*unnamed_type*/";
    }

    // Handle arrays
    if (isArray)
    {
        base += "[]";
    }

    // Handle template parameters
    if (isTemplate && templateArguments.Any())
    {
        base += "<";

        for (int i = 0; i < templateArguments.Size(); ++i)
        {
            if (i > 0)
            {
                base += ", ";
            }
            if (templateArguments[i]->type)
            {
                base += templateArguments[i]->type->Format(useCsharpSyntax);
            }
            else if (templateArguments[i]->expr)
            {
                base += "<expr>";
            }
        }

        base += ">";
    }

    // If we reference another type (pointer/reference), recursively build
    if (ptrTo)
    {
        // Combine pointer syntax with any qualifiers
        String ptrQual;

        if (isConst)
            ptrQual += " const";

        if (isVolatile)
            ptrQual += " volatile";

        return ptrTo->FormatDecl("*" + ptrQual, useCsharpSyntax);
    }
    else if (refTo)
    {
        // Combine reference syntax with any qualifiers
        String refQual;

        if (isConst)
            refQual += " const";

        if (isVolatile)
            refQual += " volatile";

        return refTo->FormatDecl("&" + refQual, useCsharpSyntax);
    }

    // Fall back to just prefix + base + declName
    return (prefix.Trimmed() + " " + base).Trimmed();
}

String ASTType::FormatDecl(const String& declName, bool useCsharpSyntax) const
{
    if (useCsharpSyntax)
    {
        return Format(true) + " " + declName;
    }

    String prefix;
    if (isConst)
        prefix += "const ";
    if (isVolatile)
        prefix += "volatile ";
    if (isInline)
        prefix += "inline ";
    if (isStatic)
        prefix += "static ";
    if (isThreadLocal)
        prefix += "thread_local ";
    if (isVirtual)
        prefix += "virtual ";
    if (isConstexpr)
        prefix += "constexpr ";

    // Build base name (e.g. "int", "MyClass", etc.)
    String base;
    if (typeName.HasValue())
    {
        if (typeName->isGlobal)
        {
            base = "::";
        }
        for (SizeType i = 0; i < typeName->parts.Size(); i++)
        {
            if (i > 0)
                base += "::";
            base += typeName->parts[i];
        }
    }
    else
    {
        base = "/*unnamed_type*/";
    }

    // Handle arrays
    if (isArray)
    {
        base += "[]";
    }

    // Handle template parameters
    if (isTemplate && templateArguments.Any())
    {
        base += "<";

        for (int i = 0; i < templateArguments.Size(); ++i)
        {
            if (i > 0)
            {
                base += ", ";
            }
            if (templateArguments[i]->type)
            {
                base += templateArguments[i]->type->Format(useCsharpSyntax);
            }
            else if (templateArguments[i]->expr)
            {
                base += "<expr>";
            }
        }

        base += ">";
    }

    // If we reference another type (pointer/reference), recursively build
    if (ptrTo)
    {
        // Combine pointer syntax with any qualifiers
        String ptrQual;

        if (isConst)
            ptrQual += " const";

        if (isVolatile)
            ptrQual += " volatile";

        return ptrTo->FormatDecl("*" + ptrQual + " " + declName, useCsharpSyntax);
    }
    else if (refTo)
    {
        // Combine reference syntax with any qualifiers
        String refQual;

        if (isConst)
            refQual += " const";

        if (isVolatile)
            refQual += " volatile";

        return refTo->FormatDecl("&" + refQual + " " + declName, useCsharpSyntax);
    }

    // Fall back to just prefix + base + declName
    return (prefix.Trimmed() + " " + base + " " + declName).Trimmed();
}

void ASTType::ToJSON(json::JSONValue& out) const
{
    json::JSONObject object;
    object["node_type"] = "ASTType";

    object["is_const"] = isConst;
    object["is_volatile"] = isVolatile;
    object["is_virtual"] = isVirtual;
    object["is_inline"] = isInline;
    object["is_static"] = isStatic;
    object["is_thread_local"] = isThreadLocal;
    object["is_constexpr"] = isConstexpr;
    object["is_lvalue_reference"] = isLvalueReference;
    object["is_rvalue_reference"] = isRvalueReference;
    object["is_pointer"] = isPointer;
    object["is_array"] = isArray;
    object["is_template"] = isTemplate;
    object["is_function_pointer"] = isFunctionPointer;
    object["is_function"] = isFunction;

    if (isArray)
    {
        json::JSONValue arrayExprJson;

        if (arrayExpr)
        {
            arrayExpr->ToJSON(arrayExprJson);
        }
        else
        {
            arrayExprJson = json::JSONNull();
        }

        object["array_expr"] = std::move(arrayExprJson);
    }

    if (ptrTo)
    {
        json::JSONValue ptrToJson;
        ptrTo->ToJSON(ptrToJson);
        object["ptr_to"] = std::move(ptrToJson);
    }

    if (refTo)
    {
        json::JSONValue refToJson;
        refTo->ToJSON(refToJson);
        object["ref_to"] = std::move(refToJson);
    }

    if (typeName.HasValue())
    {
        json::JSONObject typeNameJson;
        typeNameJson["is_global"] = typeName->isGlobal;

        json::JSONArray typeNamePartsArray;

        for (const String& part : typeName->parts)
        {
            typeNamePartsArray.PushBack(json::JSONString(part));
        }

        typeNameJson["parts"] = std::move(typeNamePartsArray);

        object["type_name"] = std::move(typeNameJson);
    }

    if (isTemplate)
    {
        json::JSONArray templateArgumentsArray;

        for (const RC<ASTTemplateArgument>& templateArgument : templateArguments)
        {
            json::JSONValue templateArgumentJson;
            templateArgument->ToJSON(templateArgumentJson);
            templateArgumentsArray.PushBack(std::move(templateArgumentJson));
        }

        object["template_arguments"] = std::move(templateArgumentsArray);
    }

    out = std::move(object);
}

void ASTMemberDecl::ToJSON(json::JSONValue& out) const
{
    json::JSONObject object;
    object["node_type"] = "ASTMemberDecl";

    object["name"] = name;

    json::JSONValue typeJson;
    type->ToJSON(typeJson);
    object["type"] = std::move(typeJson);

    if (value)
    {
        json::JSONValue valueJson;
        value->ToJSON(valueJson);
        object["value"] = std::move(valueJson);
    }
    else
    {
        object["value"] = json::JSONNull();
    }

    out = std::move(object);
}

String ASTFunctionType::Format(bool useCsharpSyntax) const
{
    if (useCsharpSyntax)
    {
        String params;

        for (SizeType i = 0; i < parameters.Size(); ++i)
        {
            if (i > 0)
            {
                params += ", ";
            }
            params += parameters[i]->type->Format(useCsharpSyntax);
        }

        String returnTypeString = returnType->Format(useCsharpSyntax);

        String result = returnTypeString + " " + "(" + params + ")";

        return result;
    }

    String params;

    for (SizeType i = 0; i < parameters.Size(); ++i)
    {
        if (i > 0)
        {
            params += ", ";
        }
        params += parameters[i]->type->FormatDecl(parameters[i]->name, useCsharpSyntax);
    }

    String returnTypeString = returnType->Format(useCsharpSyntax);

    String result = returnTypeString + "(" + params + ")";

    if (isConstMethod)
        result += " const";

    if (isNoexceptMethod)
        result += " noexcept";

    if (isRvalueMethod)
        result += " &&";
    else if (isLvalueMethod)
        result += " &";

    if (isOverrideMethod)
        result += " override";

    if (isPureVirtualMethod)
        result += " = 0";
    else if (isDefaultedMethod)
        result += " = default";
    else if (isDeletedMethod)
        result += " = delete";

    return result;
}

String ASTFunctionType::FormatDecl(const String& declName, bool useCsharpSyntax) const
{
    if (useCsharpSyntax)
    {
        String params;

        for (SizeType i = 0; i < parameters.Size(); ++i)
        {
            if (i > 0)
            {
                params += ", ";
            }
            params += parameters[i]->type->Format(useCsharpSyntax);
        }

        String returnTypeString = returnType->Format(useCsharpSyntax);

        String result = returnTypeString + " " + declName + "(" + params + ")";

        return result;
    }

    String params;

    for (SizeType i = 0; i < parameters.Size(); ++i)
    {
        if (i > 0)
        {
            params += ", ";
        }
        params += parameters[i]->type->FormatDecl(parameters[i]->name, useCsharpSyntax);
    }

    String returnTypeString = returnType->Format(useCsharpSyntax);

    String result = returnTypeString + " " + declName + "(" + params + ")";

    if (isConstMethod)
        result += " const";

    if (isNoexceptMethod)
        result += " noexcept";

    if (isRvalueMethod)
        result += " &&";
    else if (isLvalueMethod)
        result += " &";

    if (isOverrideMethod)
        result += " override";

    if (isPureVirtualMethod)
        result += " = 0";
    else if (isDefaultedMethod)
        result += " = default";
    else if (isDeletedMethod)
        result += " = delete";

    return result;
}

void ASTFunctionType::ToJSON(json::JSONValue& out) const
{
    json::JSONValue typeJson;
    ASTType::ToJSON(typeJson);

    json::JSONObject object;
    object["node_type"] = "ASTFunctionType";

    object["is_const_method"] = isConstMethod;
    object["is_override_method"] = isOverrideMethod;
    object["is_noexcept_method"] = isNoexceptMethod;
    object["is_defaulted_method"] = isDefaultedMethod;
    object["is_deleted_method"] = isDeletedMethod;
    object["is_pure_virtual_method"] = isPureVirtualMethod;
    object["is_rvalue_method"] = isRvalueMethod;
    object["is_lvalue_method"] = isLvalueMethod;

    json::JSONValue returnTypeJson;
    returnType->ToJSON(returnTypeJson);
    object["return_type"] = std::move(returnTypeJson);

    json::JSONArray parametersArray;

    for (const RC<ASTMemberDecl>& parameter : parameters)
    {
        json::JSONValue parameterJson;
        parameter->ToJSON(parameterJson);
        parametersArray.PushBack(std::move(parameterJson));
    }

    object["parameters"] = std::move(parametersArray);

    out = json::JSONObject(typeJson.ToObject()).Merge(std::move(object));
}

#pragma endregion JSON conversion

Parser::Parser(TokenStream* tokenStream, CompilationUnit* compilationUnit)
    : m_tokenStream(tokenStream),
      m_compilationUnit(compilationUnit),
      m_templateArgumentDepth(0)
{
    if (m_compilationUnit->GetPreprocessorDefinitions().Any())
    {
        TokenStream newTokenStream { m_tokenStream->GetInfo() };

        while (m_tokenStream->HasNext())
        {
            Token token = m_tokenStream->Next();

            if (token.GetTokenClass() == TokenClass::TK_IDENT)
            {
                auto preprocessorDefinitionsIt = m_compilationUnit->GetPreprocessorDefinitions().Find(token.GetValue());

                if (preprocessorDefinitionsIt != m_compilationUnit->GetPreprocessorDefinitions().End())
                {
                    SourceFile macroSourceFile("<macro>", preprocessorDefinitionsIt->second.Size());

                    ByteBuffer temp(preprocessorDefinitionsIt->second.Size(), preprocessorDefinitionsIt->second.Data());
                    macroSourceFile.ReadIntoBuffer(temp);

                    TokenStream macroTokenStream { TokenStreamInfo { "<input>" } };
                    CompilationUnit macroCompilationUnit;

                    Lexer macroLexer(SourceStream(&macroSourceFile), &macroTokenStream, &macroCompilationUnit);
                    macroLexer.Analyze();

                    while (macroTokenStream.HasNext())
                    {
                        Token macroToken = macroTokenStream.Next();

                        newTokenStream.Push(macroToken);
                    }

                    continue;
                }
            }

            newTokenStream.Push(token);
        }

        *m_tokenStream = std::move(newTokenStream);
        m_tokenStream->SetPosition(0);
    }
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
            errorStr = Token::TokenTypeToString(m_tokenStream->Peek().GetTokenClass());
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

Token Parser::MatchIdentifier(const UTF8StringView& value, bool read)
{
    Token ident = Match(TK_IDENT, false);

    if (value.Size() != 0)
    {
        if (ident && ident.GetValue() == value)
        {
            if (read && m_tokenStream->HasNext())
            {
                m_tokenStream->Next();
            }

            return ident;
        }
    }
    else
    {
        if (read && m_tokenStream->HasNext())
        {
            m_tokenStream->Next();
        }
    }

    return Token::EMPTY;
}

Token Parser::ExpectIdentifier(const UTF8StringView& value, bool read)
{
    Token ident = MatchIdentifier(value, read);

    if (!ident)
    {
        const SourceLocation location = CurrentLocation();

        m_compilationUnit->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_expected_identifier,
            location));
    }

    return ident;
}

bool Parser::ExpectEndOfStmt()
{
    const SourceLocation location = CurrentLocation();

    if (!Match(TK_SEMICOLON, true) && !Match(TK_CLOSE_BRACE, false))
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
    while (Match(TK_SEMICOLON, true))
        ;
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

QualifiedName Parser::ReadQualifiedName()
{
    QualifiedName qualifiedName;

    if (Match(TK_DOUBLE_COLON, true))
    {
        qualifiedName.isGlobal = true;
    }

    while (true)
    {
        Token ident = Expect(TK_IDENT, true);
        qualifiedName.parts.PushBack(ident.GetValue());

        if (!Match(TK_DOUBLE_COLON, true))
        {
            break;
        }
    }

    return qualifiedName;
}

RC<ASTExpr> Parser::ParseExpr()
{
    RC<ASTExpr> term = ParseTerm();

    if (Match(TK_OPERATOR, false))
    {
        // drop out of expression, return to parent call
        if (m_templateArgumentDepth > 0)
        {
            if (MatchOperator(">", false) || MatchOperator(">>", false))
            {
                return term;
            }
        }

        term = ParseBinaryExpr(0, term);
    }

    if (Match(TK_QUESTION_MARK))
    {
        term = ParseTernaryExpr(term);
    }

    return term;
}

RC<ASTExpr> Parser::ParseTerm()
{
    Token token = m_tokenStream->Peek();

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

    RC<ASTExpr> expr;

    if (Match(TK_OPEN_PARENTH))
    {
        expr = ParseParentheses();
    }
    else if (Match(TK_STRING))
    {
        expr = ParseLiteralString();
    }
    else if (Match(TK_INTEGER))
    {
        expr = ParseLiteralInt();
    }
    else if (Match(TK_FLOAT))
    {
        expr = ParseLiteralFloat();
    }
    else if (MatchIdentifier("true", true))
    {
        auto value = MakeRefCountedPtr<ASTLiteralBool>();
        value->value = true;
        expr = value;
    }
    else if (MatchIdentifier("false", true))
    {
        auto value = MakeRefCountedPtr<ASTLiteralBool>();
        value->value = false;
        expr = value;
    }
    else if (Match(TK_IDENT) || Match(TK_DOUBLE_COLON))
    {
        expr = ParseIdentifier();
    }
    else if (Match(TK_OPERATOR))
    {
        expr = ParseUnaryExprPrefix();
    }
    else if (Match(TK_OPEN_BRACE))
    {
        expr = ParseInitializerExpr();
    }
    else
    {
        m_compilationUnit->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_unexpected_token,
            token.GetLocation(),
            token.GetValue()));

        if (m_tokenStream->HasNext())
        {
            m_tokenStream->Next();
        }

        return nullptr;
    }

    Token operatorToken = Token::EMPTY;

    while (expr != nullptr && (((operatorToken = Match(TK_OPERATOR)) && Operator::IsUnaryOperator(operatorToken.GetValue(), OperatorType::POSTFIX))))
    {
        if (operatorToken)
        {
            expr = ParseUnaryExprPostfix(expr);
            operatorToken = Token::EMPTY;
        }
    }

    return expr;
}

RC<ASTExpr> Parser::ParseUnaryExprPrefix()
{
    // read the operator token
    if (Token token = Expect(TK_OPERATOR, true))
    {
        const Operator* op = nullptr;
        if (Operator::IsUnaryOperator(token.GetValue(), op))
        {
            if (RC<ASTExpr> term = ParseTerm())
            {
                RC<ASTUnaryExpr> expr = MakeRefCountedPtr<ASTUnaryExpr>();
                expr->expr = term;
                expr->op = op;
                expr->isPrefix = true;

                return expr;
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

RC<ASTExpr> Parser::ParseUnaryExprPostfix(const RC<ASTExpr>& innerExpr)
{
    // read the operator token
    if (Token token = Expect(TK_OPERATOR, true))
    {
        const Operator* op = nullptr;
        if (Operator::IsUnaryOperator(token.GetValue(), op))
        {
            RC<ASTUnaryExpr> expr = MakeRefCountedPtr<ASTUnaryExpr>();
            expr->expr = innerExpr;
            expr->op = op;
            expr->isPrefix = false;

            return expr;
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

RC<ASTExpr> Parser::ParseBinaryExpr(int exprPrecedence, RC<ASTExpr> left)
{
    while (true)
    {
        // get precedence
        const Operator* op = nullptr;
        int precedence = OperatorPrecedence(op);
        if (precedence < exprPrecedence)
        {
            return left;
        }

        // read the operator token
        Token token = Expect(TK_OPERATOR, true);

        if (auto right = ParseTerm())
        {
            // next part of expression's precedence
            const Operator* nextOp = nullptr;

            int nextPrecedence = OperatorPrecedence(nextOp);
            if (precedence < nextPrecedence)
            {
                right = ParseBinaryExpr(precedence + 1, right);
                if (right == nullptr)
                {
                    return nullptr;
                }
            }

            RC<ASTBinExpr> binExpr = MakeRefCountedPtr<ASTBinExpr>();
            binExpr->left = left;
            binExpr->right = right;
            binExpr->op = op;

            left = std::move(binExpr);
        }
    }

    return nullptr;
}

RC<ASTExpr> Parser::ParseTernaryExpr(const RC<ASTExpr>& conditional)
{
    if (Token token = Expect(TK_QUESTION_MARK, true))
    {
        // parse next ('true' part)

        RC<ASTExpr> trueExpr = ParseExpr();

        if (trueExpr == nullptr)
        {
            return nullptr;
        }

        if (!Expect(TK_COLON, true))
        {
            return nullptr;
        }

        RC<ASTExpr> falseExpr = ParseExpr();

        if (falseExpr == nullptr)
        {
            return nullptr;
        }

        RC<ASTTernaryExpr> ternary = MakeRefCountedPtr<ASTTernaryExpr>();
        ternary->conditional = conditional;
        ternary->trueExpr = trueExpr;
        ternary->falseExpr = falseExpr;

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
    if (Token token = Expect(TK_STRING, true))
    {
        RC<ASTLiteralString> value = MakeRefCountedPtr<ASTLiteralString>();
        value->value = token.GetValue();

        return value;
    }

    return nullptr;
}

RC<ASTExpr> Parser::ParseLiteralInt()
{
    if (Token token = Expect(TK_INTEGER, true))
    {
        RC<ASTLiteralInt> value = MakeRefCountedPtr<ASTLiteralInt>();
        value->value = StringUtil::Parse<int>(token.GetValue());

        return value;
    }

    return nullptr;
}

RC<ASTExpr> Parser::ParseLiteralFloat()
{
    if (Token token = Expect(TK_FLOAT, true))
    {
        RC<ASTLiteralFloat> value = MakeRefCountedPtr<ASTLiteralFloat>();
        value->value = StringUtil::Parse<double>(token.GetValue());

        return value;
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
    RC<ASTInitializerExpr> initializerExpr = MakeRefCountedPtr<ASTInitializerExpr>();

    if (Expect(TK_OPEN_BRACE, true))
    {
        if (!Match(TK_CLOSE_BRACE, false))
        {
            while (true)
            {
                initializerExpr->values.PushBack(ParseExpr());

                if (!Match(TK_COMMA, true))
                {
                    break;
                }
            }
        }

        Expect(TK_CLOSE_BRACE, true);
    }

    return initializerExpr;
}

RC<ASTMemberDecl> Parser::ParseMemberDecl()
{
    RC<ASTMemberDecl> memberDecl = MakeRefCountedPtr<ASTMemberDecl>();

    bool isInline = false;
    bool isVirtual = false;
    bool isStatic = false;
    bool isThreadLocal = false;
    bool isConstexpr = false;
    bool isFunction = false;

    bool keepReading = true;
    while (keepReading)
    {
        if (MatchIdentifier("inline", true))
        {
            isInline = true;
            isFunction = true;
        }
        else if (MatchIdentifier("virtual", true))
        {
            isVirtual = true;
            isFunction = true;
        }
        else if (MatchIdentifier("static", true))
        {
            isStatic = true;
        }
        else if (MatchIdentifier("thread_local", true))
        {
            isThreadLocal = true;
        }
        else if (MatchIdentifier("constexpr", true))
        {
            isConstexpr = true;
        }
        else
        {
            keepReading = false;
        }
    }

    memberDecl->type = ParseType();

    if (memberDecl->type->isFunctionPointer)
    {
        // Need to implement function pointer parsing
        HYP_NOT_IMPLEMENTED();
    }

    if (Token nameToken = Expect(TK_IDENT, true))
    {
        memberDecl->name = nameToken.GetValue();
    }

    Token openParenthToken = Token::EMPTY;

    if (isFunction)
    {
        openParenthToken = Expect(TK_OPEN_PARENTH, false);
    }
    else
    {
        openParenthToken = Match(TK_OPEN_PARENTH, false);
    }

    if (openParenthToken)
    {
        memberDecl->type = ParseFunctionType(memberDecl->type);
    }
    else if (Match(TK_OPEN_BRACKET, true))
    {
        RC<ASTType> arrayType = MakeRefCountedPtr<ASTType>();
        arrayType->arrayOf = memberDecl->type;
        arrayType->isArray = true;

        if (!Match(TK_CLOSE_BRACKET, false))
        {
            arrayType->arrayExpr = ParseExpr();
        }

        memberDecl->type = arrayType;

        Expect(TK_CLOSE_BRACKET, true);
    }

    if (!memberDecl->type->isFunction)
    {
        if (MatchOperator("=", true))
        {
            memberDecl->value = ParseExpr();
        }
        else if (Match(TK_OPEN_BRACE, false))
        {
            memberDecl->value = ParseInitializerExpr();
        }
    }

    memberDecl->type->isVirtual = isVirtual;
    memberDecl->type->isInline = isInline;
    memberDecl->type->isStatic = isStatic;
    memberDecl->type->isThreadLocal = isThreadLocal;
    memberDecl->type->isConstexpr = isConstexpr;

    return memberDecl;
}

RC<ASTMemberDecl> Parser::ParseEnumMemberDecl(const RC<ASTType>& underlyingType)
{
    RC<ASTMemberDecl> memberDecl = MakeRefCountedPtr<ASTMemberDecl>();
    memberDecl->type = underlyingType;

    if (!memberDecl->type)
    {
        memberDecl->type = MakeRefCountedPtr<ASTType>();
        memberDecl->type->typeName = QualifiedName { { "int" } };
    }

    if (Token nameToken = Expect(TK_IDENT, true))
    {
        memberDecl->name = nameToken.GetValue();
    }

    if (MatchOperator("=", true))
    {
        memberDecl->value = ParseExpr();
    }

    return memberDecl;
}

RC<ASTType> Parser::ParseType()
{
    // Start by creating our root type instance
    RC<ASTType> rootType = MakeRefCountedPtr<ASTType>();

    bool keepReading = true;
    while (keepReading)
    {
        if (MatchIdentifier("const", true))
        {
            rootType->isConst = true;
        }
        else if (MatchIdentifier("volatile", true))
        {
            rootType->isVolatile = true;
        }
        if (MatchIdentifier("constexpr", true))
        {
            rootType->isConstexpr = true;
        }
        else
        {
            keepReading = false;
        }
    }

    rootType->typeName = ReadQualifiedName();

    if (MatchOperator("<<", false))
    {
        m_tokenStream->Pop();

        for (int i = 0; i < 2; i++)
        {
            m_tokenStream->Push(Token(TK_OPERATOR, "<", m_tokenStream->Peek().GetLocation()), /* insertAtPosition */ true);
        }
    }

    if (MatchOperator("<", true))
    {
        ++m_templateArgumentDepth;

        rootType->isTemplate = true;

        if (MatchOperator(">>", false))
        {
            m_tokenStream->Pop();

            for (int i = 0; i < 2; i++)
            {
                m_tokenStream->Push(Token(TK_OPERATOR, ">", m_tokenStream->Peek().GetLocation()), /* insertAtPosition */ true);
            }
        }

        if (!MatchOperator(">", false))
        {
            while (true)
            {
                RC<ASTTemplateArgument> templateArgument = MakeRefCountedPtr<ASTTemplateArgument>();

                if (Match(TK_INTEGER, false))
                {
                    templateArgument->expr = ParseLiteralInt();
                }
                else if (Match(TK_FLOAT, false))
                {
                    templateArgument->expr = ParseLiteralFloat();
                }
                else if (Match(TK_STRING, false))
                {
                    templateArgument->expr = ParseLiteralString();
                }
                else if (Match(TK_OPEN_PARENTH, false))
                {
                    templateArgument->expr = ParseExpr();
                }
                else
                {
                    templateArgument->type = ParseType();
                }

                rootType->templateArguments.PushBack(templateArgument);

                if (!Match(TK_COMMA, true))
                {
                    break;
                }
            }
        }

        if (MatchOperator(">>", false))
        {
            m_tokenStream->Pop();

            for (int i = 0; i < 2; i++)
            {
                m_tokenStream->Push(Token(TK_OPERATOR, ">", m_tokenStream->Peek().GetLocation()), /* insertAtPosition */ true);
            }
        }

        if (ExpectOperator(">", true))
        {
            --m_templateArgumentDepth;
        }
    }

    while (true)
    {
        // Reference
        if (MatchOperator("&", true))
        {
            RC<ASTType> refType = MakeRefCountedPtr<ASTType>();
            refType->isLvalueReference = true;
            refType->refTo = rootType;
            // Optional cv-qualifiers after '&'
            bool moreCv = true;
            while (moreCv)
            {
                if (MatchIdentifier("const", true))
                {
                    refType->isConst = true;
                }
                else if (MatchIdentifier("volatile", true))
                {
                    refType->isVolatile = true;
                }
                else
                {
                    moreCv = false;
                }
            }
            rootType = refType;
        }
        else if (MatchOperator("&&", true))
        {
            RC<ASTType> refType = MakeRefCountedPtr<ASTType>();
            refType->isRvalueReference = true;
            refType->refTo = rootType;
            // Optional cv-qualifiers after '&&'
            bool moreCv = true;
            while (moreCv)
            {
                if (MatchIdentifier("const", true))
                {
                    refType->isConst = true;
                }
                else if (MatchIdentifier("volatile", true))
                {
                    refType->isVolatile = true;
                }
                else
                {
                    moreCv = false;
                }
            }
            rootType = refType;
        }
        else if (MatchOperator("*", true))
        {
            RC<ASTType> ptrType = MakeRefCountedPtr<ASTType>();
            ptrType->isPointer = true;
            ptrType->ptrTo = rootType;
            // Optional cv-qualifiers after '*'
            bool moreCv = true;
            while (moreCv)
            {
                if (MatchIdentifier("const", true))
                {
                    ptrType->isConst = true;
                }
                else if (MatchIdentifier("volatile", true))
                {
                    ptrType->isVolatile = true;
                }
                else
                {
                    moreCv = false;
                }
            }
            rootType = ptrType;
        }
        else
        {
            break;
        }
    }

    // if (Match(TK_OPEN_PARENTH, false)) {
    //     if (MatchOperatorAhead("*", 1)) {
    //         // Function pointer
    //         RC<ASTType> funcPtrType = MakeRefCountedPtr<ASTType>();
    //         funcPtrType->isFunctionPointer = true;
    //         funcPtrType->ptrTo = rootType;
    //         rootType = funcPtrType;

    //         // Need to implement function pointer parsing
    //         HYP_NOT_IMPLEMENTED();
    //     }
    // }

    return rootType;
}

RC<ASTFunctionType> Parser::ParseFunctionType(const RC<ASTType>& returnType)
{
    Assert(returnType != nullptr);

    RC<ASTFunctionType> funcType = MakeRefCountedPtr<ASTFunctionType>();
    funcType->returnType = returnType;

    if (Expect(TK_OPEN_PARENTH, true))
    {
        if (!Match(TK_CLOSE_PARENTH, false))
        {
            while (true)
            {
                funcType->parameters.PushBack(ParseMemberDecl());

                if (!Match(TK_COMMA, true))
                {
                    break;
                }
            }
        }

        Expect(TK_CLOSE_PARENTH, true);

        bool keepReading = true;

        while (keepReading)
        {
            if (MatchIdentifier("const", true))
            {
                funcType->isConstMethod = true;
            }
            else if (MatchIdentifier("override", true))
            {
                funcType->isOverrideMethod = true;
            }
            else if (MatchIdentifier("noexcept", true))
            {
                funcType->isNoexceptMethod = true;
            }
            else if (MatchOperator("&&", true))
            {
                funcType->isRvalueMethod = true;
            }
            else if (MatchOperator("&", true))
            {
                funcType->isLvalueMethod = true;
            }
            else
            {
                keepReading = false;
            }
        }

        // Read = default, = delete or = 0
        if (MatchOperator("=", true))
        {
            if (MatchIdentifier("default", true))
            {
                funcType->isDefaultedMethod = true;
            }
            else if (MatchIdentifier("delete", true))
            {
                funcType->isDeletedMethod = true;
            }
            else if (Token integerToken = Match(TK_INTEGER, true))
            {
                if (integerToken.GetValue() == "0")
                {
                    funcType->isPureVirtualMethod = true;
                }
                else
                {
                    m_compilationUnit->GetErrorList().AddError(CompilerError(
                        LEVEL_ERROR,
                        Msg_unexpected_character,
                        integerToken.GetLocation(),
                        integerToken.GetValue()));
                }
            }
        }
        else if (Match(TK_OPEN_BRACE, false))
        {
            // @TODO Implement function body parsing
        }
    }

    return funcType;
}

} // namespace hyperion::buildtool