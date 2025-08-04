/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/json/JSON.hpp>
#include <core/json/parser/Lexer.hpp>

#include <core/containers/FixedArray.hpp>

#include <core/math/MathUtil.hpp>

#include <core/io/BufferedByteReader.hpp>

namespace hyperion {
namespace json {

static const JSONValue g_undefined = json::JSONUndefined();
static const JSONValue g_null = json::JSONNull();
static const JSONValue g_emptyObject = json::JSONObject();
static const JSONValue g_emptyArray = json::JSONArray();
static const JSONValue g_emptyString = json::JSONString();
static const JSONValue g_true = json::JSONBool(true);
static const JSONValue g_false = json::JSONBool(false);

#pragma region Helpers

static Array<UTF8StringView> SplitStringView(UTF8StringView view, UTF8StringView::CharType separator)
{
    Array<UTF8StringView> tokens;

    uint32 currentIndex = 0;
    uint32 startIndex = 0;
    uint32 endIndex = 0;

    for (utf::u32char ch : view)
    {
        if (ch == separator)
        {
            tokens.PushBack(view.Substr(startIndex, currentIndex));

            currentIndex++;
            startIndex = currentIndex;

            continue;
        }

        currentIndex++;
    }

    if (startIndex != currentIndex)
    {
        tokens.PushBack(view.Substr(startIndex, currentIndex));
    }

    return tokens;
}

static String GetIndentationString(uint32 depth)
{
    // Preallocate indentation strings
    static const FixedArray<String, 10> preallocatedStrings {
        "",
        "  ",
        "    ",
        "      ",
        "        ",
        "          ",
        "            ",
        "              ",
        "                ",
        "                  "
    };

    if (depth < preallocatedStrings.Size())
    {
        return preallocatedStrings[depth];
    }

    String indentation = preallocatedStrings[preallocatedStrings.Size() - 1];

    for (uint32 i = preallocatedStrings.Size(); i <= depth; i++)
    {
        indentation += "  ";
    }

    return indentation;
}

template <class T, typename = std::enable_if_t<!std::is_const_v<T>>>
JSONSubscriptWrapper<T> SelectHelper(const JSONSubscriptWrapper<T>& subscriptWrapper, UTF8StringView path, bool createIntermediateObjects)
{
    if (path.Size() == 0)
    {
        return subscriptWrapper;
    }

    static constexpr utf::u32char separator = utf::u32char('.');
    
    JSONSubscriptWrapper<T> elementSubscriptWrapper = subscriptWrapper;

    while (elementSubscriptWrapper.value && elementSubscriptWrapper.value->IsObject())
    {
        SizeType characterIndex = 0;
        UTF8StringView curr = path;

        for (utf::u32char ch : path)
        {
            if (ch == separator)
            {
                curr = path.Substr(0, characterIndex);
                path = path.Substr(characterIndex + 1, SizeType(-1));
                
                ++characterIndex;

                break;
            }
            
            ++characterIndex;
        }

        auto& asObject = elementSubscriptWrapper.value->AsObject();

        auto it = asObject.FindAs(curr);

        if (it == asObject.End())
        {
            if (!createIntermediateObjects)
            {
                return { nullptr };
            }

            it = asObject.Insert(curr, JSONUndefined()).first;
        }

        auto& value = it->second;

        if (createIntermediateObjects && (value.IsUndefined() || value.IsNull()))
        {
            value = JSONObject();
        }

        elementSubscriptWrapper = JSONSubscriptWrapper<T> { &value };
        
        if (curr.Data() == path.Data()) // no separator found if pointers are the same
        {
            return elementSubscriptWrapper;
        }
    }

    return elementSubscriptWrapper;
}

template <class T>
JSONSubscriptWrapper<const T> SelectHelper(const JSONSubscriptWrapper<const T>& subscriptWrapper, UTF8StringView path)
{
    return SelectHelper(JSONSubscriptWrapper<T>(const_cast<T* const>(subscriptWrapper.value)), path, false);
}

template <class T, typename = std::enable_if_t<!std::is_const_v<T>>>
JSONSubscriptWrapper<T> SelectHelper(const JSONSubscriptWrapper<T>& subscriptWrapper, Span<UTF8StringView> parts, bool createIntermediateObjects)
{
    if (parts.Size() == 0)
    {
        return subscriptWrapper;
    }

    if (subscriptWrapper.value && subscriptWrapper.value->IsObject())
    {
        auto& asObject = subscriptWrapper.value->AsObject();

        auto it = asObject.FindAs(*parts.Begin());

        if (it == asObject.End())
        {
            if (!createIntermediateObjects)
            {
                return { nullptr };
            }

            it = asObject.Insert(*parts.Begin(), JSONUndefined()).first;
        }

        auto& value = it->second;

        if (createIntermediateObjects && (value.IsUndefined() || value.IsNull()))
        {
            value = JSONObject();
        }

        JSONSubscriptWrapper<T> elementSubscriptWrapper { &value };

        return SelectHelper(
            elementSubscriptWrapper,
            parts + 1,
            createIntermediateObjects);
    }

    return JSONSubscriptWrapper<T> { nullptr };
}

template <class T>
JSONSubscriptWrapper<const T> SelectHelper(const JSONSubscriptWrapper<const T>& subscriptWrapper, Span<UTF8StringView> parts)
{
    return SelectHelper(JSONSubscriptWrapper<T>(const_cast<T* const>(subscriptWrapper.value)), parts, false);
}

#pragma endregion Helpers

#pragma region JSONSubscriptWrapper<JSONValue>

JSONValue& JSONSubscriptWrapper<JSONValue>::Get() const
{
    HYP_CORE_ASSERT(value != nullptr);

    return *value;
}

bool JSONSubscriptWrapper<JSONValue>::IsString() const
{
    return value && value->IsString();
}

bool JSONSubscriptWrapper<JSONValue>::IsNumber() const
{
    return value && value->IsNumber();
}

bool JSONSubscriptWrapper<JSONValue>::IsBool() const
{
    return value && value->IsBool();
}

bool JSONSubscriptWrapper<JSONValue>::IsArray() const
{
    return value && value->IsArray();
}

bool JSONSubscriptWrapper<JSONValue>::IsObject() const
{
    return value && value->IsObject();
}

bool JSONSubscriptWrapper<JSONValue>::IsNull() const
{
    return value && value->IsNull();
}

bool JSONSubscriptWrapper<JSONValue>::IsUndefined() const
{
    return !value || value->IsUndefined();
}

JSONString& JSONSubscriptWrapper<JSONValue>::AsString() const
{
    HYP_CORE_ASSERT(IsString());

    return value->AsString();
}

JSONString JSONSubscriptWrapper<JSONValue>::ToString() const
{
    if (!value)
    {
        return JSONString();
    }

    return value->ToString();
}

JSONNumber JSONSubscriptWrapper<JSONValue>::AsNumber() const
{
    HYP_CORE_ASSERT(IsNumber());

    return value->AsNumber();
}

JSONNumber JSONSubscriptWrapper<JSONValue>::ToNumber() const
{
    if (!value)
    {
        return JSONNumber(0.0);
    }

    return value->ToNumber();
}

JSONBool JSONSubscriptWrapper<JSONValue>::AsBool() const
{
    HYP_CORE_ASSERT(IsBool());

    return value->AsBool();
}

JSONBool JSONSubscriptWrapper<JSONValue>::ToBool() const
{
    if (!value)
    {
        return JSONBool(false);
    }

    return value->ToBool();
}

JSONArray& JSONSubscriptWrapper<JSONValue>::AsArray() const
{
    HYP_CORE_ASSERT(IsArray());

    return value->AsArray();
}

const JSONArray& JSONSubscriptWrapper<JSONValue>::ToArray() const
{
    if (!value || !value->IsArray())
    {
        return g_emptyArray.AsArray();
    }

    return value->AsArray();
}

JSONObject& JSONSubscriptWrapper<JSONValue>::AsObject() const
{
    HYP_CORE_ASSERT(IsObject());

    return value->AsObject();
}

const JSONObject& JSONSubscriptWrapper<JSONValue>::ToObject() const
{
    if (!value || !value->IsObject())
    {
        return g_emptyObject.AsObject();
    }

    return value->AsObject();
}

JSONSubscriptWrapper<JSONValue> JSONSubscriptWrapper<JSONValue>::operator[](uint32 index)
{
    if (!value)
    {
        return *this;
    }

    if (value->IsArray())
    {
        auto asArray = value->AsArray();

        if (index >= asArray.Size())
        {
            return { nullptr };
        }

        return { &asArray[index] };
    }

    return { nullptr };
}

JSONSubscriptWrapper<const JSONValue> JSONSubscriptWrapper<JSONValue>::operator[](uint32 index) const
{
    return JSONSubscriptWrapper<const JSONValue> { const_cast<RemoveConstPointer<decltype(this)>>(this)->operator[](index).value };
}

JSONSubscriptWrapper<JSONValue> JSONSubscriptWrapper<JSONValue>::operator[](UTF8StringView key)
{
    if (!value)
    {
        return { nullptr };
    }

    if (value->IsObject())
    {
        JSONObject& asObject = value->AsObject();

        auto it = asObject.FindAs(key);

        if (it == asObject.End())
        {
            return { nullptr };
        }

        return { &it->second };
    }

    return { nullptr };
}

JSONSubscriptWrapper<const JSONValue> JSONSubscriptWrapper<JSONValue>::operator[](UTF8StringView key) const
{
    return JSONSubscriptWrapper<const JSONValue> { const_cast<RemoveConstPointer<decltype(this)>>(this)->operator[](key).value };
}

JSONSubscriptWrapper<JSONValue> JSONSubscriptWrapper<JSONValue>::Get(UTF8StringView path, bool createIntermediateObjects)
{
    if (!value)
    {
        return *this;
    }

    return SelectHelper(*this, path, createIntermediateObjects);
}

JSONSubscriptWrapper<const JSONValue> JSONSubscriptWrapper<JSONValue>::Get(UTF8StringView path) const
{
    if (!value)
    {
        return JSONSubscriptWrapper<const JSONValue> { value };
    }

    return SelectHelper(JSONSubscriptWrapper<const JSONValue> { value }, path);
}

void JSONSubscriptWrapper<JSONValue>::Set(UTF8StringView path, const JSONValue& value)
{
    JSONValue* target = this->value;

    if (!target)
    {
        return;
    }

    Array<UTF8StringView> parts = SplitStringView(path, '.');

    if (parts.Empty())
    {
        return;
    }

    UTF8StringView key = parts.PopBack();

    if (parts.Any())
    {
        auto selectResult = SelectHelper(*this, parts.ToSpan(), true);

        if (!selectResult.value)
        {
            return;
        }

        target = selectResult.value;
    }

    if (target && target->IsObject())
    {
        target->AsObject().Set(key, value);
    }
}

HashCode JSONSubscriptWrapper<JSONValue>::GetHashCode() const
{
    if (!value)
    {
        return HashCode();
    }

    return value->GetHashCode();
}

#pragma endregion JSONSubscriptWrapper < JSONValue>

#pragma region JSONSubscriptWrapper<const JSONValue>

const JSONValue& JSONSubscriptWrapper<const JSONValue>::Get() const
{
    HYP_CORE_ASSERT(value != nullptr);

    return *value;
}

bool JSONSubscriptWrapper<const JSONValue>::IsString() const
{
    return value && value->IsString();
}

bool JSONSubscriptWrapper<const JSONValue>::IsNumber() const
{
    return value && value->IsNumber();
}

bool JSONSubscriptWrapper<const JSONValue>::IsBool() const
{
    return value && value->IsBool();
}

bool JSONSubscriptWrapper<const JSONValue>::IsArray() const
{
    return value && value->IsArray();
}

bool JSONSubscriptWrapper<const JSONValue>::IsObject() const
{
    return value && value->IsObject();
}

bool JSONSubscriptWrapper<const JSONValue>::IsNull() const
{
    return value && value->IsNull();
}

bool JSONSubscriptWrapper<const JSONValue>::IsUndefined() const
{
    return !value || value->IsUndefined();
}

const JSONString& JSONSubscriptWrapper<const JSONValue>::AsString() const
{
    HYP_CORE_ASSERT(IsString());

    return value->AsString();
}

JSONString JSONSubscriptWrapper<const JSONValue>::ToString() const
{
    if (!value)
    {
        return JSONString();
    }

    return value->ToString();
}

JSONNumber JSONSubscriptWrapper<const JSONValue>::AsNumber() const
{
    HYP_CORE_ASSERT(IsNumber());

    return value->AsNumber();
}

JSONNumber JSONSubscriptWrapper<const JSONValue>::ToNumber() const
{
    if (!value)
    {
        return JSONNumber(0.0);
    }

    return value->ToNumber();
}

JSONBool JSONSubscriptWrapper<const JSONValue>::AsBool() const
{
    HYP_CORE_ASSERT(IsBool());

    return value->AsBool();
}

JSONBool JSONSubscriptWrapper<const JSONValue>::ToBool() const
{
    if (!value)
    {
        return JSONBool(false);
    }

    return value->ToBool();
}

const JSONArray& JSONSubscriptWrapper<const JSONValue>::AsArray() const
{
    HYP_CORE_ASSERT(IsArray());

    return value->AsArray();
}

const JSONArray& JSONSubscriptWrapper<const JSONValue>::ToArray() const
{
    if (!value || !value->IsArray())
    {
        return g_emptyArray.AsArray();
    }

    return value->AsArray();
}

const JSONObject& JSONSubscriptWrapper<const JSONValue>::AsObject() const
{
    HYP_CORE_ASSERT(IsObject());

    return value->AsObject();
}

const JSONObject& JSONSubscriptWrapper<const JSONValue>::ToObject() const
{
    if (!value || !value->IsObject())
    {
        return g_emptyObject.AsObject();
    }

    return value->AsObject();
}

JSONSubscriptWrapper<const JSONValue> JSONSubscriptWrapper<const JSONValue>::operator[](uint32 index) const
{
    if (!value)
    {
        return *this;
    }

    if (value->IsArray())
    {
        const JSONArray& asArray = value->AsArray();

        if (index >= asArray.Size())
        {
            return { nullptr };
        }

        return { &asArray[index] };
    }

    return { nullptr };
}

JSONSubscriptWrapper<const JSONValue> JSONSubscriptWrapper<const JSONValue>::operator[](UTF8StringView key) const
{
    if (!value)
    {
        return *this;
    }

    if (value->IsObject())
    {
        const JSONObject& asObject = value->AsObject();

        auto it = asObject.FindAs(key);

        if (it == asObject.End())
        {
            return { nullptr };
        }

        return { &it->second };
    }

    return { nullptr };
}

JSONSubscriptWrapper<const JSONValue> JSONSubscriptWrapper<const JSONValue>::Get(UTF8StringView path) const
{
    if (!value)
    {
        return *this;
    }

    return SelectHelper(*this, path);
}

HashCode JSONSubscriptWrapper<const JSONValue>::GetHashCode() const
{
    if (!value)
    {
        return HashCode();
    }

    return value->GetHashCode();
}

#pragma endregion JSONSubscriptWrapper < const JSONValue>

#pragma region JSONParser

class JSONParser
{
public:
    JSONParser(
        TokenStream* tokenStream,
        CompilationUnit* compilationUnit)
        : m_tokenStream(tokenStream),
          m_compilationUnit(compilationUnit)
    {
    }

    JSONParser(const JSONParser& other) = delete;
    JSONParser& operator=(JSONParser& other) = delete;

    JSONParser(JSONParser&& other) noexcept = delete;
    JSONParser& operator=(JSONParser&& other) noexcept = delete;

    ~JSONParser() = default;

    JSONValue Parse()
    {
        json::JSONValue value = ParseValue();

        // Should not have any tokens left
        if (m_tokenStream->HasNext())
        {
            m_compilationUnit->GetErrorList().AddError(CompilerError(
                ErrorLevel::LEVEL_ERROR,
                ErrorMessage::MSG_UNEXPECTED_TOKEN,
                CurrentLocation()));
        }

        return value;
    }

private:
    JSONValue ParseValue()
    {
        if (Match(TokenClass::TK_OPEN_BRACE, false))
        {
            return JSONValue(ParseObject());
        }

        if (Match(TokenClass::TK_OPEN_BRACKET, false))
        {
            return JSONValue(ParseArray());
        }

        if (Match(TokenClass::TK_STRING, false))
        {
            return JSONValue(ParseString());
        }

        if (Match(TokenClass::TK_INTEGER, false) || Match(TokenClass::TK_FLOAT, false))
        {
            return JSONValue(ParseNumber());
        }

        const SourceLocation location = CurrentLocation();

        if (Token identifier = Match(TokenClass::TK_IDENT, true))
        {
            if (identifier.GetValue() == "true")
            {
                return JSONValue(true);
            }

            if (identifier.GetValue() == "false")
            {
                return JSONValue(false);
            }

            if (identifier.GetValue() == "null")
            {
                return JSONValue(JSONNull());
            }

            m_compilationUnit->GetErrorList().AddError(CompilerError(
                ErrorLevel::LEVEL_ERROR,
                ErrorMessage::MSG_UNEXPECTED_IDENTIFIER,
                location));
        }

        return JSONValue(JSONUndefined());
    }

    JSONString ParseString()
    {
        if (Token token = Expect(TokenClass::TK_STRING, true))
        {
            return token.GetValue();
        }

        return "";
    }

    JSONNumber ParseNumber()
    {
        Token token = Match(TokenClass::TK_INTEGER, true);

        if (!token)
        {
            token = Expect(TokenClass::TK_FLOAT, true);
        }

        if (!token)
        {
            return JSONNumber(0);
        }

        std::istringstream ss(token.GetValue().Data());

        JSONNumber value;
        ss >> value;

        return value;
    }

    JSONArray ParseArray()
    {
        JSONArray array;

        if (Token token = Expect(TokenClass::TK_OPEN_BRACKET, true))
        {
            do
            {
                if (Match(TokenClass::TK_CLOSE_BRACKET, false))
                {
                    break;
                }

                array.PushBack(ParseValue());
            }
            while (Match(TokenClass::TK_COMMA, true));

            Expect(TokenClass::TK_CLOSE_BRACKET, true);
        }

        return array;
    }

    JSONObject ParseObject()
    {
        JSONObject object;

        if (Token token = Expect(TokenClass::TK_OPEN_BRACE, true))
        {
            do
            {
                if (Match(TokenClass::TK_CLOSE_BRACE, false))
                {
                    break;
                }

                if (Match(TokenClass::TK_STRING, false))
                {
                    const JSONString key = ParseString();

                    if (Expect(TokenClass::TK_COLON, true))
                    {
                        object[key] = ParseValue();
                    }
                }
            }
            while (Match(TokenClass::TK_COMMA, true));

            Expect(TokenClass::TK_CLOSE_BRACE, true);
        }

        return object;
    }

    SourceLocation CurrentLocation() const
    {
        if (m_tokenStream->GetSize() != 0 && !m_tokenStream->HasNext())
        {
            return m_tokenStream->Last().GetLocation();
        }

        return m_tokenStream->Peek().GetLocation();
    }

    Token Match(TokenClass tokenClass, bool read)
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

        return Token::empty;
    }

    Token MatchAhead(TokenClass tokenClass, int n)
    {
        Token peek = m_tokenStream->Peek(n);

        if (peek && peek.GetTokenClass() == tokenClass)
        {
            return peek;
        }

        return Token::empty;
    }

    Token Expect(TokenClass tokenClass, bool read)
    {
        Token token = Match(tokenClass, read);

        if (!token)
        {
            const SourceLocation location = CurrentLocation();

            ErrorMessage errorMsg;
            String errorStr;

            switch (tokenClass)
            {
            case TokenClass::TK_IDENT:
                errorMsg = ErrorMessage::MSG_EXPECTED_IDENTIFIER;
                break;
            default:
                errorMsg = ErrorMessage::MSG_EXPECTED_TOKEN;
                errorStr = Token::TokenTypeToString(tokenClass);
            }

            m_compilationUnit->GetErrorList().AddError(CompilerError(
                ErrorLevel::LEVEL_ERROR,
                errorMsg,
                location,
                errorStr));
        }

        return token;
    }

    Token MatchIdentifier(const String& value, bool read)
    {
        const Token token = Match(TokenClass::TK_IDENT, false);

        if (!token)
        {
            return Token::empty;
        }

        if (token.GetValue() != value)
        {
            return Token::empty;
        }

        if (read && m_tokenStream->HasNext())
        {
            // read the token since it was matched
            m_tokenStream->Next();
        }

        return token;
    }

    Token ExpectIdentifier(const String& value, bool read)
    {
        Token token = MatchIdentifier(value, read);

        if (!token)
        {
            const SourceLocation location = CurrentLocation();

            m_compilationUnit->GetErrorList().AddError(CompilerError(
                ErrorLevel::LEVEL_ERROR,
                ErrorMessage::MSG_UNEXPECTED_IDENTIFIER,
                location));

            // Skip the token
            if (read && m_tokenStream->HasNext())
            {
                m_tokenStream->Next();
            }
        }

        return token;
    }

    TokenStream* m_tokenStream;
    CompilationUnit* m_compilationUnit;
};

#pragma endregion JSONParser

#pragma region JSONValue

JSONValue::JSONValue(const JSONArray& array)
    : m_inner(JSONArrayRef::Construct(array))
{
}

JSONValue::JSONValue(JSONArray&& array)
    : m_inner(JSONArrayRef::Construct(std::move(array)))
{
}

JSONValue::JSONValue(const JSONObject& object)
    : m_inner(JSONObjectRef::Construct(object))
{
}

JSONValue::JSONValue(JSONObject&& object)
    : m_inner(JSONObjectRef::Construct(std::move(object)))
{
}

const JSONObject& JSONValue::ToObject() const
{
    if (IsObject())
    {
        return AsObject();
    }

    return g_emptyObject.AsObject();
}

JSONString JSONValue::ToString(bool representation, uint32 depth) const
{
    return ToString_Internal(representation, depth);
}

JSONString JSONValue::ToString_Internal(bool representation, uint32 depth) const
{
    if (IsString())
    {
        if (representation)
        {
            return "\"" + AsString().Escape() + "\"";
        }
        else
        {
            return AsString();
        }
    }

    if (IsBool())
    {
        return (AsBool() ? "true" : "false");
    }

    if (IsNull())
    {
        return "null";
    }

    if (IsUndefined())
    {
        return "undefined";
    }

    if (IsNumber())
    {
        // Format string
        const JSONNumber number = AsNumber();

        const bool isInteger = MathUtil::Fract(number) < MathUtil::epsilonD;

        Array<char, InlineAllocator<16>> chars;

        // ensure we take up as much space as we can to avoid reallocations
        chars.Resize(chars.Capacity());

        if (isInteger)
        {
            int n = std::snprintf(chars.Data(), chars.Size(), "%lld", (long long)number);
            if (n > int(chars.Size()))
            {
                chars.Resize(SizeType(n) + 1);
                std::snprintf(chars.Data(), chars.Size(), "%lld", (long long)number);
            }

            chars.Resize(SizeType(n) + 1);
        }
        else
        {
            int n = std::snprintf(chars.Data(), chars.Size(), "%f", number);
            if (n > int(chars.Size()))
            {
                chars.Resize(SizeType(n) + 1);
                std::snprintf(chars.Data(), chars.Size(), "%f", number);
            }

            chars.Resize(SizeType(n) + 1);
        }

        return String(chars.ToByteView());
    }

    if (IsArray())
    {
        const JSONArray& asArray = AsArray();

        String result = "[";

        for (SizeType index = 0; index < asArray.Size(); index++)
        {
            result += asArray[index].ToString(true, depth + 1);

            if (index != asArray.Size() - 1)
            {
                result += ", ";
            }
        }

        result += "]";

        return result;
    }

    if (IsObject())
    {
        const JSONObject& asObject = AsObject();

        Array<const KeyValuePair<JSONString, JSONValue>*> members;
        members.Reserve(asObject.Size());

        for (const auto& member : asObject)
        {
            members.PushBack(&member);
        }

        const String indentation = GetIndentationString(depth);
        const String propertyIndentation = GetIndentationString(depth + 1);

        String result = "{";

        for (SizeType index = 0; index < members.Size(); index++)
        {
            result += "\n" + propertyIndentation + "\"" + members[index]->first.Escape() + "\": ";

            result += members[index]->second.ToString(true, depth + 1);

            if (index != members.Size() - 1)
            {
                result += ",";
            }
            else
            {
                result += "\n" + indentation;
            }
        }

        result += "}";

        return result;
    }

    if (representation)
    {
        return "\"<invalid value>\"";
    }
    else
    {
        return "<invalid value>";
    }
}

HashCode JSONValue::GetHashCode() const
{
    if (IsString())
    {
        return HashCode::GetHashCode(AsString());
    }

    if (IsNumber())
    {
        return HashCode::GetHashCode(AsNumber());
    }

    if (IsBool())
    {
        return HashCode::GetHashCode(AsBool());
    }

    if (IsArray())
    {
        return HashCode::GetHashCode(AsArray());
    }

    if (IsObject())
    {
        return HashCode::GetHashCode(AsObject());
    }

    if (IsNull())
    {
        return HashCode::GetHashCode(SizeType(-1));
    }

    if (IsUndefined())
    {
        return HashCode::GetHashCode(SizeType(-2));
    }

    return HashCode();
}

#pragma endregion JSONValue

#pragma region JSON

const JSONValue& JSON::Undefined()
{
    return g_undefined;
}

const JSONValue& JSON::Null()
{
    return g_null;
}

const JSONValue& JSON::EmptyObject()
{
    return g_emptyObject;
}

const JSONValue& JSON::EmptyArray()
{
    return g_emptyArray;
}

const JSONValue& JSON::EmptyString()
{
    return g_emptyString;
}

const JSONValue& JSON::True()
{
    return g_true;
}

const JSONValue& JSON::False()
{
    return g_false;
}

ParseResult JSON::Parse(BufferedReader& reader)
{
    SourceFile sourceFile("<input>", reader.Max());
    sourceFile.ReadIntoBuffer(reader.ReadBytes());

    return Parse(sourceFile);
}

ParseResult JSON::Parse(const String& jsonString)
{
    SourceFile sourceFile("<input>", jsonString.Size());

    ByteBuffer temp(jsonString.Size(), jsonString.Data());
    sourceFile.ReadIntoBuffer(temp);

    return Parse(sourceFile);
}

ParseResult JSON::Parse(const SourceFile& sourceFile)
{
    // use the lexer and parser on this file buffer
    TokenStream tokenStream(TokenStreamInfo { "<input>" });

    CompilationUnit unit;

    const auto handleErrors = [&]() -> ParseResult
    {
        HYP_CORE_ASSERT(unit.GetErrorList().HasFatalErrors());

        String errorMessage;

        for (SizeType index = 0; index < unit.GetErrorList().Size(); index++)
        {
            errorMessage += String::ToString(unit.GetErrorList()[index].GetLocation().GetLine() + 1)
                + "," + String::ToString(unit.GetErrorList()[index].GetLocation().GetColumn() + 1)
                + ": " + unit.GetErrorList()[index].GetText() + "\n";
        }

        return { false, errorMessage, JSONValue() };
    };

    Lexer lexer(SourceStream(&sourceFile), &tokenStream, &unit);
    lexer.Analyze();

    if (unit.GetErrorList().HasFatalErrors())
    {
        return handleErrors();
    }

    JSONParser parser(
        &tokenStream,
        &unit);

    JSONValue value = parser.Parse();

    if (unit.GetErrorList().HasFatalErrors())
    {
        return handleErrors();
    }

    return { true, "", value };
}

#pragma endregion JSON

} // namespace json
} // namespace hyperion
