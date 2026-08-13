/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Containers/String.hpp>
#include <Core/Containers/Array.hpp>
#include <Core/Containers/ArrayMap.hpp>

#include <Core/Utilities/Variant.hpp>
#include <Core/Utilities/StringUtil.hpp>

#include <Core/Memory/SharedPtr.hpp>

#include <Core/Defines.hpp>

namespace Hyperion {
class BufferedReader;
class ByteReader;
} // namespace Hyperion

namespace Hyperion::DataProcessing::JSON {

template <class JSONValueType>
struct JSONSubscriptWrapper;

class Value;
class Object;

using JString = String;
using Number = double;
using JArray = Array<Value, DynamicAllocator>;
using JArrayRef = SharedPtr<JArray>;
using ObjectRef = SharedPtr<Object>;

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
struct CORE_API JSONSubscriptWrapper<const Value>
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

    const JString& AsString() const;
    JString ToString() const;

    Number AsNumber() const;
    Number ToNumber() const;

    bool AsBool() const;
    bool ToBool() const;

    const JArray& AsArray() const;
    const JArray& ToArray() const;

    const Object& AsObject() const;
    const Object& ToObject() const;

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
struct CORE_API JSONSubscriptWrapper<Value>
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

    JString& AsString() const;
    JString ToString() const;

    Number AsNumber() const;
    Number ToNumber() const;

    bool AsBool() const;
    bool ToBool() const;

    JArray& AsArray() const;
    const JArray& ToArray() const;

    Object& AsObject() const;
    const Object& ToObject() const;

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

class CORE_API Value
{
private:
    using InnerType = Variant<
        JString,
        Number,
        bool,
        JArrayRef,
        ObjectRef,
        JSNull,
        JSUndefined>;

public:
    Value()
        : m_inner(JSUndefined {})
    {
    }

    /// UTF-8

    Value(String str)
        : m_inner(JString(std::move(str)))
    {
    }

    Value(const UTF8StringView& str)
        : Value(JString(str))
    {
    }

    Value(const utf::Char8* str)
        : Value(UTF8StringView(str))
    {
    }

    /// ANSI

    Value(const ANSIString& str)
        : m_inner(JString(str))
    {
    }

    Value(const ANSIStringView& str)
        : Value(JString(str))
    {
    }

    /// UTF-16

    Value(const UTF16String& str)
        : m_inner(JString(str))
    {
    }

    Value(const UTF16StringView& str)
        : Value(JString(str))
    {
    }

    Value(const utf::Char16* str)
        : Value(UTF16StringView(str))
    {
    }

    /// UTF-32

    Value(const UTF32String& str)
        : m_inner(JString(str))
    {
    }

    Value(const UTF32StringView& str)
        : Value(JString(str))
    {
    }

    Value(const utf::Char32* str)
        : Value(UTF32StringView(str))
    {
    }

    /// Wide

    Value(const WideString& str)
        : m_inner(JString(str))
    {
    }

    Value(const WideStringView& str)
        : Value(JString(str))
    {
    }

    Value(const wchar_t* str)
        : Value(WideStringView(str))
    {
    }

    Value(Number number)
        : m_inner(number)
    {
    }

    Value(uint8 number)
        : m_inner(Number(number))
    {
    }

    Value(uint16 number)
        : m_inner(Number(number))
    {
    }

    Value(uint32 number)
        : m_inner(Number(number))
    {
    }

    Value(uint64 number)
        : m_inner(Number(number))
    {
    }

    Value(int8 number)
        : m_inner(Number(number))
    {
    }

    Value(int16 number)
        : m_inner(Number(number))
    {
    }

    Value(int32 number)
        : m_inner(Number(number))
    {
    }

    Value(int64 number)
        : m_inner(Number(number))
    {
    }

    Value(float number)
        : m_inner(Number(number))
    {
    }

    Value(bool boolean)
        : m_inner(boolean)
    {
    }

    Value(const JArray& array);
    Value(JArray&& array);

    Value(const Object& object);
    Value(Object&& object);

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
        return m_inner.Is<JString>();
    }

    HYP_FORCE_INLINE bool IsNumber() const
    {
        return m_inner.Is<Number>();
    }

    HYP_FORCE_INLINE bool IsBool() const
    {
        return m_inner.Is<bool>();
    }

    HYP_FORCE_INLINE bool IsArray() const
    {
        return m_inner.Is<JArrayRef>();
    }

    HYP_FORCE_INLINE bool IsObject() const
    {
        return m_inner.Is<ObjectRef>();
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

    HYP_FORCE_INLINE JString& AsString()
    {
        HYP_CORE_ASSERT(IsString());

        return m_inner.GetUnchecked<JString>();
    }

    HYP_FORCE_INLINE const JString& AsString() const
    {
        HYP_CORE_ASSERT(IsString());

        return m_inner.GetUnchecked<JString>();
    }

    HYP_FORCE_INLINE JString ToString(bool representation = false) const
    {
        return ToString(representation, 0);
    }

    HYP_FORCE_INLINE Number AsNumber() const
    {
        HYP_CORE_ASSERT(IsNumber());

        return m_inner.GetUnchecked<Number>();
    }

    /*! \brief Convert the JSON value to a number. If the value is undefined, the default value is returned.
     *
     *  \param defaultValue The default value to return if the value is not a number. (Default: 0.0)
     *  \return The number value.
     */
    HYP_FORCE_INLINE Number ToNumber(Number defaultValue = 0.0) const
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
            return StringUtil::Parse<Number>(String(AsString()).Data(), defaultValue);
        }

        return defaultValue;
    }

    HYP_FORCE_INLINE int8 ToInt8(int8 defaultValue = 0) const
    {
        return int8(ToNumber(Number(defaultValue)));
    }

    HYP_FORCE_INLINE int16 ToInt16(int16 defaultValue = 0) const
    {
        return int16(ToNumber(Number(defaultValue)));
    }

    HYP_FORCE_INLINE int32 ToInt32(int32 defaultValue = 0) const
    {
        return int32(ToNumber(Number(defaultValue)));
    }

    HYP_FORCE_INLINE int64 ToInt64(int64 defaultValue = 0) const
    {
        return int64(ToNumber(Number(defaultValue)));
    }

    HYP_FORCE_INLINE uint8 ToUInt8(uint8 defaultValue = 0) const
    {
        return uint8(ToNumber(Number(defaultValue)));
    }

    HYP_FORCE_INLINE uint16 ToUInt16(uint16 defaultValue = 0) const
    {
        return uint16(ToNumber(Number(defaultValue)));
    }

    HYP_FORCE_INLINE uint32 ToUInt32(uint32 defaultValue = 0) const
    {
        return uint32(ToNumber(Number(defaultValue)));
    }

    HYP_FORCE_INLINE uint64 ToUInt64(uint64 defaultValue = 0) const
    {
        return uint64(ToNumber(Number(defaultValue)));
    }

    HYP_FORCE_INLINE float ToFloat(float defaultValue = 0.0f) const
    {
        return static_cast<float>(ToNumber(Number(defaultValue)));
    }

    HYP_FORCE_INLINE double ToDouble(double defaultValue = 0.0) const
    {
        return ToNumber(Number(defaultValue));
    }

    HYP_FORCE_INLINE bool AsBool() const
    {
        HYP_CORE_ASSERT(IsBool());

        return m_inner.GetUnchecked<bool>();
    }

    /*! \brief Convert the JSON value to a boolean. If the value is undefined, the default value is returned.
     *
     *  \param defaultValue The default value to return if the value is not a boolean. (Default: false)
     *  \return The boolean value.
     */
    HYP_FORCE_INLINE bool ToBool(bool defaultValue = false) const
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
            return false;
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
            return true;
        }

        if (IsArray())
        {
            return true;
        }

        return defaultValue;
    }

    HYP_FORCE_INLINE JArray& AsArray()
    {
        HYP_CORE_ASSERT(IsArray());

        return *m_inner.GetUnchecked<JArrayRef>();
    }

    HYP_FORCE_INLINE const JArray& AsArray() const
    {
        HYP_CORE_ASSERT(IsArray());

        return *m_inner.GetUnchecked<JArrayRef>();
    }

    HYP_FORCE_INLINE JArray ToArray() const
    {
        if (IsArray())
        {
            return AsArray();
        }

        if (IsUndefined())
        {
            return JArray();
        }

        JArray arrayValue;
        arrayValue.PushBack(*this);
        return arrayValue;
    }

    HYP_FORCE_INLINE Object& AsObject()
    {
        HYP_CORE_ASSERT(IsObject());

        return *m_inner.GetUnchecked<ObjectRef>();
    }

    HYP_FORCE_INLINE const Object& AsObject() const
    {
        HYP_CORE_ASSERT(IsObject());

        return *m_inner.GetUnchecked<ObjectRef>();
    }

    const Object& ToObject() const;

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
    JString ToString(bool representation, uint32 depth) const;
    JString ToString_Internal(bool representation, uint32 depth) const;

    InnerType m_inner;
};

class Object final : public ArrayMap<JString, Value>
{
public:
    using Base = ArrayMap<JString, Value>;

    Object() = default;

    Object(std::initializer_list<KeyValuePair<JString, Value>> initializerList)
        : Base(initializerList)
    {
    }

    Object(const Object& other) = default;
    Object& operator=(const Object& other) = default;

    Object(Object&& other) noexcept = default;
    Object& operator=(Object&& other) noexcept = default;

    ~Object() = default;

    /*! \brief Merge another Object into this one.
     *  If a key exists in both objects, the value from the other object is used.
     *  If the value is an object, it is replaced with the other object's value.
     *
     *  \param other The other Object to merge.
     *  \return A reference to this Object.
     */
    template <class OtherContainerType>
    Object& Merge(OtherContainerType&& other)
    {
        Base::Merge(std::forward<OtherContainerType>(other));

        return *this;
    }

    /*! \brief Merge another Object into this one, recursively merging objects.
     *  If a key exists in both objects and the value is an object, the values are merged.
     *  Otherwise, the value from the other object is used.
     *
     *  \param other The other Object to merge.
     *  \return A reference to this Object.
     */
    Object& MergeDeep(const Object& other)
    {
        if (this == &other)
        {
            return *this;
        }

        for (const auto& kv : other)
        {
            const JString& key = kv.first;
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

CORE_API const Value& Undefined();
CORE_API const Value& Null();
CORE_API const Value& EmptyObject();
CORE_API const Value& EmptyArray();
CORE_API const Value& EmptyString();
CORE_API const Value& True();
CORE_API const Value& False();

CORE_API ParseResult Parse(const UTF8StringView& jsonString);
CORE_API ParseResult Parse(ByteReader& reader);

} // namespace Hyperion::DataProcessing::JSON

namespace Hyperion {
namespace JSON = DataProcessing::JSON;
} // namespace Hyperion
