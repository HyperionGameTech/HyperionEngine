#include <util/json/JSON.hpp>

namespace hyperion {
namespace json {

JSONValue &JSONSubscriptWrapper<JSONValue>::Get()
{
    AssertThrow(value != nullptr);

    return *value;
}

const JSONValue &JSONSubscriptWrapper<JSONValue>::Get() const
{
    AssertThrow(value != nullptr);

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

JSONString &JSONSubscriptWrapper<JSONValue>::AsString()
{
    AssertThrow(IsString());

    return value->AsString();
}

const JSONString &JSONSubscriptWrapper<JSONValue>::AsString() const
{
    AssertThrow(IsString());

    return value->AsString();
}

JSONString JSONSubscriptWrapper<JSONValue>::ToString() const
{
    if (!value) {
        return JSONString();
    }

    return value->ToString();
}

JSONNumber &JSONSubscriptWrapper<JSONValue>::AsNumber()
{
    AssertThrow(IsNumber());

    return value->AsNumber();
}

JSONNumber JSONSubscriptWrapper<JSONValue>::AsNumber() const
{
    AssertThrow(IsNumber());

    return value->AsNumber();
}

JSONNumber JSONSubscriptWrapper<JSONValue>::ToNumber() const
{
    if (!value) {
        return JSONNumber(0.0);
    }

    return value->ToNumber();
}

JSONBool &JSONSubscriptWrapper<JSONValue>::AsBool()
{
    AssertThrow(IsBool());

    return value->AsBool();
}

JSONBool JSONSubscriptWrapper<JSONValue>::AsBool() const
{
    AssertThrow(IsBool());

    return value->AsBool();
}

JSONBool JSONSubscriptWrapper<JSONValue>::ToBool() const
{
    if (!value) {
        return JSONBool(false);
    }

    return value->ToBool();
}

JSONArray &JSONSubscriptWrapper<JSONValue>::AsArray()
{
    AssertThrow(IsArray());

    return value->AsArray();
}

const JSONArray &JSONSubscriptWrapper<JSONValue>::AsArray() const
{
    AssertThrow(IsArray());

    return value->AsArray();
}

JSONArray JSONSubscriptWrapper<JSONValue>::ToArray() const
{
    if (!value) {
        return JSONArray();
    }

    return value->ToArray();
}

JSONObject &JSONSubscriptWrapper<JSONValue>::AsObject()
{
    AssertThrow(IsObject());

    return value->AsObject();
}

const JSONObject &JSONSubscriptWrapper<JSONValue>::AsObject() const
{
    AssertThrow(IsObject());

    return value->AsObject();
}

JSONObject JSONSubscriptWrapper<JSONValue>::ToObject() const
{
    if (!value) {
        return JSONObject();
    }

    return value->ToObject();
}

JSONSubscriptWrapper<JSONValue> JSONSubscriptWrapper<JSONValue>::operator[](uint index)
{
    if (!value) {
        return *this;
    }

    if (value->IsArray()) {
        auto as_array = value->AsArray();

        if (index >= as_array.Size()) {
            return { nullptr };
        }

        return { &as_array[index] };
    }

    return { nullptr };
}

JSONSubscriptWrapper<const JSONValue> JSONSubscriptWrapper<JSONValue>::operator[](uint index) const
{
    return JSONSubscriptWrapper<const JSONValue> { const_cast<RemoveConstPointer<decltype(this)>>(this)->operator[](index).value };
}

JSONSubscriptWrapper<JSONValue> JSONSubscriptWrapper<JSONValue>::operator[](const JSONString &key)
{
    if (!value) {
        return *this;
    }

    if (value->IsObject()) {
        JSONObject &as_object = value->AsObject();

        auto it = as_object.Find(key);

        if (it == as_object.End()) {
            return { nullptr };
        }

        return { &it->second };
    }

    return { nullptr };
}

JSONSubscriptWrapper<const JSONValue> JSONSubscriptWrapper<JSONValue>::operator[](const JSONString &key) const
{
    return JSONSubscriptWrapper<const JSONValue> { const_cast<RemoveConstPointer<decltype(this)>>(this)->operator[](key).value };
}


const JSONValue &JSONSubscriptWrapper<const JSONValue>::Get() const
{
    AssertThrow(value != nullptr);

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

const JSONString &JSONSubscriptWrapper<const JSONValue>::AsString() const
{
    AssertThrow(IsString());

    return value->AsString();
}

JSONString JSONSubscriptWrapper<const JSONValue>::ToString() const
{
    if (!value) {
        return JSONString();
    }

    return value->ToString();
}

JSONNumber JSONSubscriptWrapper<const JSONValue>::AsNumber() const
{
    AssertThrow(IsNumber());

    return value->AsNumber();
}

JSONNumber JSONSubscriptWrapper<const JSONValue>::ToNumber() const
{
    if (!value) {
        return JSONNumber(0.0);
    }

    return value->ToNumber();
}

JSONBool JSONSubscriptWrapper<const JSONValue>::AsBool() const
{
    AssertThrow(IsBool());

    return value->AsBool();
}

JSONBool JSONSubscriptWrapper<const JSONValue>::ToBool() const
{
    if (!value) {
        return JSONBool(false);
    }

    return value->ToBool();
}

const JSONArray &JSONSubscriptWrapper<const JSONValue>::AsArray() const
{
    AssertThrow(IsArray());

    return value->AsArray();
}

JSONArray JSONSubscriptWrapper<const JSONValue>::ToArray() const
{
    if (!value) {
        return JSONArray();
    }

    return value->ToArray();
}

const JSONObject &JSONSubscriptWrapper<const JSONValue>::AsObject() const
{
    AssertThrow(IsObject());

    return value->AsObject();
}

JSONObject JSONSubscriptWrapper<const JSONValue>::ToObject() const
{
    if (!value) {
        return JSONObject();
    }

    return value->ToObject();
}

JSONSubscriptWrapper<const JSONValue> JSONSubscriptWrapper<const JSONValue>::operator[](uint index) const
{
    if (!value) {
        return *this;
    }

    if (value->IsArray()) {
        const JSONArray &as_array = value->AsArray();

        if (index >= as_array.Size()) {
            return { nullptr };
        }

        return { &as_array[index] };
    }

    return { nullptr };
}

JSONSubscriptWrapper<const JSONValue> JSONSubscriptWrapper<const JSONValue>::operator[](const JSONString &key) const
{
    if (!value) {
        return *this;
    }

    if (value->IsObject()) {
        const JSONObject &as_object = value->AsObject();

        auto it = as_object.Find(key);

        if (it == as_object.End()) {
            return { nullptr };
        }

        return { &it->second };
    }

    return { nullptr };
}

class JSONParser
{
public:
    JSONParser(
        TokenStream *token_stream,
        CompilationUnit *compilation_unit
    ) : m_token_stream(token_stream),
        m_compilation_unit(compilation_unit)
    {
    }

    JSONParser(const JSONParser &other) = delete;
    JSONParser &operator=(JSONParser &other) = delete;

    JSONParser(JSONParser &&other) noexcept = delete;
    JSONParser &operator=(JSONParser &&other) noexcept = delete;

    ~JSONParser() = default;

    JSONValue Parse()
    {
        return ParseValue();
    }

private:
    JSONValue ParseValue()
    {
        if (Match(TokenClass::TK_OPEN_BRACE, false)) {
            return JSONValue(ParseObject());
        }

        if (Match(TokenClass::TK_OPEN_BRACKET, false)) {
            return JSONValue(ParseArray());
        }

        if (Match(TokenClass::TK_STRING, false)) {
            return JSONValue(ParseString());
        }

        if (Match(TokenClass::TK_INTEGER, false) || Match(TokenClass::TK_FLOAT, false)) {
            return JSONValue(ParseNumber());
        }

        if (MatchIdentifier("true", true)) {
            return JSONValue(true);
        }

        if (MatchIdentifier("false", true)) {
            return JSONValue(false);
        }

        if (MatchIdentifier("null", true)) {
            return JSONValue(JSONNull());
        }

        return JSONValue(JSONUndefined());
    }

    JSONString ParseString()
    {
        if (Token token = Expect(TokenClass::TK_STRING, true)) {
            return token.GetValue();
        }

        return "";
    }

    JSONNumber ParseNumber()
    {
        Token token = Match(TokenClass::TK_INTEGER, true);

        if (!token) {
            token = Expect(TokenClass::TK_FLOAT, true);
        }

        if (!token) {
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

        if (Token token = Expect(TokenClass::TK_OPEN_BRACKET, true)) {
            do {
                if (Match(TokenClass::TK_CLOSE_BRACKET, false)) {
                    break;
                }

                array.PushBack(ParseValue());
            } while (Match(TokenClass::TK_COMMA, true));

            Expect(TokenClass::TK_CLOSE_BRACKET, true);
        }

        return array;
    }

    JSONObject ParseObject()
    {
        JSONObject object;

        if (Token token = Expect(TokenClass::TK_OPEN_BRACE, true)) {
            do {
                if (Match(TokenClass::TK_CLOSE_BRACE, false)) {
                    break;
                }

                if (Match(TokenClass::TK_STRING, false)) {
                    const JSONString key = ParseString();

                    if (Expect(TokenClass::TK_COLON, true)) {
                        object[key] = ParseValue();
                    }
                }
            } while (Match(TokenClass::TK_COMMA, true));

            Expect(TokenClass::TK_CLOSE_BRACE, true);
        }

        return object;
    }

    SourceLocation CurrentLocation() const
    {
        if (m_token_stream->GetSize() != 0 && !m_token_stream->HasNext()) {
            return m_token_stream->Last().GetLocation();
        }

        return m_token_stream->Peek().GetLocation();
    }

    Token Match(TokenClass token_class, bool read)
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

    Token MatchAhead(TokenClass token_class, int n)
    {
        Token peek = m_token_stream->Peek(n);
        
        if (peek && peek.GetTokenClass() == token_class) {
            return peek;
        }
        
        return Token::EMPTY;
    }

    Token Expect(TokenClass token_class, bool read)
    {
        Token token = Match(token_class, read);
        
        if (!token) {
            const SourceLocation location = CurrentLocation();

            ErrorMessage error_msg;
            String error_str;

            switch (token_class) {
            case TokenClass::TK_IDENT:
                error_msg = ErrorMessage::Msg_expected_identifier;
                break;
            default:
                error_msg = ErrorMessage::Msg_expected_token;
                error_str = Token::TokenTypeToString(token_class);
            }

            m_compilation_unit->GetErrorList().AddError(CompilerError(
                ErrorLevel::LEVEL_ERROR,
                error_msg,
                location,
                error_str
            ));
        }

        return token;
    }

    Token MatchIdentifier(const String &value, bool read)
    {
        const Token token = Match(TokenClass::TK_IDENT, read);

        if (!token) {
            return Token::EMPTY;
        }

        if (token.GetValue() != value) {
            return Token::EMPTY;
        }

        return token;
    }

    Token ExpectIdentifier(const String &value, bool read)
    {
        Token token = MatchIdentifier(value, read);
        
        if (!token) {
            const SourceLocation location = CurrentLocation();

            m_compilation_unit->GetErrorList().AddError(CompilerError(
                ErrorLevel::LEVEL_ERROR,
                ErrorMessage::Msg_expected_identifier,
                location
            ));
        }

        return token;
    }

    TokenStream *m_token_stream;
    CompilationUnit *m_compilation_unit;
};

ParseResult JSON::Parse(const String &json_string)
{
    SourceFile source_file("<input>", json_string.Size());

    ByteBuffer temp(json_string.Size(), json_string.Data());
    source_file.ReadIntoBuffer(temp);

    // use the lexer and parser on this file buffer
    TokenStream token_stream(TokenStreamInfo { "<input>" });

    CompilationUnit unit;

    const auto HandleErrors = [&]() -> ParseResult {
        AssertThrow(unit.GetErrorList().HasFatalErrors());

        String error_message;

        for (SizeType index = 0; index < unit.GetErrorList().Size(); index++) {
            error_message += String::ToString(unit.GetErrorList()[index].GetLocation().GetLine() + 1)
                + "," + String::ToString(unit.GetErrorList()[index].GetLocation().GetColumn() + 1)
                + ": " + unit.GetErrorList()[index].GetText() + "\n";
        }

        return { false, error_message, JSONValue() };
    };

    Lexer lexer(SourceStream(&source_file), &token_stream, &unit);
    lexer.Analyze();

    if (unit.GetErrorList().HasFatalErrors()) {
        return HandleErrors();
    }

    JSONParser parser(
        &token_stream,
        &unit
    );

    JSONValue value = parser.Parse();

    if (unit.GetErrorList().HasFatalErrors()) {
        return HandleErrors();
    }

    return { true, "", value };
}

}
}