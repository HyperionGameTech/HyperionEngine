/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/String.hpp>
#include <core/containers/Array.hpp>
#include <core/containers/HashMap.hpp>
#include <core/utilities/Variant.hpp>
#include <core/memory/RefCountedPtr.hpp>
#include <core/Defines.hpp>

#include <core/utilities/StringUtil.hpp>

namespace hyperion {

class BufferedReader;

namespace json {

class SourceFile;

template <class JSONValueType>
struct JSONSubscriptWrapper;

class JSONValue;
class JSONObject;

using JSONString = String;
using JSONNumber = double;
using JSONBool = bool;
using JSONArray = Array<JSONValue>;
using JSONArrayRef = RC<JSONArray>;
using JSONObjectRef = RC<JSONObject>;

struct JSONNull
{
};

struct JSONUndefined
{
};

template <class JSONValueType>
struct JSONSubscriptWrapper
{
};

template <>
struct HYP_API JSONSubscriptWrapper<const JSONValue>
{
    const JSONValue* value = nullptr;

    JSONSubscriptWrapper(const JSONValue* value)
        : value(value)
    {
    }

    JSONSubscriptWrapper(const JSONSubscriptWrapper& other)
        : value(other.value)
    {
    }

    JSONSubscriptWrapper& operator=(const JSONSubscriptWrapper& other)
    {
        if (std::addressof(*this) == std::addressof(other))
        {
            return *this;
        }

        value = other.value;

        return *this;
    }

    JSONSubscriptWrapper(JSONSubscriptWrapper&& other) noexcept
        : value(other.value)
    {
        other.value = nullptr;
    }

    JSONSubscriptWrapper& operator=(JSONSubscriptWrapper&& other) noexcept
    {
        if (std::addressof(*this) == std::addressof(other))
        {
            return *this;
        }

        value = other.value;

        other.value = nullptr;

        return *this;
    }

    ~JSONSubscriptWrapper() = default;

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return ToBool();
    }

    HYP_FORCE_INLINE const JSONValue& operator*() const
    {
        return Get();
    }

    const JSONValue& Get() const;

    bool IsString() const;
    bool IsNumber() const;
    bool IsBool() const;
    bool IsArray() const;
    bool IsObject() const;
    bool IsNull() const;
    bool IsUndefined() const;

    HYP_FORCE_INLINE bool IsNullOrUndefined() const
    {
        return IsNull() || IsUndefined();
    }

    const JSONString& AsString() const;
    JSONString ToString() const;

    JSONNumber AsNumber() const;
    JSONNumber ToNumber() const;

    JSONBool AsBool() const;
    JSONBool ToBool() const;

    const JSONArray& AsArray() const;
    const JSONArray& ToArray() const;

    const JSONObject& AsObject() const;
    const JSONObject& ToObject() const;

    JSONSubscriptWrapper<const JSONValue> operator[](uint32 index) const;
    JSONSubscriptWrapper<const JSONValue> operator[](UTF8StringView key) const;

    /*! \brief Get a value within the JSON object using a path (e.g. "key1.key2.key3").
     *  If the path does not exist, or the value is not an object, an undefined value is returned.
     *
     *  \param path The path to the value.
     *  \return A JSONSubscriptWrapper object.
     */
    JSONSubscriptWrapper<const JSONValue> Get(UTF8StringView path) const;

    HashCode GetHashCode() const;
};

template <>
struct HYP_API JSONSubscriptWrapper<JSONValue>
{
    JSONValue* value = nullptr;

    JSONSubscriptWrapper(JSONValue* value)
        : value(value)
    {
    }

    JSONSubscriptWrapper(const JSONSubscriptWrapper& other)
        : value(other.value)
    {
    }

    JSONSubscriptWrapper& operator=(const JSONSubscriptWrapper& other)
    {
        if (std::addressof(*this) == std::addressof(other))
        {
            return *this;
        }

        value = other.value;

        return *this;
    }

    JSONSubscriptWrapper(JSONSubscriptWrapper&& other) noexcept
        : value(other.value)
    {
        other.value = nullptr;
    }

    JSONSubscriptWrapper& operator=(JSONSubscriptWrapper&& other) noexcept
    {
        if (std::addressof(*this) == std::addressof(other))
        {
            return *this;
        }

        value = other.value;

        other.value = nullptr;

        return *this;
    }

    ~JSONSubscriptWrapper() = default;
    
    HYP_FORCE_INLINE operator JSONSubscriptWrapper<const JSONValue>() const
    {
        return JSONSubscriptWrapper<const JSONValue>(const_cast<JSONValue* const>(value));
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return ToBool();
    }

    HYP_FORCE_INLINE JSONValue& operator*() const
    {
        return Get();
    }

    JSONValue& Get() const;

    bool IsString() const;
    bool IsNumber() const;
    bool IsBool() const;
    bool IsArray() const;
    bool IsObject() const;
    bool IsNull() const;
    bool IsUndefined() const;

    JSONString& AsString() const;
    JSONString ToString() const;

    JSONNumber AsNumber() const;
    JSONNumber ToNumber() const;

    JSONBool AsBool() const;
    JSONBool ToBool() const;

    JSONArray& AsArray() const;
    const JSONArray& ToArray() const;

    JSONObject& AsObject() const;
    const JSONObject& ToObject() const;

    JSONSubscriptWrapper<JSONValue> operator[](uint32 index);
    JSONSubscriptWrapper<const JSONValue> operator[](uint32 index) const;
    JSONSubscriptWrapper<JSONValue> operator[](UTF8StringView key);
    JSONSubscriptWrapper<const JSONValue> operator[](UTF8StringView key) const;

    /*! \brief Get a value within the JSON object using a path (e.g. "key1.key2.key3").
     *  If the path does not exist, or the value is not an object, an undefined value is returned.
     *
     *  \param path The path to the value.
     *  \param createIntermediateObjects If true, intermediate objects are created between the path elements if they do not exist.
     *  \return A JSONSubscriptWrapper object.
     */
    JSONSubscriptWrapper<JSONValue> Get(UTF8StringView path, bool createIntermediateObjects = false);

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
    void Set(UTF8StringView path, const JSONValue& value);

    HashCode GetHashCode() const;
};

class HYP_API JSONValue
{
private:
    using InnerType = Variant<
        JSONString,
        JSONNumber,
        JSONBool,
        JSONArrayRef,
        JSONObjectRef,
        JSONNull,
        JSONUndefined>;

public:
    JSONValue()
        : m_inner(JSONUndefined {})
    {
    }

    JSONValue(const char* string)
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

    JSONValue(const JSONArray& array);
    JSONValue(JSONArray&& array);

    JSONValue(const JSONObject& object);
    JSONValue(JSONObject&& object);

    JSONValue(JSONNull)
        : m_inner(JSONNull())
    {
    }

    JSONValue(JSONUndefined)
        : m_inner(JSONUndefined())
    {
    }

    JSONValue(const JSONValue& other)
        : m_inner(other.m_inner)
    {
    }

    JSONValue& operator=(const JSONValue& other)
    {
        m_inner = other.m_inner;

        return *this;
    }

    JSONValue(JSONValue&& other) noexcept
        : m_inner(std::move(other.m_inner))
    {
    }

    JSONValue& operator=(JSONValue&& other) noexcept
    {
        m_inner = std::move(other.m_inner);

        return *this;
    }

    ~JSONValue() = default;

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return ToBool();
    }

    HYP_FORCE_INLINE bool IsString() const
    {
        return m_inner.Is<JSONString>();
    }

    HYP_FORCE_INLINE bool IsNumber() const
    {
        return m_inner.Is<JSONNumber>();
    }

    HYP_FORCE_INLINE bool IsBool() const
    {
        return m_inner.Is<JSONBool>();
    }

    HYP_FORCE_INLINE bool IsArray() const
    {
        return m_inner.Is<JSONArrayRef>();
    }

    HYP_FORCE_INLINE bool IsObject() const
    {
        return m_inner.Is<JSONObjectRef>();
    }

    HYP_FORCE_INLINE bool IsNull() const
    {
        return m_inner.Is<JSONNull>();
    }

    HYP_FORCE_INLINE bool IsUndefined() const
    {
        return m_inner.Is<JSONUndefined>();
    }

    HYP_FORCE_INLINE bool IsNullOrUndefined() const
    {
        return IsNull() || IsUndefined();
    }

    HYP_FORCE_INLINE JSONString& AsString()
    {
        HYP_CORE_ASSERT(IsString());

        return m_inner.GetUnchecked<JSONString>();
    }

    HYP_FORCE_INLINE const JSONString& AsString() const
    {
        HYP_CORE_ASSERT(IsString());

        return m_inner.GetUnchecked<JSONString>();
    }

    HYP_FORCE_INLINE JSONString ToString(bool representation = false) const
    {
        return ToString(representation, 0);
    }

    HYP_FORCE_INLINE JSONNumber AsNumber() const
    {
        HYP_CORE_ASSERT(IsNumber());

        return m_inner.GetUnchecked<JSONNumber>();
    }

    /*! \brief Convert the JSON value to a number. If the value is undefined, the default value is returned.
     *
     *  \param defaultValue The default value to return if the value is not a number. (Default: 0.0)
     *  \return The number value.
     */
    HYP_FORCE_INLINE JSONNumber ToNumber(JSONNumber defaultValue = 0.0) const
    {
        if (IsNumber())
        {
            return AsNumber();
        }

        if (IsNull())
        {
            return 0;
        }

        if (IsUndefined())
        {
            return defaultValue;
        }

        if (IsBool())
        {
            return AsBool() ? 1 : 0;
        }

        if (IsString())
        {
            return StringUtil::Parse<JSONNumber>(AsString().Data(), defaultValue);
        }

        return defaultValue;
    }

    HYP_FORCE_INLINE int8 ToInt8(int8 defaultValue = 0) const
    {
        return static_cast<int8>(ToNumber(JSONNumber(defaultValue)));
    }

    HYP_FORCE_INLINE int16 ToInt16(int16 defaultValue = 0) const
    {
        return static_cast<int16>(ToNumber(JSONNumber(defaultValue)));
    }

    HYP_FORCE_INLINE int32 ToInt32(int32 defaultValue = 0) const
    {
        return static_cast<int32>(ToNumber(JSONNumber(defaultValue)));
    }

    HYP_FORCE_INLINE int64 ToInt64(int64 defaultValue = 0) const
    {
        return static_cast<int64>(ToNumber(JSONNumber(defaultValue)));
    }

    HYP_FORCE_INLINE uint8 ToUInt8(uint8 defaultValue = 0) const
    {
        return static_cast<uint8>(ToNumber(JSONNumber(defaultValue)));
    }

    HYP_FORCE_INLINE uint16 ToUInt16(uint16 defaultValue = 0) const
    {
        return static_cast<uint16>(ToNumber(JSONNumber(defaultValue)));
    }

    HYP_FORCE_INLINE uint32 ToUInt32(uint32 defaultValue = 0) const
    {
        return static_cast<uint32>(ToNumber(JSONNumber(defaultValue)));
    }

    HYP_FORCE_INLINE uint64 ToUInt64(uint64 defaultValue = 0) const
    {
        return static_cast<uint64>(ToNumber(JSONNumber(defaultValue)));
    }

    HYP_FORCE_INLINE float ToFloat(float defaultValue = 0.0f) const
    {
        return static_cast<float>(ToNumber(JSONNumber(defaultValue)));
    }

    HYP_FORCE_INLINE double ToDouble(double defaultValue = 0.0) const
    {
        return ToNumber(JSONNumber(defaultValue));
    }

    HYP_FORCE_INLINE JSONBool AsBool() const
    {
        HYP_CORE_ASSERT(IsBool());

        return m_inner.GetUnchecked<JSONBool>();
    }

    /*! \brief Convert the JSON value to a boolean. If the value is undefined, the default value is returned.
     *
     *  \param defaultValue The default value to return if the value is not a boolean. (Default: false)
     *  \return The boolean value.
     */
    HYP_FORCE_INLINE JSONBool ToBool(JSONBool defaultValue = false) const
    {
        if (IsBool())
        {
            return AsBool();
        }

        if (IsUndefined())
        {
            return defaultValue;
        }

        if (IsNull())
        {
            return JSONBool(false);
        }

        if (IsNumber())
        {
            return AsNumber() != 0;
        }

        if (IsString())
        {
            return !AsString().Empty();
        }

        if (IsObject())
        {
            return JSONBool(true);
        }

        if (IsArray())
        {
            return JSONBool(true);
        }

        return defaultValue;
    }

    HYP_FORCE_INLINE JSONArray& AsArray()
    {
        HYP_CORE_ASSERT(IsArray());

        return *m_inner.GetUnchecked<JSONArrayRef>();
    }

    HYP_FORCE_INLINE const JSONArray& AsArray() const
    {
        HYP_CORE_ASSERT(IsArray());

        return *m_inner.GetUnchecked<JSONArrayRef>();
    }

    HYP_FORCE_INLINE JSONArray ToArray() const
    {
        if (IsArray())
        {
            return AsArray();
        }

        if (IsUndefined())
        {
            return JSONArray();
        }

        JSONArray arrayValue;
        arrayValue.PushBack(*this);
        return arrayValue;
    }

    HYP_FORCE_INLINE JSONObject& AsObject()
    {
        HYP_CORE_ASSERT(IsObject());

        return *m_inner.GetUnchecked<JSONObjectRef>();
    }

    HYP_FORCE_INLINE const JSONObject& AsObject() const
    {
        HYP_CORE_ASSERT(IsObject());

        return *m_inner.GetUnchecked<JSONObjectRef>();
    }

    const JSONObject& ToObject() const;

    HYP_FORCE_INLINE JSONSubscriptWrapper<JSONValue> operator[](uint32 index)
    {
        return JSONSubscriptWrapper<JSONValue>(this)[index];
    }

    HYP_FORCE_INLINE JSONSubscriptWrapper<const JSONValue> operator[](uint32 index) const
    {
        return JSONSubscriptWrapper<const JSONValue>(this)[index];
    }

    HYP_FORCE_INLINE JSONSubscriptWrapper<JSONValue> operator[](UTF8StringView key)
    {
        return JSONSubscriptWrapper<JSONValue>(this)[key];
    }

    HYP_FORCE_INLINE JSONSubscriptWrapper<const JSONValue> operator[](UTF8StringView key) const
    {
        return JSONSubscriptWrapper<const JSONValue>(this)[key];
    }

    HYP_FORCE_INLINE JSONSubscriptWrapper<JSONValue> Get(UTF8StringView path, bool createIntermediateObjects = false)
    {
        return JSONSubscriptWrapper<JSONValue>(this).Get(path, createIntermediateObjects);
    }

    HYP_FORCE_INLINE JSONSubscriptWrapper<const JSONValue> Get(UTF8StringView path) const
    {
        return JSONSubscriptWrapper<const JSONValue>(this).Get(path);
    }

    HYP_FORCE_INLINE void Set(UTF8StringView path, const JSONValue& value)
    {
        JSONSubscriptWrapper<JSONValue>(this).Set(path, value);
    }

    HashCode GetHashCode() const;

private:
    JSONString ToString(bool representation, uint32 depth) const;
    JSONString ToString_Internal(bool representation, uint32 depth) const;

    InnerType m_inner;
};

class JSONObject : public HashMap<JSONString, JSONValue>
{
public:
    using Base = HashMap<JSONString, JSONValue>;

    JSONObject() = default;

    JSONObject(std::initializer_list<KeyValuePair<JSONString, JSONValue>> initializerList)
        : Base(initializerList)
    {
    }

    JSONObject(const JSONObject& other) = default;
    JSONObject& operator=(const JSONObject& other) = default;

    JSONObject(JSONObject&& other) noexcept = default;
    JSONObject& operator=(JSONObject&& other) noexcept = default;

    ~JSONObject() = default;

    /*! \brief Merge another JSONObject into this one.
     *  If a key exists in both objects, the value from the other object is used.
     *  If the value is an object, it is replaced with the other object's value.
     *
     *  \param other The other JSONObject to merge.
     *  \return A reference to this JSONObject.
     */
    template <class OtherContainerType>
    JSONObject& Merge(OtherContainerType&& other)
    {
        Base::Merge(std::forward<OtherContainerType>(other));

        return *this;
    }

    /*! \brief Merge another JSONObject into this one, recursively merging objects.
     *  If a key exists in both objects and the value is an object, the values are merged.
     *  Otherwise, the value from the other object is used.
     *
     *  \param other The other JSONObject to merge.
     *  \return A reference to this JSONObject.
     */
    JSONObject& MergeDeep(const JSONObject& other)
    {
        if (this == std::addressof(other))
        {
            return *this;
        }

        for (const auto& kv : other)
        {
            const JSONString& key = kv.first;
            const JSONValue& value = kv.second;

            if (value.IsObject())
            {
                auto it = Find(key);

                if (it != End())
                {
                    if (it->second.IsObject())
                    {
                        it->second.AsObject().MergeDeep(value.AsObject());
                    }
                    else
                    {
                        it->second = value;
                    }

                    continue;
                }
            }

            Set(key, value);
        }

        return *this;
    }
};

struct ParseResult
{
    bool ok = true;
    String message;
    JSONValue value;
};

class HYP_API JSON
{
public:
    static const JSONValue& Undefined();
    static const JSONValue& Null();
    static const JSONValue& EmptyObject();
    static const JSONValue& EmptyArray();
    static const JSONValue& EmptyString();
    static const JSONValue& True();
    static const JSONValue& False();

    static ParseResult Parse(const String& jsonString);
    static ParseResult Parse(BufferedReader& reader);
    static ParseResult Parse(const SourceFile& sourceFile);
};

} // namespace json
} // namespace hyperion
