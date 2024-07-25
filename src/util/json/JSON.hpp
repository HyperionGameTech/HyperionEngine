/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_JSON_HPP
#define HYPERION_JSON_HPP

#include <core/containers/String.hpp>
#include <core/containers/Array.hpp>
#include <core/containers/HashMap.hpp>
#include <core/utilities/Variant.hpp>
#include <core/memory/RefCountedPtr.hpp>
#include <core/Defines.hpp>

#include <util/StringUtil.hpp>

namespace hyperion {
namespace json {

template <class JSONValueType>
struct JSONSubscriptWrapper;

class JSONValue;

using JSONString = String;
using JSONNumber = double;
using JSONBool = bool;
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
struct HYP_API JSONSubscriptWrapper<const JSONValue>
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

    HYP_FORCE_INLINE explicit operator bool() const
        { return ToBool(); }

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

    JSONSubscriptWrapper<const JSONValue> operator[](uint index) const;
    JSONSubscriptWrapper<const JSONValue> operator[](UTF8StringView key) const;

    /*! \brief Get a value within the JSON object using a path (e.g. "key1.key2.key3").
     *  If the path does not exist, or the value is not an object, an undefined value is returned.
     *
     *  \param path The path to the value.
     *  \return A JSONSubscriptWrapper object.
     */
    JSONSubscriptWrapper<const JSONValue> Get(UTF8StringView path) const;
};

template <>
struct HYP_API JSONSubscriptWrapper<JSONValue>
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

    HYP_FORCE_INLINE explicit operator bool() const
        { return ToBool(); }

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

    JSONSubscriptWrapper<JSONValue> operator[](uint index);
    JSONSubscriptWrapper<const JSONValue> operator[](uint index) const;
    JSONSubscriptWrapper<JSONValue> operator[](UTF8StringView key);
    JSONSubscriptWrapper<const JSONValue> operator[](UTF8StringView key) const;

    /*! \brief Get a value within the JSON object using a path (e.g. "key1.key2.key3").
     *  If the path does not exist, or the value is not an object, an undefined value is returned.
     *
     *  \param path The path to the value.
     *  \return A JSONSubscriptWrapper object.
     */
    JSONSubscriptWrapper<JSONValue> Get(UTF8StringView path);

    /*! \brief Get a value within the JSON object using a path (e.g. "key1.key2.key3").
     *  If the path does not exist, or the value is not an object, an undefined value is returned.
     *
     *  \param path The path to the value.
     *  \return A JSONSubscriptWrapper object.
     */
    JSONSubscriptWrapper<const JSONValue> Get(UTF8StringView path) const;

    /*! \brief Set a value within the JSON object using a path.
     *  (e.g. "key1.key2.key3"). If the value is not an object, the value is not set. If the path does not exist, it is created.
     *
     *  \param path The path to the value.
     *  \param value The value to set.
     */
    void Set(UTF8StringView path, const JSONValue &value);
};

class HYP_API JSONValue
{
private:
    using InnerType = Variant<JSONString, JSONNumber, JSONBool, JSONArrayRef, JSONObjectRef, JSONNull, JSONUndefined>;

public:
    JSONValue()
        : m_inner(JSONUndefined { })
    {
    }

    JSONValue(const char *string)
        : JSONValue(JSONString(string))
    {
    }

    JSONValue(JSONString string)
        : m_inner(std::move(string))
    {
    }

    JSONValue(ANSIString string)
        : m_inner(JSONString(std::move(string)))
    {
    }

    JSONValue(UTF16String string)
        : m_inner(JSONString(std::move(string)))
    {
    }

    JSONValue(UTF32String string)
        : m_inner(JSONString(std::move(string)))
    {
    }

    JSONValue(WideString string)
        : m_inner(JSONString(std::move(string)))
    {
    }

    JSONValue(ANSIStringView string)
        : JSONValue(JSONString(string))
    {
    }

    JSONValue(UTF8StringView string)
        : JSONValue(JSONString(string))
    {
    }

    JSONValue(UTF16StringView string)
        : JSONValue(JSONString(string))
    {
    }

    JSONValue(UTF32StringView string)
        : JSONValue(JSONString(string))
    {
    }

    JSONValue(WideStringView string)
        : JSONValue(JSONString(string))
    {
    }

    JSONValue(JSONNumber number)
        : m_inner(number)
    {
    }

    JSONValue(uint8 number)
        : m_inner(JSONNumber(number))
    {
    }

    JSONValue(uint16 number)
        : m_inner(JSONNumber(number))
    {
    }

    JSONValue(uint32 number)
        : m_inner(JSONNumber(number))
    {
    }

    JSONValue(uint64 number)
        : m_inner(JSONNumber(number))
    {
    }

    JSONValue(int8 number)
        : m_inner(JSONNumber(number))
    {
    }

    JSONValue(int16 number)
        : m_inner(JSONNumber(number))
    {
    }

    JSONValue(int32 number)
        : m_inner(JSONNumber(number))
    {
    }

    JSONValue(int64 number)
        : m_inner(JSONNumber(number))
    {
    }

    JSONValue(float number)
        : m_inner(JSONNumber(number))
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

    HYP_FORCE_INLINE explicit operator bool() const
        { return ToBool(); }

    HYP_FORCE_INLINE bool IsString() const
        { return m_inner.Is<JSONString>(); }

    HYP_FORCE_INLINE bool IsNumber() const
        { return m_inner.Is<JSONNumber>(); }

    HYP_FORCE_INLINE bool IsBool() const
        { return m_inner.Is<JSONBool>(); }

    HYP_FORCE_INLINE bool IsArray() const
        { return m_inner.Is<JSONArrayRef>(); }

    HYP_FORCE_INLINE bool IsObject() const
        { return m_inner.Is<JSONObjectRef>(); }

    HYP_FORCE_INLINE bool IsNull() const
        { return m_inner.Is<JSONNull>(); }

    HYP_FORCE_INLINE bool IsUndefined() const
        { return m_inner.Is<JSONUndefined>(); }

    HYP_FORCE_INLINE JSONString &AsString()
    {
        AssertThrow(IsString());

        return m_inner.Get<JSONString>();
    }

    HYP_FORCE_INLINE const JSONString &AsString() const
    {
        AssertThrow(IsString());

        return m_inner.Get<JSONString>();
    }

    HYP_FORCE_INLINE JSONString ToString(bool representation = false) const
        { return ToString(representation, 0); }

    HYP_FORCE_INLINE JSONNumber &AsNumber()
    {
        AssertThrow(IsNumber());

        return m_inner.Get<JSONNumber>();
    }

    HYP_FORCE_INLINE JSONNumber AsNumber() const
    {
        AssertThrow(IsNumber());

        return m_inner.Get<JSONNumber>();
    }

    HYP_FORCE_INLINE JSONNumber ToNumber() const
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

    HYP_FORCE_INLINE int32 ToInt8() const
        { return static_cast<int8>(ToNumber()); }

    HYP_FORCE_INLINE int32 ToInt16() const
        { return static_cast<int16>(ToNumber()); }

    HYP_FORCE_INLINE int32 ToInt32() const
        { return static_cast<int32>(ToNumber()); }

    HYP_FORCE_INLINE int64 ToInt64() const
        { return static_cast<int64>(ToNumber()); }

    HYP_FORCE_INLINE uint32 ToUInt8() const
        { return static_cast<uint8>(ToNumber()); }

    HYP_FORCE_INLINE uint32 ToUInt16() const
        { return static_cast<uint16>(ToNumber()); }

    HYP_FORCE_INLINE uint32 ToUInt32() const
        { return static_cast<uint32>(ToNumber()); }

    HYP_FORCE_INLINE uint64 ToUInt64() const
        { return static_cast<uint64>(ToNumber()); }

    HYP_FORCE_INLINE float ToFloat() const
        { return static_cast<float>(ToNumber()); }

    HYP_FORCE_INLINE double ToDouble() const
        { return ToNumber(); }
    
    HYP_FORCE_INLINE JSONBool &AsBool()
    {
        AssertThrow(IsBool());

        return m_inner.Get<JSONBool>();
    }

    HYP_FORCE_INLINE JSONBool AsBool() const
    {
        AssertThrow(IsBool());

        return m_inner.Get<JSONBool>();
    }

    HYP_FORCE_INLINE JSONBool ToBool() const
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
            return !AsString().Empty();
        }

        if (IsObject()) {
            return JSONBool(true);
        }

        if (IsArray()) {
            return JSONBool(true);
        }

        return JSONBool(false);
    }

    HYP_FORCE_INLINE JSONArray &AsArray()
    {
        AssertThrow(IsArray());

        return *m_inner.Get<JSONArrayRef>();
    }

    HYP_FORCE_INLINE const JSONArray &AsArray() const
    {
        AssertThrow(IsArray());

        return *m_inner.Get<JSONArrayRef>();
    }

    HYP_FORCE_INLINE JSONArray ToArray() const
    {
        if (IsArray()) {
            return AsArray();
        }

        return JSONArray();
    }

    HYP_FORCE_INLINE JSONObject &AsObject()
    {
        AssertThrow(IsObject());

        return *m_inner.Get<JSONObjectRef>();
    }

    HYP_FORCE_INLINE const JSONObject &AsObject() const
    {
        AssertThrow(IsObject());

        return *m_inner.Get<JSONObjectRef>();
    }

    HYP_FORCE_INLINE JSONObject ToObject() const
    {
        if (IsObject()) {
            return AsObject();
        }

        return JSONObject();
    }

    HYP_FORCE_INLINE JSONSubscriptWrapper<JSONValue> operator[](uint index)
        { return JSONSubscriptWrapper<JSONValue>(this)[index]; }

    HYP_FORCE_INLINE JSONSubscriptWrapper<const JSONValue> operator[](uint index) const
        { return JSONSubscriptWrapper<const JSONValue>(this)[index]; }

    HYP_FORCE_INLINE JSONSubscriptWrapper<JSONValue> operator[](UTF8StringView key)
        { return JSONSubscriptWrapper<JSONValue>(this)[key]; }

    HYP_FORCE_INLINE JSONSubscriptWrapper<const JSONValue> operator[](UTF8StringView key) const
        { return JSONSubscriptWrapper<const JSONValue>(this)[key]; }

    HYP_FORCE_INLINE JSONSubscriptWrapper<JSONValue> Get(UTF8StringView path)
        { return JSONSubscriptWrapper<JSONValue>(this).Get(path); }

    HYP_FORCE_INLINE JSONSubscriptWrapper<const JSONValue> Get(UTF8StringView path) const
        { return JSONSubscriptWrapper<const JSONValue>(this).Get(path); }

    HYP_FORCE_INLINE void Set(UTF8StringView path, const JSONValue &value)
        { JSONSubscriptWrapper<JSONValue>(this).Set(path, value); }

private:
    JSONString ToString(bool representation, uint32 depth) const;
    JSONString ToString_Internal(bool representation, uint32 depth) const;

    InnerType   m_inner;
};

struct ParseResult
{
    bool        ok = true;
    String      message;
    JSONValue   value;
};

class HYP_API JSON
{
public:
    static ParseResult Parse(const String &json_string);
};

} // namespace json
} // namespace hyperion

#endif