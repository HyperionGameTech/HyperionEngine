/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/json/JSON.hpp>
#include <Core/json/parser/Lexer.hpp>

#include <Core/containers/FixedArray.hpp>

#include <Core/math/MathUtil.hpp>

#include <Core/utilities/DeferredScope.hpp>

#include <Core/io/BufferedByteReader.hpp>

// needed for TypeInfo
#include <Core/reflection/BoxedValue.hpp>

namespace Hyperion {
namespace JSON {

static const Value s_undefined = JSON::JSUndefined();
static const Value s_null = JSON::JSNull();
static const Value s_emptyObject = JSON::Object();
static const Value s_emptyArray = JSON::JArray();
static const Value s_emptyString = JSON::JString();
static const Value s_true = true;
static const Value s_false = false;

#pragma region Helpers

static Array<UTF8StringView> SplitStringView(UTF8StringView view, UTF8StringView::CharType separator)
{
    Array<UTF8StringView> tokens;

    uint32 currentIndex = 0;
    uint32 startIndex = 0;
    uint32 endIndex = 0;

    for (utf::Char32 ch : view)
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

static JString GetIndentationString(uint32 depth)
{
    // Preallocate indentation strings
    static const FixedArray<JString, 10> PreallocatedIndentationStrings {
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

    if (depth < PreallocatedIndentationStrings.Size())
    {
        return PreallocatedIndentationStrings[depth];
    }

    JString indentation = PreallocatedIndentationStrings[PreallocatedIndentationStrings.Size() - 1];

    for (uint32 i = PreallocatedIndentationStrings.Size(); i <= depth; i++)
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

    static constexpr utf::Char32 PathSeparator = utf::Char32('.');

    JSONSubscriptWrapper<T> elementSubscriptWrapper = subscriptWrapper;

    while (elementSubscriptWrapper.value && elementSubscriptWrapper.value->IsObject())
    {
        size_t characterIndex = 0;
        UTF8StringView curr = path;

        for (utf::Char32 ch : path)
        {
            if (ch == PathSeparator)
            {
                curr = path.Substr(0, characterIndex);
                path = path.Substr(characterIndex + 1, size_t(-1));

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

            it = asObject.Insert(curr, JSUndefined()).first;
        }

        auto& value = it->second;

        if (createIntermediateObjects && (value.IsUndefined() || value.IsNull()))
        {
            value = Object();
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

            it = asObject.Insert(*parts.Begin(), JSUndefined()).first;
        }

        auto& value = it->second;

        if (createIntermediateObjects && (value.IsUndefined() || value.IsNull()))
        {
            value = Object();
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

#pragma region JSONSubscriptWrapper < Value>

Value& JSONSubscriptWrapper<Value>::Get() const
{
    HYP_CORE_ASSERT(value != nullptr);

    return *value;
}

bool JSONSubscriptWrapper<Value>::IsString() const
{
    return value && value->IsString();
}

bool JSONSubscriptWrapper<Value>::IsNumber() const
{
    return value && value->IsNumber();
}

bool JSONSubscriptWrapper<Value>::IsBool() const
{
    return value && value->IsBool();
}

bool JSONSubscriptWrapper<Value>::IsArray() const
{
    return value && value->IsArray();
}

bool JSONSubscriptWrapper<Value>::IsObject() const
{
    return value && value->IsObject();
}

bool JSONSubscriptWrapper<Value>::IsNull() const
{
    return value && value->IsNull();
}

bool JSONSubscriptWrapper<Value>::IsUndefined() const
{
    return !value || value->IsUndefined();
}

JString& JSONSubscriptWrapper<Value>::AsString() const
{
    HYP_CORE_ASSERT(IsString());

    return value->AsString();
}

JString JSONSubscriptWrapper<Value>::ToString() const
{
    if (!value)
    {
        return JString::empty;
    }

    return value->ToString();
}

Number JSONSubscriptWrapper<Value>::AsNumber() const
{
    HYP_CORE_ASSERT(IsNumber());

    return value->AsNumber();
}

Number JSONSubscriptWrapper<Value>::ToNumber() const
{
    if (!value)
    {
        return Number(0.0);
    }

    return value->ToNumber();
}

bool JSONSubscriptWrapper<Value>::AsBool() const
{
    HYP_CORE_ASSERT(IsBool());

    return value->AsBool();
}

bool JSONSubscriptWrapper<Value>::ToBool() const
{
    if (!value)
    {
        return false;
    }

    return value->ToBool();
}

JArray& JSONSubscriptWrapper<Value>::AsArray() const
{
    HYP_CORE_ASSERT(IsArray());

    return value->AsArray();
}

const JArray& JSONSubscriptWrapper<Value>::ToArray() const
{
    if (!value || !value->IsArray())
    {
        return s_emptyArray.AsArray();
    }

    return value->AsArray();
}

Object& JSONSubscriptWrapper<Value>::AsObject() const
{
    HYP_CORE_ASSERT(IsObject());

    return value->AsObject();
}

const Object& JSONSubscriptWrapper<Value>::ToObject() const
{
    if (!value || !value->IsObject())
    {
        return s_emptyObject.AsObject();
    }

    return value->AsObject();
}

JSONSubscriptWrapper<Value> JSONSubscriptWrapper<Value>::operator[](uint32 index)
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

JSONSubscriptWrapper<const Value> JSONSubscriptWrapper<Value>::operator[](uint32 index) const
{
    return JSONSubscriptWrapper<const Value> { const_cast<remove_const_pointer_t<decltype(this)>>(this)->operator[](index).value };
}

JSONSubscriptWrapper<Value> JSONSubscriptWrapper<Value>::operator[](UTF8StringView key)
{
    if (!value)
    {
        return { nullptr };
    }

    if (value->IsObject())
    {
        Object& asObject = value->AsObject();

        auto it = asObject.FindAs(key);

        if (it == asObject.End())
        {
            return { nullptr };
        }

        return { &it->second };
    }

    return { nullptr };
}

JSONSubscriptWrapper<const Value> JSONSubscriptWrapper<Value>::operator[](UTF8StringView key) const
{
    return JSONSubscriptWrapper<const Value> { const_cast<remove_const_pointer_t<decltype(this)>>(this)->operator[](key).value };
}

JSONSubscriptWrapper<Value> JSONSubscriptWrapper<Value>::Get(UTF8StringView path, bool createIntermediateObjects)
{
    if (!value)
    {
        return *this;
    }

    return SelectHelper(*this, path, createIntermediateObjects);
}

JSONSubscriptWrapper<const Value> JSONSubscriptWrapper<Value>::Get(UTF8StringView path) const
{
    if (!value)
    {
        return JSONSubscriptWrapper<const Value> { value };
    }

    return SelectHelper(JSONSubscriptWrapper<const Value> { value }, path);
}

void JSONSubscriptWrapper<Value>::Set(UTF8StringView path, const Value& value)
{
    Value* target = this->value;

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

HashCode JSONSubscriptWrapper<Value>::GetHashCode() const
{
    if (!value)
    {
        return HashCode();
    }

    return value->GetHashCode();
}

#pragma endregion JSONSubscriptWrapper < Value>

#pragma region JSONSubscriptWrapper < const Value>

const Value& JSONSubscriptWrapper<const Value>::Get() const
{
    HYP_CORE_ASSERT(value != nullptr);

    return *value;
}

bool JSONSubscriptWrapper<const Value>::IsString() const
{
    return value && value->IsString();
}

bool JSONSubscriptWrapper<const Value>::IsNumber() const
{
    return value && value->IsNumber();
}

bool JSONSubscriptWrapper<const Value>::IsBool() const
{
    return value && value->IsBool();
}

bool JSONSubscriptWrapper<const Value>::IsArray() const
{
    return value && value->IsArray();
}

bool JSONSubscriptWrapper<const Value>::IsObject() const
{
    return value && value->IsObject();
}

bool JSONSubscriptWrapper<const Value>::IsNull() const
{
    return value && value->IsNull();
}

bool JSONSubscriptWrapper<const Value>::IsUndefined() const
{
    return !value || value->IsUndefined();
}

const JString& JSONSubscriptWrapper<const Value>::AsString() const
{
    HYP_CORE_ASSERT(IsString());

    return value->AsString();
}

JString JSONSubscriptWrapper<const Value>::ToString() const
{
    if (!value)
    {
        return JString::empty;
    }

    return value->ToString();
}

Number JSONSubscriptWrapper<const Value>::AsNumber() const
{
    HYP_CORE_ASSERT(IsNumber());

    return value->AsNumber();
}

Number JSONSubscriptWrapper<const Value>::ToNumber() const
{
    if (!value)
    {
        return Number(0.0);
    }

    return value->ToNumber();
}

bool JSONSubscriptWrapper<const Value>::AsBool() const
{
    HYP_CORE_ASSERT(IsBool());

    return value->AsBool();
}

bool JSONSubscriptWrapper<const Value>::ToBool() const
{
    if (!value)
    {
        return false;
    }

    return value->ToBool();
}

const JArray& JSONSubscriptWrapper<const Value>::AsArray() const
{
    HYP_CORE_ASSERT(IsArray());

    return value->AsArray();
}

const JArray& JSONSubscriptWrapper<const Value>::ToArray() const
{
    if (!value || !value->IsArray())
    {
        return s_emptyArray.AsArray();
    }

    return value->AsArray();
}

const Object& JSONSubscriptWrapper<const Value>::AsObject() const
{
    HYP_CORE_ASSERT(IsObject());

    return value->AsObject();
}

const Object& JSONSubscriptWrapper<const Value>::ToObject() const
{
    if (!value || !value->IsObject())
    {
        return s_emptyObject.AsObject();
    }

    return value->AsObject();
}

JSONSubscriptWrapper<const Value> JSONSubscriptWrapper<const Value>::operator[](uint32 index) const
{
    if (!value)
    {
        return *this;
    }

    if (value->IsArray())
    {
        const JArray& asArray = value->AsArray();

        if (index >= asArray.Size())
        {
            return { nullptr };
        }

        return { &asArray[index] };
    }

    return { nullptr };
}

JSONSubscriptWrapper<const Value> JSONSubscriptWrapper<const Value>::operator[](UTF8StringView key) const
{
    if (!value)
    {
        return *this;
    }

    if (value->IsObject())
    {
        const Object& asObject = value->AsObject();

        auto it = asObject.FindAs(key);

        if (it == asObject.End())
        {
            return { nullptr };
        }

        return { &it->second };
    }

    return { nullptr };
}

JSONSubscriptWrapper<const Value> JSONSubscriptWrapper<const Value>::Get(UTF8StringView path) const
{
    if (!value)
    {
        return *this;
    }

    return SelectHelper(*this, path);
}

HashCode JSONSubscriptWrapper<const Value>::GetHashCode() const
{
    if (!value)
    {
        return HashCode();
    }

    return value->GetHashCode();
}

#pragma endregion JSONSubscriptWrapper < const Value>

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

    Value Parse()
    {
        JSON::Value value = ParseValue();

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
    Value ParseValue()
    {
        if (Match(TokenClass::TK_OPEN_BRACE, false))
        {
            return Value(ParseObject());
        }

        if (Match(TokenClass::TK_OPEN_BRACKET, false))
        {
            return Value(ParseArray());
        }

        if (Match(TokenClass::TK_STRING, false))
        {
            return Value(ParseString());
        }

        if (Match(TokenClass::TK_INTEGER, false) || Match(TokenClass::TK_FLOAT, false))
        {
            return Value(ParseNumber());
        }

        const SourceLocation location = CurrentLocation();

        if (Token identifier = Match(TokenClass::TK_IDENT, true))
        {
            if (identifier.GetValue() == "true")
            {
                return Value(true);
            }

            if (identifier.GetValue() == "false")
            {
                return Value(false);
            }

            if (identifier.GetValue() == "null")
            {
                return Value(JSNull());
            }

            m_compilationUnit->GetErrorList().AddError(CompilerError(
                ErrorLevel::LEVEL_ERROR,
                ErrorMessage::MSG_UNEXPECTED_IDENTIFIER,
                location));
        }

        return Value(JSUndefined());
    }

    String ParseString()
    {
        if (Token token = Expect(TokenClass::TK_STRING, true))
        {
            return token.GetValue();
        }

        return "";
    }

    Number ParseNumber()
    {
        Token token = Match(TokenClass::TK_INTEGER, true);

        if (!token)
        {
            token = Expect(TokenClass::TK_FLOAT, true);
        }

        if (!token)
        {
            return Number(0);
        }

        std::istringstream ss(token.GetValue().Data());

        Number value;
        ss >> value;

        return value;
    }

    JArray ParseArray()
    {
        JArray array;

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

    Object ParseObject()
    {
        Object object;

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
                    const String key = ParseString();

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

#pragma region Value

Value::Value(const JArray& array)
    : m_inner(JArrayRef::Construct(array))
{
}

Value::Value(JArray&& array)
    : m_inner(JArrayRef::Construct(std::move(array)))
{
}

Value::Value(const Object& object)
    : m_inner(ObjectRef::Construct(object))
{
}

Value::Value(Object&& object)
    : m_inner(ObjectRef::Construct(std::move(object)))
{
}

const Object& Value::ToObject() const
{
    if (IsObject())
    {
        return AsObject();
    }

    return s_emptyObject.AsObject();
}

JString Value::ToString(bool representation, uint32 depth) const
{
    return ToString_Internal(representation, depth);
}

JString Value::ToString_Internal(bool representation, uint32 depth) const
{
    static thread_local HashSet<const Value*> s_serializedObjects;

    if (!s_serializedObjects.Insert(this).second)
    {
        // already serializing this object, circular reference detected
        return "<circular reference>";
    }

    HYP_DEFER({
        s_serializedObjects.Erase(this);
    });

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

    if (IsNull() || IsUndefined())
    {
        if (representation)
        {
            // JSON doesn't have undefined, only null.
            return "null";
        }
        
        // empty string
        return "";
    }

    if (IsNumber())
    {
        // Format string
        const Number number = AsNumber();

        const bool isInteger = MathUtil::Fract(number) < MathUtil::epsilonD;

        Array<char, InlineAllocator<16>> chars;

        // ensure we take up as much space as we can to avoid reallocations
        chars.Resize(chars.Capacity());

        if (isInteger)
        {
            int n = std::snprintf(chars.Data(), chars.Size(), "%lld", (long long)number);
            if (n > int(chars.Size()))
            {
                chars.Resize(size_t(n) + 1);
                std::snprintf(chars.Data(), chars.Size(), "%lld", (long long)number);
            }

            chars.Resize(size_t(n) + 1);
        }
        else
        {
            if (representation && (isnan(number) || !isfinite(number)))
            {
                std::snprintf(chars.Data(), chars.Size(), "null");
            }
            else
            {
                int n = std::snprintf(chars.Data(), chars.Size(), "%f", number);
                if (n > int(chars.Size()))
                {
                    chars.Resize(size_t(n) + 1);
                    std::snprintf(chars.Data(), chars.Size(), "%f", number);
                }

                chars.Resize(size_t(n) + 1);
            }
        }

        return String(chars.ToByteView());
    }

    if (IsArray())
    {
        const JArray& asArray = AsArray();

        JString result = "[";

        for (size_t index = 0; index < asArray.Size(); index++)
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
        const Object& asObject = AsObject();

        Array<const KeyValuePair<JString, Value>*> members;
        members.Reserve(asObject.Size());

        for (const auto& member : asObject)
        {
            members.PushBack(&member);
        }

        const JString indentation = GetIndentationString(depth);
        const JString propertyIndentation = GetIndentationString(depth + 1);

        JString result = "{";

        for (size_t index = 0; index < members.Size(); index++)
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

HashCode Value::GetHashCode() const
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
        return HashCode::GetHashCode(size_t(-1));
    }

    if (IsUndefined())
    {
        return HashCode::GetHashCode(size_t(-2));
    }

    return HashCode();
}

#pragma endregion Value

#pragma region JSON

const Value& Undefined()
{
    return s_undefined;
}

const Value& Null()
{
    return s_null;
}

const Value& EmptyObject()
{
    return s_emptyObject;
}

const Value& EmptyArray()
{
    return s_emptyArray;
}

const Value& EmptyString()
{
    return s_emptyString;
}

const Value& True()
{
    return s_true;
}

const Value& False()
{
    return s_false;
}

ParseResult Parse(BufferedReader& reader)
{
    SourceFile sourceFile("<input>", reader.Max());
    sourceFile.ReadIntoBuffer(reader.ReadBytes());

    return Parse(sourceFile);
}

ParseResult Parse(const String& jsonString)
{
    const size_t bufferLength = jsonString.Size();
    
    SourceFile sourceFile("<input>", bufferLength);
    Assert(sourceFile.GetBuffer().Size() == bufferLength);

    ByteBuffer temp(bufferLength, jsonString.Data());
    sourceFile.ReadIntoBuffer(temp);

    return Parse(sourceFile);
}

ParseResult Parse(const SourceFile& sourceFile)
{
    // use the lexer and parser on this file buffer
    TokenStream tokenStream(TokenStreamInfo { "<input>" });

    CompilationUnit unit;

    const auto handleErrors = [&]() -> ParseResult
    {
        HYP_CORE_ASSERT(unit.GetErrorList().HasFatalErrors());

        String errorMessage;

        for (size_t index = 0; index < unit.GetErrorList().Size(); index++)
        {
            errorMessage += String::ToString(unit.GetErrorList()[index].GetLocation().GetLine() + 1)
                + "," + String::ToString(unit.GetErrorList()[index].GetLocation().GetColumn() + 1)
                + ": " + unit.GetErrorList()[index].GetText() + "\n";
        }

        return { false, errorMessage, Value() };
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

    Value value = parser.Parse();

    if (unit.GetErrorList().HasFatalErrors())
    {
        return handleErrors();
    }

    return { true, "", value };
}

#pragma endregion JSON

} // namespace JSON
} // namespace Hyperion
