/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/DataProcessing/HMF/HMF.hpp>
#include <Core/DataProcessing/HMF/Parser/Parser.hpp>
#include <Core/DataProcessing/Shared/CompilerError.hpp>
#include <Core/DataProcessing/Shared/Token.hpp>

#include <Core/Logging/Logger.hpp>

#include <Core/Reflection/BoxedValue.hpp>
#include <Core/Reflection/TypeInfo.hpp>
#include <Core/Reflection/Class.hpp>
#include <Core/Reflection/Enum.hpp>
#include <Core/Reflection/Field.hpp>
#include <Core/Reflection/Property.hpp>
#include <Core/Reflection/StaticField.hpp>
#include <Core/Reflection/Member.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Utilities/StringUtil.hpp>
#include <Core/Utilities/Uuid.hpp>

#include <cstdlib>

namespace Hyperion::DataProcessing::HMF {

Parser::Parser(
    TokenStream* tokenStream,
    DataProcessing::ErrorList<CompilerError>* errorList,
    BoxedValue* target)
    : m_tokenStream(tokenStream),
      m_errorList(errorList),
      m_target(target),
      m_ownsTarget(target == nullptr)
{
}

Parser::~Parser()
{
    if (m_ownsTarget)
    {
        delete m_target;
    }
}

bool Parser::Parse(BoxedValue& out, bool moveResult)
{
    if (!Parse())
    {
        return false;
    }

    Assert(m_target != nullptr);

    if (moveResult)
    {
        AssertDebug(m_ownsTarget);

        out = std::move(*m_target);
    }
    else
    {
        out = *m_target;
    }

    return true;
}

bool Parser::Parse()
{
    String className;

    if (!ExpectIdentifier(className))
    {
        return false;
    }

    const Class* cls = Hyperion::GetClass(StringHash(className));

    if (!cls)
    {
        Error(MSG_CLASS_NOT_FOUND, Peek().GetLocation(), className);
        return false;
    }

    if (cls->IsAbstract() || !cls->CanCreateInstance())
    {
        Error(MSG_CLASS_NOT_FOUND, Peek().GetLocation(), className);
        return false;
    }

    String objectName;

    if (Peek().GetTokenClass() == TK_STRING || Peek().GetTokenClass() == TK_IDENT)
    {
        objectName = Next().GetValue();
    }

    if (!Expect(TK_OPEN_BRACE, "{"))
    {
        return false;
    }

    if (m_target != nullptr)
    {
        const Class* targetClass = m_target->GetTypeInfo()->GetClass();
        if (!targetClass)
        {
            Error(MSG_TYPE_MISMATCH, Peek().GetLocation());
            return false;
        }

        if (!targetClass->IsDerivedFrom(cls))
        {
            Error(MSG_TYPE_MISMATCH, Peek().GetLocation());
            return false;
        }
    }
    else
    {
        Assert(m_ownsTarget);
        m_target = new BoxedValue();
        
        if (!cls->CreateInstance(*m_target))
        {
            Error(MSG_INTERNAL_ERROR, Peek().GetLocation());
            return false;
        }
    }

    if (!ParseObjectBody(cls, *m_target, objectName))
    {
        return false;
    }

    if (!Expect(TK_CLOSE_BRACE, "}"))
    {
        return false;
    }

    return true;
}

bool Parser::ParseObjectBody(const Class* cls, BoxedValue& target, const UTF8StringView& objectName)
{
    const SourceLocation currLocation = Peek().GetLocation();

    while (Peek().GetTokenClass() != TK_CLOSE_BRACE && Peek().GetTokenClass() != TK_EMPTY)
    {
        if (Peek().GetTokenClass() == TK_COMMA)
        {
            const Token comma = Next();

            if (Peek().GetTokenClass() == TK_CLOSE_BRACE)
            {
                Error(MSG_UNEXPECTED_TOKEN, comma.GetLocation(), ",");
                return false;
            }
        }

        // Field name
        String fieldName;
        if (!ExpectIdentifier(fieldName))
        {
            return false;
        }

        // Expect '=' separator
        if (!Expect(TK_EQUALS, "="))
        {
            return false;
        }

        // Look up the member on the class
        const IMember* member = cls->GetMember(StringHash(fieldName), MemberType::Field | MemberType::Property);

        if (!member)
        {
            // Unknown field: warn and skip its value
            Warning(MSG_UNKNOWN_FIELD, Peek().GetLocation(), cls->GetName().ToString() + "::" + fieldName);
            
            SkipValue();

            continue;
        }

        // If has `Property = ...`, we want to grab the synthetic Property created instead.
        if (member->GetMemberType() == MemberType::Field && member->GetAttribute(Attributes::g_attrProperty).IsValid())
        {
            if (Property* prop = cls->GetProperty(StringHash(fieldName)))
            {
                member = prop;
            }
        }

        if (member->GetAttribute(Attributes::g_attrJsonIgnore).IsValid())
        {
            SkipValue();

            continue;
        }
        
        const TypeInfo& fieldTypeInfo = member->GetTypeInfo();
        
        BoxedValue fieldValue;
        if (!ParseValue(fieldTypeInfo, fieldValue))
        {
            return false;
        }

        if (member->GetMemberType() == MemberType::Property)
        {
            if (!static_cast<const Property*>(member)->CanSet())
            {
                // Dispatch warning and continue on
                Warning(MSG_CANNOT_ASSIGN_PROPERTY, Peek().GetLocation(), cls->GetName().ToString() + "::" + member->GetName().ToString());
                
                continue;
            }
        }

        switch (member->GetMemberType())
        {
        case MemberType::Field:
            static_cast<const Field*>(member)->Set(target, fieldValue);
            break;
        case MemberType::Property:
            static_cast<const Property*>(member)->Set(target, fieldValue);
            break;
        default:
            break;
        }
    }

    // Set `Name` if present
    if (objectName)
    {
        const IMember* member = cls->GetMember("Name"_sh, MemberType::Field | MemberType::Property);

        if (member != nullptr)
        {
        switch (member->GetMemberType())
        {
        case MemberType::Field:
            static_cast<const Field*>(member)->Set(target, BoxedValue(CreateNameFromDynamicString(objectName)));
            break;
        case MemberType::Property:
            if (!static_cast<const Property*>(member)->CanSet())
            {
                Warning(ErrorMessage::MSG_CANNOT_ASSIGN_PROPERTY, currLocation, "Name");
                break;
            }

            static_cast<const Property*>(member)->Set(target, BoxedValue(CreateNameFromDynamicString(objectName)));
            break;
        default:
            break;
        }
        }
    }

    return true;
}

bool Parser::ParseValue(const TypeInfo& typeInfo, BoxedValue& out)
{
    const Token next = Peek();

    // '@' before string denotes AssetPath literal
    if (next.GetTokenClass() == TK_AT_STRING)
    {
        return ParseAssetPathLiteral(typeInfo, out);
    }

    if (typeInfo.IsBoolType())
    {
        return ParseBoolValue(out);
    }

    if (typeInfo.IsIntegralType())
    {
        return ParseIntegralValue(typeInfo, out);
    }

    if (typeInfo.IsFloatType())
    {
        return ParseFloatValue(typeInfo, out);
    }

    if (typeInfo.IsStringType())
    {
        return ParseStringValue(typeInfo, out);
    }

    // Name / StringHash / UUID handling.
    {
        if (typeInfo.id == TypeId::ForType<Name>()
            || typeInfo.id == TypeId::ForType<StringHash>())
        {
            // try reading IDENT first
            if (Peek().GetTokenClass() == TK_IDENT)
            {
                Token ident = Next();
                out = BoxedValue(CreateNameFromDynamicString(ident.GetValue()));
                return true;
            }

            // Try reading string
            return ParseStringValue(typeInfo, out);
        }

        if (typeInfo.id == TypeId::ForType<UUID>())
        {
            return ParseStringValue(typeInfo, out);
        }
    }

    if (typeInfo.IsEnumFlags())
    {
        return ParseEnumFlagsValue(typeInfo, out);
    }

    if (typeInfo.IsEnum())
    {
        return ParseEnumValue(typeInfo, out);
    }

    if (typeInfo.IsVectorType())
    {
        return ParseVectorValue(typeInfo, out);
    }

    if (typeInfo.IsArrayType())
    {
        return ParseArrayValue(typeInfo, out);
    }

    if (typeInfo.IsPairType())
    {
        return ParsePairValue(typeInfo, out);
    }

    if (typeInfo.IsTupleType())
    {
        return ParseTupleValue(typeInfo, out);
    }

    if (typeInfo.IsMatrixType())
    {
        return ParseMatrixValue(typeInfo, out);
    }

    if (typeInfo.IsVariantType())
    {
        return ParseVariantValue(typeInfo, out);
    }

    if (typeInfo.IsClass() || typeInfo.IsStruct())
    {
        return ParseObjectValue(typeInfo, out);
    }

    Error(MSG_NOT_IMPLEMENTED, next.GetLocation(), String("Parsing of type ") + typeInfo.name.ToString());

    return false;
}

bool Parser::ParseBoolValue(BoxedValue& out)
{
    Token token = Peek();

    if (token.GetTokenClass() != TK_IDENT)
    {
        Error(MSG_UNEXPECTED_TOKEN, token.GetLocation(), Token::TokenTypeToString(token.GetTokenClass()));

        return false;
    }

    Next();

    if (token.GetValue() == "true")
    {
        out = BoxedValue(true);
        return true;
    }

    if (token.GetValue() == "false")
    {
        out = BoxedValue(false);
        return true;
    }

    Error(MSG_UNEXPECTED_IDENTIFIER, token.GetLocation(), token.GetValue());

    return false;
}

bool Parser::ParseIntegralValue(const TypeInfo& typeInfo, BoxedValue& out)
{
    Token token = Peek();

    if (token.GetTokenClass() != TK_INTEGER)
    {
        Error(MSG_UNEXPECTED_TOKEN, token.GetLocation(), Token::TokenTypeToString(token.GetTokenClass()));

        return false;
    }

    Next();

    const String& text = token.GetValue();

    const bool isHex = text.Size() >= 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X');
    const int base = isHex ? 16 : 10;

    const TypeId& id = typeInfo.id;

    if (id == TypeId::ForType<int8>())
    {
        out = BoxedValue(int8(std::strtoll(text.Data(), nullptr, base)));
    }
    else if (id == TypeId::ForType<int16>())
    {
        out = BoxedValue(int16(std::strtoll(text.Data(), nullptr, base)));
    }
    else if (id == TypeId::ForType<int32>())
    {
        out = BoxedValue(int32(std::strtoll(text.Data(), nullptr, base)));
    }
    else if (id == TypeId::ForType<int64>())
    {
        out = BoxedValue(int64(std::strtoll(text.Data(), nullptr, base)));
    }
    else if (id == TypeId::ForType<uint8>())
    {
        out = BoxedValue(uint8(std::strtoull(text.Data(), nullptr, base)));
    }
    else if (id == TypeId::ForType<uint16>())
    {
        out = BoxedValue(uint16(std::strtoull(text.Data(), nullptr, base)));
    }
    else if (id == TypeId::ForType<uint32>())
    {
        out = BoxedValue(uint32(std::strtoull(text.Data(), nullptr, base)));
    }
    else if (id == TypeId::ForType<uint64>())
    {
        out = BoxedValue(uint64(std::strtoull(text.Data(), nullptr, base)));
    }
    else if (id == TypeId::ForType<char>())
    {
        out = BoxedValue(static_cast<char>(std::strtoll(text.Data(), nullptr, base)));
    }
    else
    {
        Error(MSG_INVALID_LITERAL_FOR_TYPE, token.GetLocation(), text, typeInfo.name.LookupString());

        return false;
    }

    return true;
}

bool Parser::ParseFloatValue(const TypeInfo& typeInfo, BoxedValue& out)
{
    Token token = Peek();

    if (token.GetTokenClass() != TK_FLOAT && token.GetTokenClass() != TK_INTEGER)
    {
        Error(MSG_UNEXPECTED_TOKEN, token.GetLocation(), Token::TokenTypeToString(token.GetTokenClass()));

        return false;
    }

    Next();

    const String& text = token.GetValue();
    const double parsed = std::strtod(text.Data(), nullptr);

    if (typeInfo.id == TypeId::ForType<float>())
    {
        out = BoxedValue(static_cast<float>(parsed));
    }
    else if (typeInfo.id == TypeId::ForType<double>())
    {
        out = BoxedValue(parsed);
    }
    else
    {
        Error(MSG_INVALID_LITERAL_FOR_TYPE, token.GetLocation(), text, typeInfo.name.LookupString());

        return false;
    }

    return true;
}

bool Parser::ParseStringValue(const TypeInfo& typeInfo, BoxedValue& out)
{
    Token token = Peek();

    if (token.GetTokenClass() != TK_STRING && token.GetTokenClass() != TK_AT_STRING)
    {
        Error(MSG_UNEXPECTED_TOKEN, token.GetLocation(), Token::TokenTypeToString(token.GetTokenClass()));

        return false;
    }

    Next();

    const String& text = token.GetValue();

    // Name / StringHash are stored as Name internally
    if (typeInfo.id == TypeId::ForType<Name>()
        || typeInfo.id == TypeId::ForType<StringHash>())
    {
        out = BoxedValue(CreateNameFromDynamicString(text.Data()));
    }
    else if (typeInfo.id == TypeId::ForType<UUID>())
    {
        out = BoxedValue(UUID(text.Data()));
    }
    else
    {
        out = BoxedValue(text);
    }

    return true;
}

bool Parser::ResolveEnumName(const Class* enumClass, const String& name, BoxedValue& outValue)
{
    if (!enumClass || !enumClass->IsEnumType())
    {
        return false;
    }

    if (StaticField* staticField = enumClass->GetStaticField(StringHash(name)))
    {
        outValue = staticField->Get();

        // NOT strict, we want to check if it is 'a' numeric type.
        if (outValue.Is<uint64>(/* strict */ false))
        {
            return true;
        }
    }

    return false;
}

bool Parser::ParseEnumValue(const TypeInfo& typeInfo, BoxedValue& out)
{
    Token token = Peek();

    // Identifier - enum literal
    if (token.GetTokenClass() == TK_IDENT)
    {
        Next();

        const Class* enumClass = typeInfo.GetClass();

        if (!ResolveEnumName(enumClass, token.GetValue(), out))
        {
            Error(MSG_UNRESOLVED_ENUM_NAME, token.GetLocation(),
                    enumClass ? enumClass->GetName().ToString() + "::" + token.GetValue() : token.GetValue());

            return false;
        }

        return true;
    }

    // Integral value representing the enum value
    // We need this as we sometimes use enum classes as strongly typed integer IDs
    // (For ex. see LightmapElementId)
    if (token.GetTokenClass() == TK_INTEGER)
    {
        Next();
        
        uint64 uValue;

        if (StringUtil::Parse(token.GetValue(), &uValue))
        {
            out = BoxedValue(uValue);

            return true;
        }
    }
    
    Error(MSG_UNEXPECTED_TOKEN, token.GetLocation(), Token::TokenTypeToString(token.GetTokenClass()));

    return false;
}

bool Parser::ParseEnumFlagsValue(const TypeInfo& typeInfo, BoxedValue& out)
{
    const TypeInfo* enumType = typeInfo.GetEnumType();

    if (!enumType)
    {
        Error(MSG_NOT_AN_ENUM_FLAGS_TYPE, Peek().GetLocation(), typeInfo.name.LookupString());

        return false;
    }

    const Class* enumClass = enumType->GetClass();

    Token firstToken = Peek();

    if (firstToken.GetTokenClass() != TK_IDENT)
    {
        Error(MSG_UNEXPECTED_TOKEN, firstToken.GetLocation(), Token::TokenTypeToString(firstToken.GetTokenClass()));

        return false;
    }

    Next();

    uint64 combined = 0;
    bool anyResolved = false;

    auto resolveOne = [&](const Token& nameToken)
    {
        BoxedValue boxed;
        if (ResolveEnumName(enumClass, nameToken.GetValue(), boxed))
        {
            Assert(boxed.Is<uint64>());
            combined |= boxed.Get<uint64>();
            anyResolved = true;
        }
        else
        {
            // Check for numeric
            const String& s = nameToken.GetValue();

            if (!s.Empty() && std::isdigit(s.GetChar(0)))
            {
                uint64 uValue;
                if (StringUtil::Parse(s, &uValue))
                {
                    combined |= uValue;
                    anyResolved = true;
                }
                else
                {
                    Error(MSG_UNEXPECTED_TOKEN, nameToken.GetLocation());
                }
            }
            else
            {
                Warning(MSG_UNRESOLVED_ENUM_NAME, nameToken.GetLocation(),
                        enumClass ? enumClass->GetName().ToString() + "::" + s : s);
            }
        }
    };

    resolveOne(firstToken);

    // Consume any "| Name" pairs
    while (Peek().GetTokenClass() == TK_PIPE)
    {
        Next(); // consume '|'

        const Token nameToken = Peek();

        if (nameToken.GetTokenClass() != TK_IDENT)
        {
            Error(MSG_EXPECTED_IDENTIFIER, nameToken.GetLocation());

            return false;
        }

        Next();

        resolveOne(nameToken);
    }

    if (!anyResolved)
    {
        // No names resolved; leave at 0 (warning already emitted)
    }

    // Store as the enum's underlying integer type
    const TypeInfo* underlyingTypeInfo = typeInfo.GetUnderlyingType();
    const TypeId underlyingTypeId = underlyingTypeInfo != nullptr ? underlyingTypeInfo->id : TypeId::ForType<uint32>();

    // Select the proper type based on underlying type id
    if (underlyingTypeId == TypeId::ForType<int8>()) { out = BoxedValue(int8(combined)); }
    else if (underlyingTypeId == TypeId::ForType<int16>()) { out = BoxedValue(int16(combined)); }
    else if (underlyingTypeId == TypeId::ForType<int32>()) { out = BoxedValue(int32(combined)); }
    else if (underlyingTypeId == TypeId::ForType<int64>()) { out = BoxedValue(int64(combined)); }
    else if (underlyingTypeId == TypeId::ForType<uint8>()) { out = BoxedValue(uint8(combined)); }
    else if (underlyingTypeId == TypeId::ForType<uint16>()) { out = BoxedValue(uint16(combined)); }
    else if (underlyingTypeId == TypeId::ForType<uint32>()) { out = BoxedValue(uint32(combined)); }
    else if (underlyingTypeId == TypeId::ForType<uint64>()) { out = BoxedValue(uint64(combined)); }
    else { out = BoxedValue(uint32(combined)); }

    return true;
}

bool Parser::ParseVectorValue(const TypeInfo& typeInfo, BoxedValue& out)
{
    auto* handler = static_cast<ITypeInfoVectorHandler*>(typeInfo.extendedInfo.handler);
    if (!handler)
    {
        Error(MSG_INTERNAL_ERROR, Peek().GetLocation());

        return false;
    }

    if (!handler->CreateInstance(out))
    {
        Error(MSG_INTERNAL_ERROR, Peek().GetLocation());

        return false;
    }

    if (!Expect(TK_OPEN_PARENTH, "("))
    {
        return false;
    }

    const int numComponents = handler->GetNumComponents();
    const TypeInfo* elementType = typeInfo.GetElementType();

    for (int i = 0; i < numComponents; i++)
    {
        if (Peek().GetTokenClass() == TK_COMMA)
        {
            Token comma = Next();
            if (Peek().GetTokenClass() == TK_CLOSE_PARENTH)
            {
                Error(MSG_UNEXPECTED_TOKEN, comma.GetLocation(), ",");

                return false;
            }
        }

        BoxedValue component;
        if (!ParseValue(*elementType, component))
        {
            return false;
        }

        handler->SetComponent(out, i, component);
    }

    if (!Expect(TK_CLOSE_PARENTH, ")"))
    {
        return false;
    }

    return true;
}

bool Parser::ParseArrayValue(const TypeInfo& typeInfo, BoxedValue& out)
{
    if (!Expect(TK_OPEN_BRACKET, "["))
    {
        return false;
    }

    const TypeInfo* elementType = typeInfo.GetElementType();

    if (!elementType)
    {
        Error(MSG_INTERNAL_ERROR, Peek().GetLocation());

        return false;
    }

    Array<BoxedValue> elements;

    while (Peek().GetTokenClass() != TK_CLOSE_BRACKET && Peek().GetTokenClass() != TK_EMPTY)
    {
        if (Peek().GetTokenClass() == TK_COMMA)
        {
            Token comma = Next();

            if (Peek().GetTokenClass() == TK_CLOSE_BRACKET)
            {
                Error(MSG_UNEXPECTED_TOKEN, comma.GetLocation(), ",");

                return false;
            }
        }

        BoxedValue element;

        if (!ParseValue(*elementType, element))
        {
            return false;
        }

        elements.PushBack(std::move(element));
    }

    if (!Expect(TK_CLOSE_BRACKET, "]"))
    {
        return false;
    }

    // Now build the array via the handler
    auto* handler = static_cast<ITypeInfoArrayHandler*>(typeInfo.extendedInfo.handler);
    if (!handler)
    {
        Error(MSG_INTERNAL_ERROR, Peek().GetLocation());

        return false;
    }

    if (!handler->CreateInstance(out))
    {
        Error(MSG_INTERNAL_ERROR, Peek().GetLocation());

        return false;
    }

    handler->Resize(out, elements.Size());

    for (size_t i = 0; i < elements.Size(); i++)
    {
        handler->SetElementAt(out, i, std::move(elements[i]));
    }

    return true;
}

bool Parser::ParseSetValue(const TypeInfo& typeInfo, BoxedValue& out)
{
    if (!Expect(TK_OPEN_BRACKET, "["))
    {
        return false;
    }

    const TypeInfo* elementType = typeInfo.GetElementType();

    if (!elementType)
    {
        Error(MSG_INTERNAL_ERROR, Peek().GetLocation());

        return false;
    }

    auto* handler = static_cast<ITypeInfoSetHandler*>(typeInfo.extendedInfo.handler);

    if (!handler)
    {
        Error(MSG_INTERNAL_ERROR, Peek().GetLocation());

        return false;
    }

    if (!handler->CreateInstance(out))
    {
        Error(MSG_INTERNAL_ERROR, Peek().GetLocation());

        return false;
    }

    while (Peek().GetTokenClass() != TK_CLOSE_BRACKET && Peek().GetTokenClass() != TK_EMPTY)
    {
        if (Peek().GetTokenClass() == TK_COMMA)
        {
            Token comma = Next();
            
            if (Peek().GetTokenClass() == TK_CLOSE_BRACKET)
            {
                Error(MSG_UNEXPECTED_TOKEN, comma.GetLocation(), ",");

                return false;
            }
        }

        BoxedValue element;

        if (!ParseValue(*elementType, element))
        {
            return false;
        }

        handler->Insert(out, element);
    }

    if (!Expect(TK_CLOSE_BRACKET, "]"))
    {
        return false;
    }

    return true;
}

/// Map format: { "Key" = Value, "Key2" = Value2 }
bool Parser::ParseMapValue(const TypeInfo& typeInfo, BoxedValue& out)
{
    if (!Expect(TK_OPEN_BRACE, "{"))
    {
        return false;
    }

    const TypeInfo* keyType = typeInfo.GetKeyType();
    const TypeInfo* valueType = typeInfo.GetValueType();

    if (!keyType || !valueType)
    {
        Error(MSG_INTERNAL_ERROR, Peek().GetLocation());

        return false;
    }

    auto* handler = static_cast<ITypeInfoMapHandler*>(typeInfo.extendedInfo.handler);

    if (!handler)
    {
        Error(MSG_INTERNAL_ERROR, Peek().GetLocation());

        return false;
    }

    if (!handler->CreateInstance(out))
    {
        Error(MSG_INTERNAL_ERROR, Peek().GetLocation());

        return false;
    }

    while (Peek().GetTokenClass() != TK_CLOSE_BRACE && Peek().GetTokenClass() != TK_EMPTY)
    {
        if (Peek().GetTokenClass() == TK_COMMA)
        {
            Token comma = Next();

            if (Peek().GetTokenClass() == TK_CLOSE_BRACE)
            {
                Error(MSG_UNEXPECTED_TOKEN, comma.GetLocation(), ",");

                return false;
            }
        }

        // Parse key (type-directed)
        BoxedValue key;

        if (!ParseValue(*keyType, key))
        {
            return false;
        }

        if (!Expect(TK_EQUALS, "="))
        {
            return false;
        }

        // Parse value
        BoxedValue value;

        if (!ParseValue(*valueType, value))
        {
            return false;
        }

        handler->SetValueAt(out, key, value);
    }

    if (!Expect(TK_CLOSE_BRACE, "}"))
    {
        return false;
    }

    return true;
}

bool Parser::ParsePairValue(const TypeInfo& typeInfo, BoxedValue& out)
{
    if (!Expect(TK_OPEN_PARENTH, "("))
    {
        return false;
    }

    auto* handler = static_cast<ITypeInfoPairHandler*>(typeInfo.extendedInfo.handler);

    if (!handler)
    {
        Error(MSG_INTERNAL_ERROR, Peek().GetLocation());
        return false;
    }

    if (!handler->CreateInstance(out))
    {
        Error(MSG_INTERNAL_ERROR, Peek().GetLocation());
        return false;
    }

    const TypeInfo* firstType = handler->GetFirstTypeInfo();
    const TypeInfo* secondType = handler->GetSecondTypeInfo();

    BoxedValue first;
    if (!ParseValue(*firstType, first))
    {
        return false;
    }

    handler->SetFirst(out, first);

    // Optional comma between pair elements
    if (Peek().GetTokenClass() == TK_COMMA)
    {
        Next();
    }

    BoxedValue second;
    if (!ParseValue(*secondType, second))
    {
        return false;
    }

    handler->SetSecond(out, second);

    if (!Expect(TK_CLOSE_PARENTH, ")"))
    {
        return false;
    }

    return true;
}

bool Parser::ParseObjectValue(const TypeInfo& typeInfo, BoxedValue& out)
{
    // Handle null object:
    if (Peek().GetTokenClass() == TK_IDENT && Peek().GetValue() == "null")
    {
        // set to null, BoxedValue allows implicit conversion of Handle<ObjectBase> to Handle<T> if it is null.
        out = BoxedValue(Handle<ObjectBase>::Null());

        Next();

        return true;
    }

    const Class* declaredClass = typeInfo.GetClass();
    const Class* actualClass = declaredClass;

    if (Peek().GetTokenClass() == TK_IDENT
        && (Peek(1).GetTokenClass() == TK_OPEN_BRACE || ((Peek(1).GetTokenClass() == TK_STRING || Peek(1).GetTokenClass() == TK_IDENT) && Peek(2).GetTokenClass() == TK_OPEN_BRACE)))
    {
        Token classToken = Next(); // consume IDENT
        const String& runtimeClassName = classToken.GetValue();

        actualClass = Hyperion::GetClass(StringHash(runtimeClassName));

        if (!actualClass)
        {
            Error(MSG_CLASS_NOT_FOUND, classToken.GetLocation(), runtimeClassName);

            return false;
        }

        if (declaredClass && !actualClass->IsDerivedFrom(declaredClass))
        {
            Error(MSG_CLASS_NOT_DERIVED, classToken.GetLocation(), runtimeClassName, declaredClass->GetName().LookupString());

            return false;
        }
    }

    if (!actualClass)
    {
        Error(MSG_INTERNAL_ERROR, Peek().GetLocation());

        return false;
    }

    if (!actualClass->CanCreateInstance())
    {
        // @TODO Better error.
        Error(MSG_CLASS_NOT_FOUND, Peek().GetLocation(), actualClass->GetName().LookupString());

        return false;
    }

    String objectName;

    if (Peek().GetTokenClass() == TK_STRING || Peek().GetTokenClass() == TK_IDENT)
    {
        // Read objects' name if there is a string/ident following after the class name.
        objectName = Next().GetValue();
    }

    if (!Expect(TK_OPEN_BRACE, "{"))
    {
        return false;
    }

    if (!actualClass->CreateInstance(out))
    {
        Error(MSG_INTERNAL_ERROR, Peek().GetLocation());

        return false;
    }

    if (!ParseObjectBody(actualClass, out, objectName))
    {
        return false;
    }

    if (!Expect(TK_CLOSE_BRACE, "}"))
    {
        return false;
    }

    return true;
}

bool Parser::ParseTupleValue(const TypeInfo& typeInfo, BoxedValue& out)
{
    auto* handler = static_cast<ITypeInfoTupleHandler*>(typeInfo.extendedInfo.handler);

    if (!handler)
    {
        Error(MSG_INTERNAL_ERROR, Peek().GetLocation());

        return false;
    }

    if (!handler->CreateInstance(out))
    {
        Error(MSG_INTERNAL_ERROR, Peek().GetLocation());

        return false;
    }

    if (!Expect(TK_OPEN_PARENTH, "("))
    {
        return false;
    }

    const int numElements = handler->GetNumElements();

    for (int i = 0; i < numElements; i++)
    {
        if (Peek().GetTokenClass() == TK_COMMA)
        {
            Token comma = Next();

            if (Peek().GetTokenClass() == TK_CLOSE_PARENTH)
            {
                Error(MSG_UNEXPECTED_TOKEN, comma.GetLocation(), ",");

                return false;
            }
        }

        BoxedValue element;

        if (!ParseValue(*handler->GetElementTypeInfoAtIndex(i), element))
        {
            return false;
        }

        handler->SetElement(out, i, element);
    }

    if (!Expect(TK_CLOSE_PARENTH, ")"))
    {
        return false;
    }

    return true;
}

bool Parser::ParseMatrixValue(const TypeInfo& typeInfo, BoxedValue& out)
{
    auto* handler = static_cast<ITypeInfoMatrixHandler*>(typeInfo.extendedInfo.handler);

    if (!handler || !handler->CreateInstance(out))
    {
        Error(MSG_INTERNAL_ERROR, Peek().GetLocation());

        return false;
    }

    if (!Expect(TK_OPEN_BRACKET, "["))
    {
        return false;
    }

    const int numRows = handler->GetNumRows();
    const int numCols = handler->GetNumColumns();
    
    const TypeInfo* elementType = typeInfo.GetElementType();

    for (int row = 0; row < numRows; row++)
    {
        if (row > 0)
        {
            if (Peek().GetTokenClass() == TK_COMMA)
            {
                Next();
            }
        }

        // Each row is a sub-array: [v, v, v]
        if (!Expect(TK_OPEN_BRACKET, "["))
        {
            return false;
        }

        for (int col = 0; col < numCols; col++)
        {
            if (col > 0)
            {
                if (Peek().GetTokenClass() == TK_COMMA)
                {
                    Next();
                }
            }

            BoxedValue element;

            if (!ParseValue(*elementType, element))
            {
                return false;
            }

            if (!element.Is<float>())
            {
                Error(MSG_TYPE_MISMATCH, Peek().GetLocation());

                return false;
            }

            handler->SetElement(out, row, col, element.Get<float>());
        }

        if (!Expect(TK_CLOSE_BRACKET, "]"))
        {
            return false;
        }
    }

    if (!Expect(TK_CLOSE_BRACKET, "]"))
    {
        return false;
    }

    return true;
}

bool Parser::ParseVariantValue(const TypeInfo& typeInfo, BoxedValue& out)
{
    auto* handler = static_cast<ITypeInfoVariantHandler*>(typeInfo.extendedInfo.handler);

    if (!handler)
    {
        Error(MSG_INTERNAL_ERROR, Peek().GetLocation());

        return false;
    }

    // null denotes uninitialized variant
    if (Peek().GetTokenClass() == TK_IDENT && Peek().GetValue() == "null")
    {
        Next();

        if (!handler->CreateInstance(out))
        {
            Error(MSG_INTERNAL_ERROR, Peek().GetLocation());

            return false;
        }

        return true;
    }

    BoxedValue variantInstance;

    if (!handler->CreateInstance(variantInstance))
    {
        Error(MSG_INTERNAL_ERROR, Peek().GetLocation());

        return false;
    }

    const int numTypes = handler->GetNumTypes();

    auto trySet = [&]() -> bool
    {
        for (int i = 0; i < numTypes; i++)
        {
            const TypeInfo* alternativeTypeInfo = handler->GetTypeInfoAtIndex(i);

            if (!alternativeTypeInfo)
            {
                continue;
            }

            const size_t savedPos = m_tokenStream->GetPosition();

            BoxedValue parsedValue;

            m_errorList->SuppressErrors(true);
            const bool parsed = ParseValue(*alternativeTypeInfo, parsedValue);
            m_errorList->SuppressErrors(false);

            if (parsed)
            {
                if (handler->SetValue(variantInstance, parsedValue))
                {
                    out = variantInstance;
                    
                    return true;
                }
            }
            m_tokenStream->SetPosition(savedPos);
        }

        return false;
    };

    if (trySet())
    {
        return true;
    }

    Error(MSG_UNKNOWN_VARIANT_TAG, Peek().GetLocation(), String("could not match any alternative type"));

    return false;
}

bool Parser::ParseAssetPathLiteral(const TypeInfo& typeInfo, BoxedValue& out)
{
    Token strToken = Peek();

    if (strToken.GetTokenClass() != TK_AT_STRING)
    {
        Error(MSG_UNEXPECTED_TOKEN, strToken.GetLocation(), Token::TokenTypeToString(strToken.GetTokenClass()));

        return false;
    }

    Next();

    // @TODO : Review the below handling.

    // Give the Engine-side resolver a chance to produce a correctly-typed BoxedValue
    // (e.g. Handle<T>, AssetPath, AssetReference) instead of a raw String.
    if (g_resolveAssetPath && g_resolveAssetPath(strToken.GetValue(), typeInfo, out))
    {
        return true;
    }

    // Fallback: store as String; the Engine-side Property::Set handles conversion to
    // AssetPath/AssetReference via their "Value"/"AssetPath" properties.
    out = BoxedValue(strToken.GetValue());

    return true;
}

void Parser::SkipValue()
{
    bool consumedAny = false;
    int depth = 0;

    while (true)
    {
        Token token = Peek();

        if (token.GetTokenClass() == TK_EMPTY)
        {
            return;
        }

        if (consumedAny && depth == 0)
        {
            const TokenClass tc = token.GetTokenClass();

            // End of containing object or array
            if (tc == TK_CLOSE_BRACE || tc == TK_CLOSE_BRACKET)
            {
                return;
            }

            // Start of the next field in the containing object: IDENT followed by '='
            if (tc == TK_IDENT && Peek(1).GetTokenClass() == TK_EQUALS)
            {
                return;
            }

            // Optional comma between fields/elements belongs to the container, not the value
            if (tc == TK_COMMA)
            {
                return;
            }
        }

        // Track nesting
        const TokenClass tokenClass = token.GetTokenClass();

        if (tokenClass == TK_OPEN_BRACE || tokenClass == TK_OPEN_BRACKET)
        {
            depth++;
        }
        else if (tokenClass == TK_CLOSE_BRACE || tokenClass == TK_CLOSE_BRACKET)
        {
            if (depth == 0)
            {
                // Unmatched closer -- not part of our value
                return;
            }

            depth--;
        }

        Next();

        consumedAny = true;
    }
}

void Parser::SkipBracedBlock()
{
    if (!Match(TK_OPEN_BRACE))
    {
        return;
    }

    int depth = 1;

    while (depth > 0 && Peek().GetTokenClass() != TK_EMPTY)
    {
        Token token = Next();

        if (token.GetTokenClass() == TK_OPEN_BRACE)
        {
            depth++;
        }
        else if (token.GetTokenClass() == TK_CLOSE_BRACE)
        {
            depth--;
        }
    }
}

void Parser::SkipBracketedBlock()
{
    if (!Match(TK_OPEN_BRACKET))
    {
        return;
    }

    int depth = 1;

    while (depth > 0 && Peek().GetTokenClass() != TK_EMPTY)
    {
        Token token = Next();

        if (token.GetTokenClass() == TK_OPEN_BRACE)
        {
            depth++;
        }
        else if (token.GetTokenClass() == TK_CLOSE_BRACE)
        {
            depth--;
        }
    }
}

bool Parser::Match(TokenClass tokenClass)
{
    if (Peek().GetTokenClass() == tokenClass)
    {
        Next();
        return true;
    }

    return false;
}

bool Parser::Expect(TokenClass tokenClass, const char* what)
{
    if (Peek().GetTokenClass() == tokenClass)
    {
        Next();

        return true;
    }

    Error(MSG_EXPECTED_TOKEN, Peek().GetLocation(), String(what));

    return false;
}

bool Parser::ExpectIdentifier(String& outName)
{
    Token token = Peek();

    if (token.GetTokenClass() != TK_IDENT)
    {
        Error(MSG_EXPECTED_IDENTIFIER, token.GetLocation());

        return false;
    }
    
    Next();
    
    outName = token.GetValue();

    return true;
}

bool Parser::IsIdentKeyword(const Token& token, const char* keyword) const
{
    return token.GetTokenClass() == TK_IDENT && token.GetValue() == keyword;
}

void Parser::Error(ErrorMessage msg, const SourceLocation& loc)
{
    m_errorList->AddError(CompilerError(ErrorLevel::Error, msg, loc));
}

void Parser::Error(ErrorMessage msg, const SourceLocation& loc, const String& arg1)
{
    m_errorList->AddError(CompilerError(ErrorLevel::Error, msg, loc, arg1));
}

void Parser::Error(ErrorMessage msg, const SourceLocation& loc, const String& arg1, const String& arg2)
{
    m_errorList->AddError(CompilerError(ErrorLevel::Error, msg, loc, arg1, arg2));
}

void Parser::Warning(ErrorMessage msg, const SourceLocation& loc, const String& arg1)
{
    m_errorList->AddError(CompilerError(ErrorLevel::Warning, msg, loc, arg1));
}

} // namespace Hyperion::DataProcessing::HMF
