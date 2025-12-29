/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/String.hpp>
#include <core/containers/Array.hpp>
#include <core/containers/ArrayMap.hpp>

#include <core/utilities/Variant.hpp>
#include <core/utilities/StringUtil.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/Defines.hpp>

namespace Hyperion {

class BufferedReader;

namespace Json {

class SourceFile;

template <class JSONValueType>
struct JSONSubscriptWrapper;

class Value;
class JSObject;

using JSString = String;
using JSNumber = double;
using JSBoolean = bool;
using JSArray = Array<Value, DynamicAllocator>;
using JSArrayRef = RC<JSArray>;
using JSObjectRef = RC<JSObject>;

struct JSNull
{
};

struct JSUndefined
{
};

template <class JSONValueType>
struct JSONSubscriptWrapper
{
};

template <>
struct HYP_API JSONSubscriptWrapper<const Value>
{
    const Value* value = nullptr;

    JSONSubscriptWrapper(const Value* value)
        : value(value)
    {
    }

    JSONSubscriptWrapper(const JSONSubscriptWrapper& other)
        : value(other.value)
    {
    }

    JSONSubscriptWrapper& operator=(const JSONSubscriptWrapper& other)
    {
        if (this == &other)
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
        if (this == &other)
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

    HYP_FORCE_INLINE const Value& operator*() const
    {
        return Get();
    }

    const Value& Get() const;

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

    const JSString& AsString() const;
    JSString ToString() const;

    JSNumber AsNumber() const;
    JSNumber ToNumber() const;

    JSBoolean AsBool() const;
    JSBoolean ToBool() const;

    const JSArray& AsArray() const;
    const JSArray& ToArray() const;

    const JSObject& AsObject() const;
    const JSObject& ToObject() const;

    JSONSubscriptWrapper<const Value> operator[](uint32 index) const;
    JSONSubscriptWrapper<const Value> operator[](UTF8StringView key) const;

    /*! \brief Get a value within the JSON object using a path (e.g. "key1.key2.key3").
     *  If the path does not exist, or the value is not an object, an undefined value is returned.
     *
     *  \param path The path to the value.
     *  \return A JSONSubscriptWrapper object.
     */
    JSONSubscriptWrapper<const Value> Get(UTF8StringView path) const;

    HashCode GetHashCode() const;
};

template <>
struct HYP_API JSONSubscriptWrapper<Value>
{
    Value* value = nullptr;

    JSONSubscriptWrapper(Value* value)
        : value(value)
    {
    }

    JSONSubscriptWrapper(const JSONSubscriptWrapper& other)
        : value(other.value)
    {
    }

    JSONSubscriptWrapper& operator=(const JSONSubscriptWrapper& other)
    {
        if (this == &other)
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
        if (this == &other)
        {
            return *this;
        }

        value = other.value;

        other.value = nullptr;

        return *this;
    }

    ~JSONSubscriptWrapper() = default;

    HYP_FORCE_INLINE operator JSONSubscriptWrapper<const Value>() const
    {
        return JSONSubscriptWrapper<const Value>(const_cast<Value* const>(value));
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return ToBool();
    }

    HYP_FORCE_INLINE Value& operator*() const
    {
        return Get();
    }

    Value& Get() const;

    bool IsString() const;
    bool IsNumber() const;
    bool IsBool() const;
    bool IsArray() const;
    bool IsObject() const;
    bool IsNull() const;
    bool IsUndefined() const;

    JSString& AsString() const;
    JSString ToString() const;

    JSNumber AsNumber() const;
    JSNumber ToNumber() const;

    JSBoolean AsBool() const;
    JSBoolean ToBool() const;

    JSArray& AsArray() const;
    const JSArray& ToArray() const;

    JSObject& AsObject() const;
    const JSObject& ToObject() const;

    JSONSubscriptWrapper<Value> operator[](uint32 index);
    JSONSubscriptWrapper<const Value> operator[](uint32 index) const;
    JSONSubscriptWrapper<Value> operator[](UTF8StringView key);
    JSONSubscriptWrapper<const Value> operator[](UTF8StringView key) const;

    /*! \brief Get a value within the JSON object using a path (e.g. "key1.key2.key3").
     *  If the path does not exist, or the value is not an object, an undefined value is returned.
     *
     *  \param path The path to the value.
     *  \param createIntermediateObjects If true, intermediate objects are created between the path elements if they do not exist.
     *  \return A JSONSubscriptWrapper object.
     */
    JSONSubscriptWrapper<Value> Get(UTF8StringView path, bool createIntermediateObjects = false);

    /*! \brief Get a value within the JSON object using a path (e.g. "key1.key2.key3").
     *  If the path does not exist, or the value is not an object, an undefined value is returned.
     *
     *  \param path The path to the value.
     *  \return A JSONSubscriptWrapper object.
     */
    JSONSubscriptWrapper<const Value> Get(UTF8StringView path) const;

    /*! \brief Set a value within the JSON object using a path.
     *  (e.g. "key1.key2.key3"). If the value is not an object, the value is not set. If the path does not exist, it is created.
     *
     *  \param path The path to the value.
     *  \param value The value to set.
     */
    void Set(UTF8StringView path, const Value& value);

    HashCode GetHashCode() const;
};

class HYP_API Value
{
private:
    using InnerType = Variant<
        JSString,
        JSNumber,
        JSBoolean,
        JSArrayRef,
        JSObjectRef,
        JSNull,
        JSUndefined>;

public:
    Value()
        : m_inner(JSUndefined {})
    {
    }

    Value(String string)
        : m_inner(JSString(std::move(string)))
    {
    }

    Value(ANSIString string)
        : m_inner(JSString(std::move(string)))
    {
    }

    Value(UTF16String string)
        : m_inner(JSString(std::move(string)))
    {
    }

    Value(UTF32String string)
        : m_inner(JSString(std::move(string)))
    {
    }

    Value(WideString string)
        : m_inner(JSString(std::move(string)))
    {
    }

    Value(ANSIStringView string)
        : Value(JSString(string))
    {
    }

    Value(UTF8StringView string)
        : Value(JSString(string))
    {
    }

    Value(UTF16StringView string)
        : Value(JSString(string))
    {
    }

    Value(UTF32StringView string)
        : Value(JSString(string))
    {
    }

    Value(WideStringView string)
        : Value(JSString(string))
    {
    }

    Value(JSNumber number)
        : m_inner(number)
    {
    }

    Value(uint8 number)
        : m_inner(JSNumber(number))
    {
    }

    Value(uint16 number)
        : m_inner(JSNumber(number))
    {
    }

    Value(uint32 number)
        : m_inner(JSNumber(number))
    {
    }

    Value(uint64 number)
        : m_inner(JSNumber(number))
    {
    }

    Value(int8 number)
        : m_inner(JSNumber(number))
    {
    }

    Value(int16 number)
        : m_inner(JSNumber(number))
    {
    }

    Value(int32 number)
        : m_inner(JSNumber(number))
    {
    }

    Value(int64 number)
        : m_inner(JSNumber(number))
    {
    }

    Value(float number)
        : m_inner(JSNumber(number))
    {
    }

    Value(JSBoolean boolean)
        : m_inner(boolean)
    {
    }

    Value(const JSArray& array);
    Value(JSArray&& array);

    Value(const JSObject& object);
    Value(JSObject&& object);

    Value(JSNull)
        : m_inner(JSNull())
    {
    }

    Value(JSUndefined)
        : m_inner(JSUndefined())
    {
    }

    Value(const Value& other)
        : m_inner(other.m_inner)
    {
    }

    Value& operator=(const Value& other)
    {
        m_inner = other.m_inner;

        return *this;
    }

    Value(Value&& other) noexcept
        : m_inner(std::move(other.m_inner))
    {
    }

    Value& operator=(Value&& other) noexcept
    {
        m_inner = std::move(other.m_inner);

        return *this;
    }

    ~Value() = default;

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return ToBool();
    }

    HYP_FORCE_INLINE bool IsString() const
    {
        return m_inner.Is<JSString>();
    }

    HYP_FORCE_INLINE bool IsNumber() const
    {
        return m_inner.Is<JSNumber>();
    }

    HYP_FORCE_INLINE bool IsBool() const
    {
        return m_inner.Is<JSBoolean>();
    }

    HYP_FORCE_INLINE bool IsArray() const
    {
        return m_inner.Is<JSArrayRef>();
    }

    HYP_FORCE_INLINE bool IsObject() const
    {
        return m_inner.Is<JSObjectRef>();
    }

    HYP_FORCE_INLINE bool IsNull() const
    {
        return m_inner.Is<JSNull>();
    }

    HYP_FORCE_INLINE bool IsUndefined() const
    {
        return m_inner.Is<JSUndefined>();
    }

    HYP_FORCE_INLINE bool IsNullOrUndefined() const
    {
        return IsNull() || IsUndefined();
    }

    HYP_FORCE_INLINE JSString& AsString()
    {
        HYP_CORE_ASSERT(IsString());

        return m_inner.GetUnchecked<JSString>();
    }

    HYP_FORCE_INLINE const JSString& AsString() const
    {
        HYP_CORE_ASSERT(IsString());

        return m_inner.GetUnchecked<JSString>();
    }

    HYP_FORCE_INLINE JSString ToString(bool representation = false) const
    {
        return ToString(representation, 0);
    }

    HYP_FORCE_INLINE JSNumber AsNumber() const
    {
        HYP_CORE_ASSERT(IsNumber());

        return m_inner.GetUnchecked<JSNumber>();
    }

    /*! \brief Convert the JSON value to a number. If the value is undefined, the default value is returned.
     *
     *  \param defaultValue The default value to return if the value is not a number. (Default: 0.0)
     *  \return The number value.
     */
    HYP_FORCE_INLINE JSNumber ToNumber(JSNumber defaultValue = 0.0) const
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
            return StringUtil::Parse<JSNumber>(String(AsString()).Data(), defaultValue);
        }

        return defaultValue;
    }

    HYP_FORCE_INLINE int8 ToInt8(int8 defaultValue = 0) const
    {
        return static_cast<int8>(ToNumber(JSNumber(defaultValue)));
    }

    HYP_FORCE_INLINE int16 ToInt16(int16 defaultValue = 0) const
    {
        return static_cast<int16>(ToNumber(JSNumber(defaultValue)));
    }

    HYP_FORCE_INLINE int32 ToInt32(int32 defaultValue = 0) const
    {
        return static_cast<int32>(ToNumber(JSNumber(defaultValue)));
    }

    HYP_FORCE_INLINE int64 ToInt64(int64 defaultValue = 0) const
    {
        return static_cast<int64>(ToNumber(JSNumber(defaultValue)));
    }

    HYP_FORCE_INLINE uint8 ToUInt8(uint8 defaultValue = 0) const
    {
        return static_cast<uint8>(ToNumber(JSNumber(defaultValue)));
    }

    HYP_FORCE_INLINE uint16 ToUInt16(uint16 defaultValue = 0) const
    {
        return static_cast<uint16>(ToNumber(JSNumber(defaultValue)));
    }

    HYP_FORCE_INLINE uint32 ToUInt32(uint32 defaultValue = 0) const
    {
        return static_cast<uint32>(ToNumber(JSNumber(defaultValue)));
    }

    HYP_FORCE_INLINE uint64 ToUInt64(uint64 defaultValue = 0) const
    {
        return static_cast<uint64>(ToNumber(JSNumber(defaultValue)));
    }

    HYP_FORCE_INLINE float ToFloat(float defaultValue = 0.0f) const
    {
        return static_cast<float>(ToNumber(JSNumber(defaultValue)));
    }

    HYP_FORCE_INLINE double ToDouble(double defaultValue = 0.0) const
    {
        return ToNumber(JSNumber(defaultValue));
    }

    HYP_FORCE_INLINE JSBoolean AsBool() const
    {
        HYP_CORE_ASSERT(IsBool());

        return m_inner.GetUnchecked<JSBoolean>();
    }

    /*! \brief Convert the JSON value to a boolean. If the value is undefined, the default value is returned.
     *
     *  \param defaultValue The default value to return if the value is not a boolean. (Default: false)
     *  \return The boolean value.
     */
    HYP_FORCE_INLINE JSBoolean ToBool(JSBoolean defaultValue = false) const
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
            return JSBoolean(false);
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
            return JSBoolean(true);
        }

        if (IsArray())
        {
            return JSBoolean(true);
        }

        return defaultValue;
    }

    HYP_FORCE_INLINE JSArray& AsArray()
    {
        HYP_CORE_ASSERT(IsArray());

        return *m_inner.GetUnchecked<JSArrayRef>();
    }

    HYP_FORCE_INLINE const JSArray& AsArray() const
    {
        HYP_CORE_ASSERT(IsArray());

        return *m_inner.GetUnchecked<JSArrayRef>();
    }

    HYP_FORCE_INLINE JSArray ToArray() const
    {
        if (IsArray())
        {
            return AsArray();
        }

        if (IsUndefined())
        {
            return JSArray();
        }

        JSArray arrayValue;
        arrayValue.PushBack(*this);
        return arrayValue;
    }

    HYP_FORCE_INLINE JSObject& AsObject()
    {
        HYP_CORE_ASSERT(IsObject());

        return *m_inner.GetUnchecked<JSObjectRef>();
    }

    HYP_FORCE_INLINE const JSObject& AsObject() const
    {
        HYP_CORE_ASSERT(IsObject());

        return *m_inner.GetUnchecked<JSObjectRef>();
    }

    const JSObject& ToObject() const;

    HYP_FORCE_INLINE JSONSubscriptWrapper<Value> operator[](uint32 index)
    {
        return JSONSubscriptWrapper<Value>(this)[index];
    }

    HYP_FORCE_INLINE JSONSubscriptWrapper<const Value> operator[](uint32 index) const
    {
        return JSONSubscriptWrapper<const Value>(this)[index];
    }

    HYP_FORCE_INLINE JSONSubscriptWrapper<Value> operator[](UTF8StringView key)
    {
        return JSONSubscriptWrapper<Value>(this)[key];
    }

    HYP_FORCE_INLINE JSONSubscriptWrapper<const Value> operator[](UTF8StringView key) const
    {
        return JSONSubscriptWrapper<const Value>(this)[key];
    }

    HYP_FORCE_INLINE JSONSubscriptWrapper<Value> Get(UTF8StringView path, bool createIntermediateObjects = false)
    {
        return JSONSubscriptWrapper<Value>(this).Get(path, createIntermediateObjects);
    }

    HYP_FORCE_INLINE JSONSubscriptWrapper<const Value> Get(UTF8StringView path) const
    {
        return JSONSubscriptWrapper<const Value>(this).Get(path);
    }

    HYP_FORCE_INLINE void Set(UTF8StringView path, const Value& value)
    {
        JSONSubscriptWrapper<Value>(this).Set(path, value);
    }

    HashCode GetHashCode() const;

private:
    JSString ToString(bool representation, uint32 depth) const;
    JSString ToString_Internal(bool representation, uint32 depth) const;

    InnerType m_inner;
};

class JSObject final : public ArrayMap<JSString, Value>
{
public:
    using Base = ArrayMap<JSString, Value>;

    JSObject() = default;

    JSObject(std::initializer_list<KeyValuePair<JSString, Value>> initializerList)
        : Base(initializerList)
    {
    }

    JSObject(const JSObject& other) = default;
    JSObject& operator=(const JSObject& other) = default;

    JSObject(JSObject&& other) noexcept = default;
    JSObject& operator=(JSObject&& other) noexcept = default;

    ~JSObject() = default;

    /*! \brief Merge another JSObject into this one.
     *  If a key exists in both objects, the value from the other object is used.
     *  If the value is an object, it is replaced with the other object's value.
     *
     *  \param other The other JSObject to merge.
     *  \return A reference to this JSObject.
     */
    template <class OtherContainerType>
    JSObject& Merge(OtherContainerType&& other)
    {
        Base::Merge(std::forward<OtherContainerType>(other));

        return *this;
    }

    /*! \brief Merge another JSObject into this one, recursively merging objects.
     *  If a key exists in both objects and the value is an object, the values are merged.
     *  Otherwise, the value from the other object is used.
     *
     *  \param other The other JSObject to merge.
     *  \return A reference to this JSObject.
     */
    JSObject& MergeDeep(const JSObject& other)
    {
        if (this == &other)
        {
            return *this;
        }

        for (const auto& kv : other)
        {
            const JSString& key = kv.first;
            const Value& value = kv.second;

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
    Value value;
};

HYP_API const Value& Undefined();
HYP_API const Value& Null();
HYP_API const Value& EmptyObject();
HYP_API const Value& EmptyArray();
HYP_API const Value& EmptyString();
HYP_API const Value& True();
HYP_API const Value& False();

HYP_API ParseResult Parse(const String& jsonString);
HYP_API ParseResult Parse(BufferedReader& reader);
HYP_API ParseResult Parse(const SourceFile& sourceFile);

} // namespace Json
} // namespace Hyperion
