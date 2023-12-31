#ifndef HYPERION_JSON_HPP
#define HYPERION_JSON_HPP

#include <asset/ByteReader.hpp>
#include <script/compiler/Lexer.hpp> // Reuse scripting language lexer
#include <core/lib/String.hpp>
#include <core/lib/Variant.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/HashMap.hpp>

#include "util/StringUtil.hpp"

namespace hyperion {
namespace json {

template <class JSONValueType>
struct JSONSubscriptWrapper;

class JSONValue;

using JSONString = String;
using JSONNumber = Double;
using JSONBool = Bool;
using JSONArray = Array<JSONValue>;
using JSONObject = HashMap<JSONString, JSONValue>;
using JSONArrayRef = RC<JSONArray>;
using JSONObjectRef = RC<JSONObject>;
struct JSONNull { };
struct JSONUndefined { };

template <class JSONValueType>
struct JSONSubscriptWrapper
{
};

template <>
struct JSONSubscriptWrapper<const JSONValue>
{
    const JSONValue *value = nullptr;

    JSONSubscriptWrapper(const JSONValue *value)
        : value(value)
    {
    }

    JSONSubscriptWrapper(const JSONSubscriptWrapper &other)
        : value(other.value)
    {
    }

    JSONSubscriptWrapper &operator=(const JSONSubscriptWrapper &other)
    {
        if (std::addressof(*this) == std::addressof(other)) {
            return *this;
        }

        value = other.value;

        return *this;
    }

    JSONSubscriptWrapper(JSONSubscriptWrapper &&other) noexcept
        : value(other.value)
    {
        other.value = nullptr;
    }

    JSONSubscriptWrapper &operator=(JSONSubscriptWrapper &&other) noexcept
    {
        if (std::addressof(*this) == std::addressof(other)) {
            return *this;
        }

        value = other.value;

        other.value = nullptr;

        return *this;
    }

    ~JSONSubscriptWrapper() = default;

    const JSONValue &Get() const;
    
    bool IsString() const;
    bool IsNumber() const;
    bool IsBool() const;
    bool IsArray() const;
    bool IsObject() const;
    bool IsNull() const;
    bool IsUndefined() const;

    const JSONString &AsString() const;
    JSONString ToString() const;

    JSONNumber AsNumber() const;
    JSONNumber ToNumber() const;

    JSONBool AsBool() const;
    JSONBool ToBool() const;

    const JSONArray &AsArray() const;
    JSONArray ToArray() const;

    const JSONObject &AsObject() const;
    JSONObject ToObject() const;

    JSONSubscriptWrapper<const JSONValue> operator[](UInt index) const;
    JSONSubscriptWrapper<const JSONValue> operator[](const JSONString &key) const;
};

template <>
struct JSONSubscriptWrapper<JSONValue>
{
    JSONValue *value = nullptr;

    JSONSubscriptWrapper(JSONValue *value)
        : value(value)
    {
    }

    JSONSubscriptWrapper(const JSONSubscriptWrapper &other)
        : value(other.value)
    {
    }

    JSONSubscriptWrapper &operator=(const JSONSubscriptWrapper &other)
    {
        if (std::addressof(*this) == std::addressof(other)) {
            return *this;
        }

        value = other.value;

        return *this;
    }

    JSONSubscriptWrapper(JSONSubscriptWrapper &&other) noexcept
        : value(other.value)
    {
        other.value = nullptr;
    }

    JSONSubscriptWrapper &operator=(JSONSubscriptWrapper &&other) noexcept
    {
        if (std::addressof(*this) == std::addressof(other)) {
            return *this;
        }

        value = other.value;

        other.value = nullptr;

        return *this;
    }

    ~JSONSubscriptWrapper() = default;

    JSONValue &Get();
    const JSONValue &Get() const;
    
    bool IsString() const;
    bool IsNumber() const;
    bool IsBool() const;
    bool IsArray() const;
    bool IsObject() const;
    bool IsNull() const;
    bool IsUndefined() const;

    JSONString &AsString();
    const JSONString &AsString() const;
    JSONString ToString() const;

    JSONNumber &AsNumber();
    JSONNumber AsNumber() const;
    JSONNumber ToNumber() const;

    JSONBool &AsBool();
    JSONBool AsBool() const;
    JSONBool ToBool() const;

    JSONArray &AsArray();
    const JSONArray &AsArray() const;
    JSONArray ToArray() const;

    JSONObject &AsObject();
    const JSONObject &AsObject() const;
    JSONObject ToObject() const;

    JSONSubscriptWrapper<JSONValue> operator[](UInt index);
    JSONSubscriptWrapper<const JSONValue> operator[](UInt index) const;
    JSONSubscriptWrapper<JSONValue> operator[](const JSONString &key);
    JSONSubscriptWrapper<const JSONValue> operator[](const JSONString &key) const;
};

class JSONValue
{
private:
    using InnerType = Variant<JSONString, JSONNumber, JSONBool, JSONArrayRef, JSONObjectRef, JSONNull, JSONUndefined>;

public:
    JSONValue()
        : m_inner(JSONUndefined { })
    {
    }

    JSONValue(JSONString string)
        : m_inner(std::move(string))
    {
    }

    JSONValue(const char *string)
        : JSONValue(JSONString(string))
    {
    }

    JSONValue(JSONNumber number)
        : m_inner(number)
    {
    }

    JSONValue(JSONBool boolean)
        : m_inner(boolean)
    {
    }

    JSONValue(JSONArray array)
        : m_inner(JSONArrayRef::Construct(std::move(array)))
    {
    }

    JSONValue(JSONObject object)
        : m_inner(JSONObjectRef::Construct(std::move(object)))
    {
    }

    JSONValue(JSONNull _null)
        : m_inner(std::move(_null))
    {
    }

    JSONValue(JSONUndefined undefined)
        : m_inner(std::move(undefined))
    {
    }

    JSONValue(const JSONValue &other)
        : m_inner(other.m_inner)
    {
    }

    JSONValue &operator=(const JSONValue &other)
    {
        m_inner = other.m_inner;

        return *this;
    }

    JSONValue(JSONValue &&other) noexcept
        : m_inner(std::move(other.m_inner))
    {
    }

    JSONValue &operator=(JSONValue &&other) noexcept
    {
        m_inner = std::move(other.m_inner);

        return *this;
    }

    ~JSONValue() = default;

    bool IsString() const
    {
        return m_inner.Is<JSONString>();
    }

    bool IsNumber() const
    {
        return m_inner.Is<JSONNumber>();
    }

    bool IsBool() const
    {
        return m_inner.Is<JSONBool>();
    }

    bool IsArray() const
    {
        return m_inner.Is<JSONArrayRef>();
    }

    bool IsObject() const
    {
        return m_inner.Is<JSONObjectRef>();
    }

    bool IsNull() const
    {
        return m_inner.Is<JSONNull>();
    }

    bool IsUndefined() const
    {
        return m_inner.Is<JSONUndefined>();
    }

    JSONString &AsString()
    {
        AssertThrow(IsString());

        return m_inner.Get<JSONString>();
    }

    const JSONString &AsString() const
    {
        AssertThrow(IsString());

        return m_inner.Get<JSONString>();
    }

    JSONString ToString(bool representation = false) const
    {
        if (IsString()) {
            if (representation) {
                return "\"" + AsString().Escape() + "\"";
            } else {
                return AsString();
            }
        }

        if (IsBool()) {
            return AsBool() ? "true" : "false";
        }

        if (IsNull()) {
            return "null";
        }

        if (IsUndefined()) {
            return "undefined";
        }

        if (IsNumber()) {
            return String(std::to_string(AsNumber()).c_str());
        }

        if (IsArray()) {
            const auto &as_array = AsArray();

            String result = "[";

            for (SizeType index = 0; index < as_array.Size(); index++) {
                result += as_array[index].ToString(true);

                if (index != as_array.Size() - 1) {
                    result += ", ";
                }
            }

            result += "]";

            return result;
        }

        if (IsObject()) {
            const auto &as_object = AsObject();

            Array<Pair<JSONString, JSONValue>> members;

            for (const auto &member : as_object) {
                members.PushBack({ member.first, member.second });
            }

            String result = "{";

            for (SizeType index = 0; index < members.Size(); index++) {
                result += "\"" + members[index].first.Escape() + "\": ";

                result += members[index].second.ToString(true);

                if (index != members.Size() - 1) {
                    result += ", ";
                }
            }

            result += "}";

            return result;
        }

        return "";
    }

    JSONNumber &AsNumber()
    {
        AssertThrow(IsNumber());

        return m_inner.Get<JSONNumber>();
    }

    JSONNumber AsNumber() const
    {
        AssertThrow(IsNumber());

        return m_inner.Get<JSONNumber>();
    }

    JSONNumber ToNumber() const
    {
        if (IsNumber()) {
            return AsNumber();
        }

        if (IsNull()) {
            return 0;
        }

        if (IsUndefined()) {
            return JSONNumber(NAN);
        }

        if (IsBool()) {
            return AsBool() ? 1 : 0;
        }

        if (IsString()) {
            return StringUtil::Parse<JSONNumber>(AsString().Data(), 0.0);
        }

        return JSONNumber(0.0);
    }

    JSONBool &AsBool()
    {
        AssertThrow(IsBool());

        return m_inner.Get<JSONBool>();
    }

    JSONBool AsBool() const
    {
        AssertThrow(IsBool());

        return m_inner.Get<JSONBool>();
    }

    JSONBool ToBool() const
    {
        if (IsBool()) {
            return AsBool();
        }

        if (IsUndefined()) {
            return JSONBool(false);
        }

        if (IsNull()) {
            return JSONBool(false);
        }

        if (IsNumber()) {
            return AsNumber() != 0;
        }

        if (IsString()) {
            return !AsString().Empty() && AsString() != "false";
        }

        return JSONBool(false);
    }

    JSONArray &AsArray()
    {
        AssertThrow(IsArray());

        return *m_inner.Get<JSONArrayRef>();
    }

    const JSONArray &AsArray() const
    {
        AssertThrow(IsArray());

        return *m_inner.Get<JSONArrayRef>();
    }

    JSONArray ToArray() const
    {
        if (IsArray()) {
            return AsArray();
        }

        return JSONArray();
    }

    JSONObject &AsObject()
    {
        AssertThrow(IsObject());

        return *m_inner.Get<JSONObjectRef>();
    }

    const JSONObject &AsObject() const
    {
        AssertThrow(IsObject());

        return *m_inner.Get<JSONObjectRef>();
    }

    JSONObject ToObject() const
    {
        if (IsObject()) {
            return AsObject();
        }

        return JSONObject();
    }

    JSONSubscriptWrapper<JSONValue> operator[](UInt index)
    {
        return JSONSubscriptWrapper<JSONValue>(this)[index];
    }

    JSONSubscriptWrapper<const JSONValue> operator[](UInt index) const
    {
        return JSONSubscriptWrapper<const JSONValue>(this)[index];
    }

    JSONSubscriptWrapper<JSONValue> operator[](const JSONString &key)
    {
        return JSONSubscriptWrapper<JSONValue>(this)[key];
    }

    JSONSubscriptWrapper<const JSONValue> operator[](const JSONString &key) const
    {
        return JSONSubscriptWrapper<const JSONValue>(this)[key];
    }

private:
    InnerType m_inner;
};

struct ParseResult
{
    bool ok = true;
    String message;
    JSONValue value;
};

class JSON
{
public:
    static ParseResult Parse(const String &json_string);
};

}

}

#endif