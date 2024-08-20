/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FBOM_DATA_HPP
#define HYPERION_FBOM_DATA_HPP

#include <core/Name.hpp>

#include <core/containers/String.hpp>

#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/Optional.hpp>

#include <core/memory/ByteBuffer.hpp>
#include <core/memory/RefCountedPtr.hpp>

#include <core/system/Debug.hpp>

#include <asset/serialization/fbom/FBOMResult.hpp>
#include <asset/serialization/fbom/FBOMBaseTypes.hpp>
#include <asset/serialization/fbom/FBOMInterfaces.hpp>

#include <math/MathUtil.hpp>
#include <math/Vector2.hpp>
#include <math/Vector3.hpp>
#include <math/Vector4.hpp>
#include <math/Matrix3.hpp>
#include <math/Matrix4.hpp>
#include <math/Quaternion.hpp>

#include <Types.hpp>

#define FBOM_ASSERT(cond, message) \
    do { \
        static const char *_message = (message); \
        \
        AssertDebugMsg((cond), "FBOM error: %s", _message); \
        \
        if (!(cond)) { \
            return FBOMResult { FBOMResult::FBOM_ERR, _message }; \
        } \
    } while (0)


#define FBOM_RETURN_OK return FBOMResult { FBOMResult::FBOM_OK }

namespace hyperion {

struct HypData;

namespace fbom {

class FBOMObject;
class FBOMArray;
struct FBOMDeserializedObject;

class HYP_API FBOMData : public IFBOMSerializable
{
public:
    FBOMData();
    FBOMData(const FBOMType &type);
    FBOMData(const FBOMType &type, ByteBuffer &&byte_buffer);
    FBOMData(const FBOMData &other);
    FBOMData &operator=(const FBOMData &other);
    FBOMData(FBOMData &&other) noexcept;
    FBOMData &operator=(FBOMData &&other) noexcept;
    virtual ~FBOMData();

    HYP_FORCE_INLINE explicit operator bool() const
        { return !type.IsUnset() || bytes.Any(); }

    HYP_FORCE_INLINE bool operator!() const
        { return !bool(*this); }

    HYP_FORCE_INLINE bool IsUnset() const
        { return type.IsUnset(); }

    HYP_FORCE_INLINE const FBOMType &GetType() const
        { return type; }

    HYP_FORCE_INLINE const ByteBuffer &GetBytes() const
        { return bytes; }

    HYP_FORCE_INLINE SizeType TotalSize() const
        { return bytes.Size(); }

    /*! \returns The number of bytes read */
    SizeType ReadBytes(SizeType n, void *out) const;
    ByteBuffer ReadBytes() const;
    ByteBuffer ReadBytes(SizeType n) const;

    void SetBytes(const ByteBuffer &byte_buffer);
    void SetBytes(SizeType count, const void *data);

#define FBOM_TYPE_FUNCTIONS(type_name, c_type) \
    bool Is##type_name() const { return type.Is(FBOM##type_name(), /* allow_unbounded */ true); } \
    FBOMResult Read(c_type *out) const \
    { \
        FBOM_ASSERT(Is##type_name(), "Type mismatch (expected " #c_type ")"); \
        ReadBytes(FBOM##type_name().size, out); \
        FBOM_RETURN_OK; \
    } \
    /*! \brief Read with static_cast to result type */ \
    template <class T> \
    FBOMResult Read##type_name(T *out) const \
    { \
        static_assert(sizeof(T) == sizeof(c_type), "sizeof(T) must match sizeof " #c_type "!"); \
        if constexpr (std::is_enum_v<T>) { \
            static_assert(std::is_same_v<std::underlying_type_t<T>, c_type>, "Underlying type of enum T must be " #c_type "!"); \
        } else { \
            static_assert(std::is_convertible_v<c_type, T>, "T must be convertible to " #c_type "!"); \
        } \
        c_type read_value; \
        FBOM_ASSERT(Is##type_name(), "Type mismatch (expected " #c_type ")"); \
        ReadBytes(FBOM##type_name().size, &read_value); \
        *out = static_cast<T>(read_value); \
        FBOM_RETURN_OK; \
    } \
    static FBOMData From##type_name(const c_type &value) \
    { \
        static_assert(std::is_standard_layout_v<c_type>, "Type " #c_type " must be standard layout"); \
        \
        FBOM##type_name type; \
        AssertThrowMsg(sizeof(c_type) == type.size, "sizeof(" #c_type ") must be equal to FBOM" #type_name "::size"); \
        \
        FBOMData data(type); \
        data.SetBytes(sizeof(c_type), &value); \
        return data; \
    } \
    explicit FBOMData(c_type value) \
        : FBOMData(From##type_name(value)) { } \

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
        { return type.IsOrExtends(FBOMString()); }

    template <int string_type>
    HYP_FORCE_INLINE FBOMResult ReadString(containers::detail::String<string_type> &str) const
    {
        static_assert(string_type == int(StringType::ANSI) || string_type == int(StringType::UTF8), "String type must be ANSI or UTF8");

        FBOM_ASSERT(IsString(), "Type mismatch (expected String)");

        const SizeType total_size = TotalSize();
        char *ch = new char[total_size + 1];

        ReadBytes(total_size, ch);
        ch[total_size] = '\0';

        str = ch;

        delete[] ch;

        FBOM_RETURN_OK;
    }
    
    template <int string_type>
    HYP_FORCE_INLINE static FBOMData FromString(const StringView<string_type> &str)
    {
        static_assert(string_type == int(StringType::ANSI) || string_type == int(StringType::UTF8), "String type must be ANSI or UTF8");

        return FBOMData(FBOMString(), ByteBuffer(str.Size(), str.Data()));
    }
    
    template <int string_type>
    HYP_FORCE_INLINE static FBOMData FromString(const containers::detail::String<string_type> &str)
    {
        return FromString(StringView<string_type>(str));
    }

    template <int string_type>
    explicit FBOMData(const StringView<string_type> &str) : FBOMData(FromString(str)) { }

    template <int string_type>
    explicit FBOMData(const containers::detail::String<string_type> &str) : FBOMData(FromString(str)) { }

#pragma endregion String

#pragma region ByteBuffer
    HYP_FORCE_INLINE bool IsByteBuffer() const
        { return type.IsOrExtends(FBOMByteBuffer()); }

    HYP_FORCE_INLINE FBOMResult ReadByteBuffer(ByteBuffer &byte_buffer) const
    {
        FBOM_ASSERT(IsByteBuffer(), "Type mismatch (expected ByteBuffer)");

        byte_buffer = bytes;

        FBOM_RETURN_OK;
    }
    
    static FBOMData FromByteBuffer(const ByteBuffer &byte_buffer)
    {
        FBOMData data(FBOMByteBuffer(byte_buffer.Size()));
        data.SetBytes(byte_buffer.Size(), byte_buffer.Data());

        return data;
    }

#pragma endregion ByteBuffer

#pragma region Struct
    template <class T>
    HYP_FORCE_INLINE bool IsStruct() const
        { return type.IsOrExtends(FBOMStruct(TypeNameWithoutNamespace<NormalizedType<T>>(), -1, TypeID::ForType<NormalizedType<T>>()), /* allow_unbounded */ true, /* allow_void_type_id */ true); }

    HYP_FORCE_INLINE bool IsStruct(const char *type_name, TypeID type_id) const
        { return type.IsOrExtends(FBOMStruct(type_name, -1, type_id), /* allow_unbounded */ true, /* allow_void_type_id */ true); }

    HYP_FORCE_INLINE bool IsStruct(const char *type_name, SizeType size, TypeID type_id) const
        { return type.IsOrExtends(FBOMStruct(type_name, size, type_id), /* allow_unbounded */ true, /* allow_void_type_id */ true); }

    HYP_FORCE_INLINE FBOMResult ReadStruct(const char *type_name, SizeType size, TypeID type_id, void *out) const
    {
        AssertThrow(out != nullptr);

        FBOM_ASSERT(IsStruct(type_name, size, type_id), "Object is not a struct or not struct of requested size");

        ReadBytes(size, out);

        FBOM_RETURN_OK;
    }

    template <class T>
    HYP_FORCE_INLINE FBOMResult ReadStruct(T *out) const
    {
        static_assert(std::is_standard_layout_v<T>, "T must be standard layout to use ReadStruct()");
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable to use ReadStruct()");

        return ReadStruct(TypeNameWithoutNamespace<NormalizedType<T>>().Data(), sizeof(NormalizedType<T>), TypeID::ForType<NormalizedType<T>>(), out);
    }

    template <class T>
    HYP_FORCE_INLINE T ReadStruct() const
    {
        static_assert(std::is_standard_layout_v<T>, "T must be standard layout to use ReadStruct()");
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable to use ReadStruct()");

        ValueStorage<NormalizedType<T>> result_storage;

        if (FBOMResult err = ReadStruct(TypeNameWithoutNamespace<NormalizedType<T>>().Data(), sizeof(NormalizedType<T>), TypeID::ForType<NormalizedType<T>>(), result_storage.GetPointer())) {
            HYP_FAIL("Failed to read struct of type %s: %s", TypeNameWithoutNamespace<NormalizedType<T>>().Data(), *err.message);
        }

        return result_storage.Get();
    }

    template <class T>
    HYP_FORCE_INLINE static FBOMData FromStruct(const T &value)
    {
        return FBOMData(FBOMStruct::Create<T>(), ByteBuffer(sizeof(T), &value));
    }

    template <class T, typename = std::enable_if_t< FBOMStruct::is_valid_struct_type< NormalizedType<T> > && std::is_class_v< NormalizedType<T> > > >
    explicit FBOMData(const T &value) : FBOMData(FromStruct(value)) { }

#pragma endregion Struct

#pragma region Name
    HYP_FORCE_INLINE bool IsName() const
        { return IsStruct<Name>(); }

    HYP_FORCE_INLINE FBOMResult ReadName(Name *out) const
        { return ReadStruct<Name>(out); }
    
    HYP_FORCE_INLINE static FBOMData FromName(Name name)
    {
        FBOMData data(FBOMStruct::Create<Name>());
        data.SetBytes(sizeof(Name), &name);

        return data;
    }

#pragma endregion Name

#pragma region Sequence
    HYP_FORCE_INLINE bool IsSequence() const
        { return type.IsOrExtends(FBOMSequence()); }

    // does NOT check that the types are exact, just that the size is a match
    HYP_FORCE_INLINE bool IsSequenceMatching(const FBOMType &held_type, SizeType num_items) const
        { return type.IsOrExtends(FBOMSequence(held_type, num_items)); }

    // does the array size equal byte_size bytes?
    HYP_FORCE_INLINE bool IsSequenceOfByteSize(SizeType byte_size) const
        { return type.IsOrExtends(FBOMSequence(FBOMUInt8(), byte_size)); }

    /*! \brief If type is an sequence, return the number of elements,
        assuming the sequence contains the given type. Note, sequence could
        contain another type, and still a result will be returned.
        
        If type is /not/ an sequence, return zero. */
    HYP_FORCE_INLINE SizeType NumElements(const FBOMType &held_type) const
    {
        if (!IsSequence()) {
            return 0;
        }

        const SizeType held_type_size = held_type.size;
        
        if (held_type_size == 0) {
            return 0;
        }

        return TotalSize() / held_type_size;
    }

    // count is number of ELEMENTS
    HYP_FORCE_INLINE FBOMResult ReadElements(const FBOMType &held_type, SizeType num_items, void *out) const
    {
        AssertThrow(out != nullptr);

        FBOM_ASSERT(IsSequence(), "Object is not an sequence");

        ReadBytes(held_type.size * num_items, out);

        FBOM_RETURN_OK;
    }

#pragma endregion Sequence

#pragma region Object
    HYP_FORCE_INLINE bool IsObject() const
        { return type.IsOrExtends(FBOMBaseObjectType()); }

    FBOMResult ReadObject(FBOMObject &out_object) const;
    
    static FBOMData FromObject(const FBOMObject &object, bool keep_native_object = true);
    static FBOMData FromObject(FBOMObject &&object, bool keep_native_object = true);

#pragma endregion Object

#pragma region Array
    HYP_FORCE_INLINE bool IsArray() const
        { return type.IsOrExtends(FBOMArrayType()); }

    FBOMResult ReadArray(FBOMArray &out_array) const;
    
    static FBOMData FromArray(const FBOMArray &array);
#pragma endregion Array

    HYP_FORCE_INLINE FBOMResult ReadBytes(SizeType count, ByteBuffer &out) const
    {
        FBOM_ASSERT(count <= bytes.Size(), "Attempt to read past max size of object");

        out = ByteBuffer(count, bytes.Data());

        FBOM_RETURN_OK;
    }

    HYP_FORCE_INLINE FBOMResult ReadAsType(const FBOMType &read_type, void *out) const
    {
        AssertThrow(out != nullptr);

        FBOM_ASSERT(type.IsOrExtends(read_type), "Type mismatch");

        ReadBytes(read_type.size, out);

        FBOM_RETURN_OK;
    }

    FBOMResult Visit(FBOMWriter *writer, ByteWriter *out, EnumFlags<FBOMDataAttributes> attributes = FBOMDataAttributes::NONE) const
        { return Visit(GetUniqueID(), writer, out, attributes); }

    virtual FBOMResult Visit(UniqueID id, FBOMWriter *writer, ByteWriter *out, EnumFlags<FBOMDataAttributes> attributes = FBOMDataAttributes::NONE) const override;

    HYP_FORCE_INLINE const RC<HypData> &GetDeserializedObject() const
        { return m_deserialized_object; }
    
    virtual String ToString(bool deep = true) const override;
    virtual UniqueID GetUniqueID() const override;
    virtual HashCode GetHashCode() const override;

private:
    ByteBuffer      bytes;
    FBOMType        type;

    RC<HypData>     m_deserialized_object;
};

} // namespace fbom
} // namespace hyperion

#undef FBOM_RETURN_OK
#undef FBOM_ASSERT

#endif
