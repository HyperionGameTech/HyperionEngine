/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FBOM_DATA_HPP
#define HYPERION_FBOM_DATA_HPP

#include <core/Name.hpp>

#include <core/containers/String.hpp>

#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/Optional.hpp>

#include <core/memory/ByteBuffer.hpp>
#include <core/memory/RefCountedPtr.hpp>

#include <core/debug/Debug.hpp>

#include <core/serialization/fbom/FBOMResult.hpp>
#include <core/serialization/fbom/FBOMBaseTypes.hpp>
#include <core/serialization/fbom/FBOMInterfaces.hpp>
#include <core/serialization/fbom/FBOMEnums.hpp>

#include <core/math/MathUtil.hpp>
#include <core/math/Vector2.hpp>
#include <core/math/Vector3.hpp>
#include <core/math/Vector4.hpp>
#include <core/math/Matrix3.hpp>
#include <core/math/Matrix4.hpp>
#include <core/math/Quaternion.hpp>

#include <Types.hpp>

#define FBOM_ASSERT(cond, message)                                \
    do                                                            \
    {                                                             \
        static const char* _message = (message);                  \
                                                                  \
        if (!(cond))                                              \
        {                                                         \
            return FBOMResult { FBOMResult::FBOM_ERR, _message }; \
        }                                                         \
    }                                                             \
    while (0)

#define FBOM_RETURN_OK      \
    return FBOMResult       \
    {                       \
        FBOMResult::FBOM_OK \
    }

namespace hyperion {

namespace json {
class JSONValue;
} // namespace json

struct HypData;

template <class T, class T2>
struct HypDataHelper;

enum class FBOMDataFlags
{
    NONE = 0x0,
    COMPRESSED = 0x1,
    EXT_REF_PLACEHOLDER = 0x2
};

HYP_MAKE_ENUM_FLAGS(FBOMDataFlags)

namespace fbom {

class FBOMObject;
class FBOMArray;
struct FBOMDeserializedObject;
class FBOMLoadContext;

template <class FBOMData, class T, class FBOMTypeClass, class T2 = void>
struct FBOMDataTypeOps;

template <class FBOMData, class T, class FBOMTypeClass>
struct FBOMDataTypeOps<FBOMData, T, FBOMTypeClass, std::enable_if_t<std::is_fundamental_v<T> || is_pod_type<T>>>
{
    const FBOMData& target;

    HYP_FORCE_INLINE bool Is() const
    {
        return target.GetType().Is(FBOMTypeClass(), /* allow_unbounded */ true);
    }

    HYP_FORCE_INLINE FBOMResult Read(T* out) const
    {
        const TypeID type_id = target.GetType().GetNativeTypeID();

        auto ReadAsType = [this, &out, &type_id]<class TReadAsType>(TypeWrapper<TReadAsType>) -> bool
        {
            if (type_id == TypeID::ForType<TReadAsType>())
            {
                TReadAsType value;
                target.ReadBytes(target.GetType().size, &value);

                *out = static_cast<T>(value);

                return true;
            }

            return false;
        };

        if (Is())
        {
            target.ReadBytes(FBOMTypeClass().size, out);

            FBOM_RETURN_OK;
        }

        // Allow implicit conversion between numeric types
        if constexpr (std::is_arithmetic_v<T> || std::is_enum_v<T>)
        {
            if (target.GetType().IsNumeric())
            {
                AssertThrowMsg(type_id != TypeID::Void(), "Type must have a valid native TypeID if it is numeric");

                if (ReadAsType(TypeWrapper<uint8> {}) || ReadAsType(TypeWrapper<uint16> {}) || ReadAsType(TypeWrapper<uint32> {}) || ReadAsType(TypeWrapper<uint64> {})
                    || ReadAsType(TypeWrapper<int8> {}) || ReadAsType(TypeWrapper<int16> {}) || ReadAsType(TypeWrapper<int32> {}) || ReadAsType(TypeWrapper<int64> {})
                    || ReadAsType(TypeWrapper<float> {}) || ReadAsType(TypeWrapper<double> {}))
                {
                    FBOM_RETURN_OK;
                }
            }
        }

        return FBOMResult { FBOMResult::FBOM_ERR, "Type mismatch" };
    }

    /*! \brief Read with static_cast to result type */
    template <class TOther>
    FBOMResult Read(TOther* out) const
    {
        static_assert(sizeof(T) == sizeof(TOther), "sizeof(T) must match sizeof(TOther)");

        if constexpr (std::is_enum_v<TOther>)
        {
            static_assert(std::is_convertible_v<T, std::underlying_type_t<TOther>>, "T must be convertible to underlying type of TOther");
        }
        else
        {
            static_assert(std::is_convertible_v<T, TOther>, "T must be convertible to TOther");
        }

        T read_value;

        if (FBOMResult err = Read(&read_value))
        {
            return err;
        }

        *out = static_cast<TOther>(read_value);

        FBOM_RETURN_OK;
    }

    static FBOMData From(const T& value, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        static_assert(std::is_standard_layout_v<T>, "Type must be standard layout");

        FBOMTypeClass type;
        AssertThrowMsg(sizeof(T) == type.size, "sizeof(T) must be equal to FBOMTypeClass::size");

        FBOMData data { type, flags };
        data.SetBytes(sizeof(T), &value);

        return data;
    }
};

class HYP_API FBOMData final : public FBOMSerializableBase
{
public:
    FBOMData(EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE);
    FBOMData(const FBOMType& type, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE);
    FBOMData(const FBOMType& type, ByteBuffer&& byte_buffer, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE);
    FBOMData(const FBOMData& other);
    FBOMData& operator=(const FBOMData& other);
    FBOMData(FBOMData&& other) noexcept;
    FBOMData& operator=(FBOMData&& other) noexcept;
    virtual ~FBOMData() override;

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return !m_type.IsUnset() || m_bytes.Any();
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return !bool(*this);
    }

    HYP_FORCE_INLINE bool IsUnset() const
    {
        return m_type.IsUnset();
    }

    HYP_FORCE_INLINE const FBOMType& GetType() const
    {
        return m_type;
    }

    HYP_FORCE_INLINE const ByteBuffer& GetBytes() const
    {
        return m_bytes;
    }

    HYP_FORCE_INLINE SizeType TotalSize() const
    {
        return m_bytes.Size();
    }

    HYP_FORCE_INLINE EnumFlags<FBOMDataFlags> GetFlags() const
    {
        return m_flags;
    }

    HYP_FORCE_INLINE bool IsCompressed() const
    {
        return m_flags & FBOMDataFlags::COMPRESSED;
    }

    /*! \returns The number of bytes read */
    SizeType ReadBytes(SizeType n, void* out) const;
    ByteBuffer ReadBytes() const;
    ByteBuffer ReadBytes(SizeType n) const;

    void SetBytes(const ByteBuffer& byte_buffer);
    void SetBytes(SizeType count, const void* data);

#define FBOM_TYPE_FUNCTIONS(type_name, c_type)                                                                          \
    bool Is##type_name() const                                                                                          \
    {                                                                                                                   \
        return FBOMDataTypeOps<FBOMData, c_type, FBOM##type_name> { *this }.Is();                                       \
    }                                                                                                                   \
                                                                                                                        \
    FBOMResult Read(c_type* out) const                                                                                  \
    {                                                                                                                   \
        return FBOMDataTypeOps<FBOMData, c_type, FBOM##type_name> { *this }.Read(out);                                  \
    }                                                                                                                   \
                                                                                                                        \
    template <class T>                                                                                                  \
    FBOMResult Read##type_name(T* out) const                                                                            \
    {                                                                                                                   \
        return FBOMDataTypeOps<FBOMData, c_type, FBOM##type_name> { *this }.Read(out);                                  \
    }                                                                                                                   \
                                                                                                                        \
    static FBOMData From##type_name(const c_type& value, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)          \
    {                                                                                                                   \
        return FBOMDataTypeOps<FBOMData, c_type, FBOM##type_name>::From(value, flags);                                  \
    }                                                                                                                   \
                                                                                                                        \
    explicit FBOMData(const c_type& value, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)                        \
        : m_type(FBOM##type_name()),                                                                                    \
          m_flags(flags)                                                                                                \
    {                                                                                                                   \
        static_assert(std::is_standard_layout_v<c_type>, "Type " #c_type " must be standard layout");                   \
                                                                                                                        \
        AssertThrowMsg(sizeof(c_type) == m_type.size, "sizeof(" #c_type ") must be equal to FBOM" #type_name "::size"); \
                                                                                                                        \
        SetBytes(sizeof(c_type), &value);                                                                               \
    }

    FBOM_TYPE_FUNCTIONS(UInt8, uint8)
    FBOM_TYPE_FUNCTIONS(UInt16, uint16)
    FBOM_TYPE_FUNCTIONS(UInt32, uint32)
    FBOM_TYPE_FUNCTIONS(UInt64, uint64)
    FBOM_TYPE_FUNCTIONS(Int8, int8)
    FBOM_TYPE_FUNCTIONS(Int16, int16)
    FBOM_TYPE_FUNCTIONS(Int32, int32)
    FBOM_TYPE_FUNCTIONS(Int64, int64)
    FBOM_TYPE_FUNCTIONS(Char, char)
    FBOM_TYPE_FUNCTIONS(Float, float)
    FBOM_TYPE_FUNCTIONS(Double, double)
    FBOM_TYPE_FUNCTIONS(Bool, bool)
    FBOM_TYPE_FUNCTIONS(Mat3f, Matrix3)
    FBOM_TYPE_FUNCTIONS(Mat4f, Matrix4)
    FBOM_TYPE_FUNCTIONS(Vec2f, Vec2f)
    FBOM_TYPE_FUNCTIONS(Vec3f, Vec3f)
    FBOM_TYPE_FUNCTIONS(Vec4f, Vec4f)
    FBOM_TYPE_FUNCTIONS(Vec2i, Vec2i)
    FBOM_TYPE_FUNCTIONS(Vec3i, Vec3i)
    FBOM_TYPE_FUNCTIONS(Vec4i, Vec4i)
    FBOM_TYPE_FUNCTIONS(Vec2u, Vec2u)
    FBOM_TYPE_FUNCTIONS(Vec3u, Vec3u)
    FBOM_TYPE_FUNCTIONS(Vec4u, Vec4u)
    FBOM_TYPE_FUNCTIONS(Quat4f, Quaternion)

#undef FBOM_TYPE_FUNCTIONS

#pragma region String
    HYP_FORCE_INLINE bool IsString() const
    {
        return m_type.IsOrExtends(FBOMString());
    }

    template <int string_type>
    HYP_FORCE_INLINE FBOMResult ReadString(containers::detail::String<string_type>& str) const
    {
        static_assert(string_type == int(StringType::ANSI) || string_type == int(StringType::UTF8), "String type must be ANSI or UTF8");

        FBOM_ASSERT(IsString(), "Type mismatch (expected String)");

        const SizeType total_size = TotalSize();
        char* ch = new char[total_size + 1];

        ReadBytes(total_size, ch);
        ch[total_size] = '\0';

        str = ch;

        delete[] ch;

        FBOM_RETURN_OK;
    }

    template <int TStringType>
    HYP_FORCE_INLINE static FBOMData FromString(const StringView<TStringType>& str)
    {
        static_assert(TStringType == int(StringType::ANSI) || TStringType == int(StringType::UTF8), "String type must be ANSI or UTF8");

        return FBOMData(FBOMString(), ByteBuffer(str.Size(), str.Data()));
    }

    template <int TStringType>
    HYP_FORCE_INLINE static FBOMData FromString(const containers::detail::String<TStringType>& str)
    {
        return FromString(StringView<TStringType>(str));
    }

    template <int TStringType>
    explicit FBOMData(const StringView<TStringType>& str)
        : FBOMData(FromString(str))
    {
    }

    template <int TStringType>
    explicit FBOMData(const containers::detail::String<TStringType>& str)
        : FBOMData(FromString(str))
    {
    }

#pragma endregion String

#pragma region JSON
    FBOMResult ToJSON(FBOMLoadContext& context, json::JSONValue& out_json) const;

    static FBOMData FromJSON(const json::JSONValue& json_value);

    explicit FBOMData(const json::JSONValue& json_value);
#pragma endregion JSON

#pragma region ByteBuffer
    HYP_FORCE_INLINE bool IsByteBuffer() const
    {
        return m_type.IsOrExtends(FBOMByteBuffer());
    }

    HYP_FORCE_INLINE FBOMResult ReadByteBuffer(ByteBuffer& byte_buffer) const
    {
        FBOM_ASSERT(IsByteBuffer(), "Type mismatch (expected ByteBuffer)");

        byte_buffer = m_bytes;

        FBOM_RETURN_OK;
    }

    static FBOMData FromByteBuffer(const ByteBuffer& byte_buffer, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        FBOMData data(FBOMByteBuffer(byte_buffer.Size()), flags);
        data.SetBytes(byte_buffer.Size(), byte_buffer.Data());

        return data;
    }

#pragma endregion ByteBuffer

#pragma region Struct

    template <class T>
    HYP_FORCE_INLINE bool IsStruct() const
    {
        return m_type.IsOrExtends(FBOMStruct(TypeNameWithoutNamespace<NormalizedType<T>>(), -1, TypeID::ForType<NormalizedType<T>>()), /* allow_unbounded */ true, /* allow_void_type_id */ true);
    }

    HYP_FORCE_INLINE bool IsStruct(const char* type_name, TypeID type_id) const
    {
        return m_type.IsOrExtends(FBOMStruct(type_name, -1, type_id), /* allow_unbounded */ true, /* allow_void_type_id */ true);
    }

    HYP_FORCE_INLINE bool IsStruct(const char* type_name, SizeType size, TypeID type_id) const
    {
        return m_type.IsOrExtends(FBOMStruct(type_name, size, type_id), /* allow_unbounded */ true, /* allow_void_type_id */ true);
    }

    HYP_FORCE_INLINE FBOMResult ReadStruct(const char* type_name, SizeType size, TypeID type_id, void* out) const
    {
        AssertThrow(out != nullptr);

        FBOM_ASSERT(IsStruct(type_name, size, type_id), "Object is not a struct or not struct of requested size");

        ReadBytes(size, out);

        FBOM_RETURN_OK;
    }

    template <class T, bool CompileTimeChecked = true>
    HYP_FORCE_INLINE FBOMResult ReadStruct(T* out) const
    {
        AssertStaticMsgCond(CompileTimeChecked, is_pod_type<T>, "T must be POD to use ReadStruct()");

        return ReadStruct(TypeNameWithoutNamespace<NormalizedType<T>>().Data(), sizeof(NormalizedType<T>), TypeID::ForType<NormalizedType<T>>(), out);
    }

    template <class T, bool CompileTimeChecked = true>
    HYP_FORCE_INLINE T ReadStruct() const
    {
        AssertStaticMsgCond(CompileTimeChecked, is_pod_type<T>, "T must be POD to use ReadStruct()");

        ValueStorage<NormalizedType<T>> result_storage;

        if (FBOMResult err = ReadStruct(TypeNameWithoutNamespace<NormalizedType<T>>().Data(), sizeof(NormalizedType<T>), TypeID::ForType<NormalizedType<T>>(), result_storage.GetPointer()))
        {
            HYP_FAIL("Failed to read struct of type %s: %s", TypeNameWithoutNamespace<NormalizedType<T>>().Data(), *err.message);
        }

        return result_storage.Get();
    }

    template <class T>
    HYP_FORCE_INLINE static FBOMData FromStruct(const T& value, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        return FBOMData(FBOMStruct::Create<T, true>(), ByteBuffer(sizeof(T), &value), flags);
    }

    template <class T>
    HYP_FORCE_INLINE static FBOMData FromStructUnchecked(const T& value, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        return FBOMData(FBOMStruct::Create<T, false>(), ByteBuffer(sizeof(T), &value), flags);
    }

    // template <class T, typename = std::enable_if_t< FBOMStruct::is_valid_struct_type< NormalizedType<T> > && std::is_class_v< NormalizedType<T> > > >
    // explicit FBOMData(const T &value) : FBOMData(FromStruct(value)) { }

#pragma endregion Struct

#pragma region Name
    HYP_FORCE_INLINE bool IsName() const
    {
        return IsStruct<Name>();
    }

    HYP_FORCE_INLINE FBOMResult ReadName(Name* out) const
    {
        return ReadStruct<Name>(out);
    }

    HYP_FORCE_INLINE static FBOMData FromName(Name name)
    {
        FBOMData data(FBOMStruct::Create<Name>());
        data.SetBytes(sizeof(Name), &name);

        return data;
    }

#pragma endregion Name

#pragma region Sequence
    HYP_FORCE_INLINE bool IsSequence() const
    {
        return m_type.IsOrExtends(FBOMSequence());
    }

    // does NOT check that the types are exact, just that the size is a match
    HYP_FORCE_INLINE bool IsSequenceMatching(const FBOMType& held_type, SizeType num_items) const
    {
        return m_type.IsOrExtends(FBOMSequence(held_type, num_items));
    }

    // does the array size equal byte_size bytes?
    HYP_FORCE_INLINE bool IsSequenceOfByteSize(SizeType byte_size) const
    {
        return m_type.IsOrExtends(FBOMSequence(FBOMUInt8(), byte_size));
    }

    /*! \brief If type is an sequence, return the number of elements,
        assuming the sequence contains the given type. Note, sequence could
        contain another type, and still a result will be returned.

        If type is /not/ an sequence, return zero. */
    HYP_FORCE_INLINE SizeType NumElements(const FBOMType& held_type) const
    {
        if (!IsSequence())
        {
            return 0;
        }

        const SizeType held_type_size = held_type.size;

        if (held_type_size == 0)
        {
            return 0;
        }

        return TotalSize() / held_type_size;
    }

    // count is number of ELEMENTS
    HYP_FORCE_INLINE FBOMResult ReadElements(const FBOMType& held_type, SizeType num_items, void* out) const
    {
        AssertThrow(out != nullptr);

        FBOM_ASSERT(IsSequence(), "Object is not an sequence");

        ReadBytes(held_type.size * num_items, out);

        FBOM_RETURN_OK;
    }

#pragma endregion Sequence

#pragma region Object
    HYP_FORCE_INLINE bool IsObject() const
    {
        return m_type.IsOrExtends(FBOMBaseObjectType());
    }

    FBOMResult ReadObject(FBOMLoadContext& context, FBOMObject& out_object) const;

    static FBOMData FromObject(const FBOMObject& object, bool keep_native_object = true);
    static FBOMData FromObject(FBOMObject&& object, bool keep_native_object = true);

#pragma endregion Object

#pragma region Array
    HYP_FORCE_INLINE bool IsArray() const
    {
        return m_type.IsOrExtends(FBOMArrayType());
    }

    FBOMResult ReadArray(FBOMLoadContext& context, FBOMArray& out_array) const;

    static FBOMData FromArray(const FBOMArray& array);
#pragma endregion Array

    HYP_FORCE_INLINE FBOMResult ReadBytes(SizeType count, ByteBuffer& out) const
    {
        FBOM_ASSERT(count <= m_bytes.Size(), "Attempt to read past max size of object");

        out = ByteBuffer(count, m_bytes.Data());

        FBOM_RETURN_OK;
    }

    HYP_FORCE_INLINE FBOMResult ReadAsType(const FBOMType& read_type, void* out) const
    {
        AssertThrow(out != nullptr);

        FBOM_ASSERT(m_type.IsOrExtends(read_type), "Type mismatch");

        ReadBytes(read_type.size, out);

        FBOM_RETURN_OK;
    }

    FBOMResult Visit(FBOMWriter* writer, ByteWriter* out, EnumFlags<FBOMDataAttributes> attributes = FBOMDataAttributes::NONE) const
    {
        return Visit(GetUniqueID(), writer, out, attributes);
    }

    virtual FBOMResult Visit(UniqueID id, FBOMWriter* writer, ByteWriter* out, EnumFlags<FBOMDataAttributes> attributes = FBOMDataAttributes::NONE) const override;

    virtual String ToString(bool deep = true) const override;
    virtual UniqueID GetUniqueID() const override;
    virtual HashCode GetHashCode() const override;

    HYP_FORCE_INLINE const RC<HypData>& GetDeserializedObject() const
    {
        return m_deserialized_object;
    }

private:
    ByteBuffer m_bytes;
    FBOMType m_type;

    EnumFlags<FBOMDataFlags> m_flags;

    RC<HypData> m_deserialized_object;
};

} // namespace fbom
} // namespace hyperion

#undef FBOM_RETURN_OK
#undef FBOM_ASSERT

#endif
