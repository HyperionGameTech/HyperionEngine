/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

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

#include <core/utilities/Uuid.hpp>

#include <core/Types.hpp>

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

enum class FBOMDataFlags : uint32
{
    NONE = 0x0,
    COMPRESSED = 0x1,
    EXT_REF_PLACEHOLDER = 0x2
};

HYP_MAKE_ENUM_FLAGS(FBOMDataFlags)

namespace serialization {

class FBOMObject;
class FBOMArray;
struct FBOMDeserializedObject;
class FBOMLoadContext;

template <class FBOMData, class T, class FBOMTypeClass, class T2 = void>
struct FBOMDataTypeOps;

template <class FBOMData, class T, class FBOMTypeClass>
struct FBOMDataTypeOps<FBOMData, T, FBOMTypeClass, std::enable_if_t<std::is_fundamental_v<T> || isPodType<T>>>
{
    const FBOMData& target;

    HYP_FORCE_INLINE bool IsType() const
    {
        return target.GetType().IsType(FBOMTypeClass(), /* allowUnbounded */ true);
    }

    HYP_FORCE_INLINE FBOMResult Read(T* out) const
    {
        const TypeId typeId = target.GetType().GetNativeTypeId();

        auto readAsType = [this, &out, &typeId]<class TReadAsType>(TypeWrapper<TReadAsType>) -> bool
        {
            if (typeId == TypeId::ForType<TReadAsType>())
            {
                TReadAsType value;
                target.ReadBytes(target.GetType().size, &value);

                *out = static_cast<T>(value);

                return true;
            }

            return false;
        };

        if (IsType())
        {
            target.ReadBytes(FBOMTypeClass().size, out);

            FBOM_RETURN_OK;
        }

        // Allow implicit conversion between numeric types
        if constexpr (std::is_arithmetic_v<T> || std::is_enum_v<T>)
        {
            if (target.GetType().IsNumeric())
            {
                HYP_CORE_ASSERT(typeId != TypeId::Void(), "Type must have a valid native TypeId if it is numeric");

                if (readAsType(TypeWrapper<uint8> {}) || readAsType(TypeWrapper<uint16> {}) || readAsType(TypeWrapper<uint32> {}) || readAsType(TypeWrapper<uint64> {})
                    || readAsType(TypeWrapper<int8> {}) || readAsType(TypeWrapper<int16> {}) || readAsType(TypeWrapper<int32> {}) || readAsType(TypeWrapper<int64> {})
                    || readAsType(TypeWrapper<float> {}) || readAsType(TypeWrapper<double> {}))
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

        T readValue;

        if (FBOMResult err = Read(&readValue))
        {
            return err;
        }

        *out = static_cast<TOther>(readValue);

        FBOM_RETURN_OK;
    }

    static FBOMData From(const T& value, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        static_assert(std::is_standard_layout_v<T>, "Type must be standard layout");

        FBOMTypeClass type;
        HYP_CORE_ASSERT(sizeof(T) == type.size, "sizeof(T) must be equal to FBOMTypeClass::size");

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
    FBOMData(const FBOMType& type, ByteBuffer&& byteBuffer, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE);
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

    HYP_FORCE_INLINE void SetFlags(EnumFlags<FBOMDataFlags> flags)
    {
        m_flags = flags;
    }

    HYP_FORCE_INLINE bool IsCompressed() const
    {
        return m_flags & FBOMDataFlags::COMPRESSED;
    }

    /*! \returns The number of bytes read */
    SizeType ReadBytes(SizeType n, void* out) const;
    ByteBuffer ReadBytes() const;
    ByteBuffer ReadBytes(SizeType n) const;

    void SetBytes(const ByteBuffer& byteBuffer);
    void SetBytes(SizeType count, const void* data);

#define FBOM_TYPE_FUNCTIONS(typeName, cType)                                                                          \
    bool Is##typeName() const                                                                                         \
    {                                                                                                                 \
        return FBOMDataTypeOps<FBOMData, cType, FBOM##typeName> { *this }.IsType();                                   \
    }                                                                                                                 \
                                                                                                                      \
    FBOMResult Read(cType* out) const                                                                                 \
    {                                                                                                                 \
        return FBOMDataTypeOps<FBOMData, cType, FBOM##typeName> { *this }.Read(out);                                  \
    }                                                                                                                 \
                                                                                                                      \
    template <class T>                                                                                                \
    FBOMResult Read##typeName(T* out) const                                                                           \
    {                                                                                                                 \
        return FBOMDataTypeOps<FBOMData, cType, FBOM##typeName> { *this }.Read(out);                                  \
    }                                                                                                                 \
                                                                                                                      \
    static FBOMData From##typeName(const cType& value, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)          \
    {                                                                                                                 \
        return FBOMDataTypeOps<FBOMData, cType, FBOM##typeName>::From(value, flags);                                  \
    }                                                                                                                 \
                                                                                                                      \
    explicit FBOMData(const cType& value, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)                       \
        : m_type(FBOM##typeName()),                                                                                   \
          m_flags(flags)                                                                                              \
    {                                                                                                                 \
        static_assert(std::is_standard_layout_v<cType>, "Type " #cType " must be standard layout");                   \
                                                                                                                      \
        HYP_CORE_ASSERT(sizeof(cType) == m_type.size, "sizeof(" #cType ") must be equal to FBOM" #typeName "::size"); \
                                                                                                                      \
        SetBytes(sizeof(cType), &value);                                                                              \
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

    template <int TStringType>
    HYP_FORCE_INLINE FBOMResult ReadString(containers::String<TStringType>& str) const
    {
        static_assert(TStringType == int(StringType::ANSI) || TStringType == int(StringType::UTF8), "String type must be ANSI or UTF8");

        FBOM_ASSERT(IsString(), "Type mismatch (expected String)");

        const SizeType totalSize = TotalSize();
        char* ch = new char[totalSize + 1];

        ReadBytes(totalSize, ch);
        ch[totalSize] = '\0';

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
    HYP_FORCE_INLINE static FBOMData FromString(const containers::String<TStringType>& str)
    {
        return FromString(StringView<TStringType>(str));
    }

    template <int TStringType>
    explicit FBOMData(const StringView<TStringType>& str)
        : FBOMData(FromString(str))
    {
    }

    template <int TStringType>
    explicit FBOMData(const containers::String<TStringType>& str)
        : FBOMData(FromString(str))
    {
    }

#pragma endregion String

#pragma region JSON
    FBOMResult ToJSON(FBOMLoadContext& context, json::JSONValue& outJson) const;

    static FBOMData FromJSON(const json::JSONValue& jsonValue);

    explicit FBOMData(const json::JSONValue& jsonValue);
#pragma endregion JSON

#pragma region ByteBuffer
    HYP_FORCE_INLINE bool IsByteBuffer() const
    {
        return m_type.IsOrExtends(FBOMByteBuffer());
    }

    HYP_FORCE_INLINE FBOMResult ReadByteBuffer(ByteBuffer& byteBuffer) const
    {
        FBOM_ASSERT(IsByteBuffer(), "Type mismatch (expected ByteBuffer)");

        byteBuffer = m_bytes;

        FBOM_RETURN_OK;
    }

    static FBOMData FromByteBuffer(const ByteBuffer& byteBuffer, EnumFlags<FBOMDataFlags> flags = FBOMDataFlags::NONE)
    {
        FBOMData data(FBOMByteBuffer(byteBuffer.Size()), flags);
        data.SetBytes(byteBuffer.Size(), byteBuffer.Data());

        return data;
    }

#pragma endregion ByteBuffer

#pragma region Struct

    template <class T>
    HYP_FORCE_INLINE bool IsStruct() const
    {
        return m_type.IsOrExtends(FBOMStruct(TypeNameWithoutNamespace<NormalizedType<T>>(), -1, TypeId::ForType<NormalizedType<T>>()), /* allowUnbounded */ true, /* allowVoidTypeId */ true);
    }

    HYP_FORCE_INLINE bool IsStruct(const char* typeName, TypeId typeId) const
    {
        return m_type.IsOrExtends(FBOMStruct(typeName, -1, typeId), /* allowUnbounded */ true, /* allowVoidTypeId */ true);
    }

    HYP_FORCE_INLINE bool IsStruct(const char* typeName, SizeType size, TypeId typeId) const
    {
        return m_type.IsOrExtends(FBOMStruct(typeName, size, typeId), /* allowUnbounded */ true, /* allowVoidTypeId */ true);
    }

    HYP_FORCE_INLINE FBOMResult ReadStruct(const char* typeName, SizeType size, TypeId typeId, void* out) const
    {
        HYP_CORE_ASSERT(out != nullptr);

        FBOM_ASSERT(IsStruct(typeName, size, typeId), "Object is not a struct or not struct of requested size");

        ReadBytes(size, out);

        FBOM_RETURN_OK;
    }

    template <class T, bool CompileTimeChecked = true>
    HYP_FORCE_INLINE FBOMResult ReadStruct(T* out) const
    {
        AssertStaticMsgCond(CompileTimeChecked, isPodType<T>, "T must be POD to use ReadStruct()");

        return ReadStruct(TypeNameWithoutNamespace<NormalizedType<T>>().Data(), sizeof(NormalizedType<T>), TypeId::ForType<NormalizedType<T>>(), out);
    }

    template <class T, bool CompileTimeChecked = true>
    HYP_FORCE_INLINE T ReadStruct() const
    {
        AssertStaticMsgCond(CompileTimeChecked, isPodType<T>, "T must be POD to use ReadStruct()");

        ValueStorage<NormalizedType<T>> resultStorage;

        if (FBOMResult err = ReadStruct(TypeNameWithoutNamespace<NormalizedType<T>>().Data(), sizeof(NormalizedType<T>), TypeId::ForType<NormalizedType<T>>(), resultStorage.GetPointer()))
        {
            HYP_FAIL("Failed to read struct of type %s: %s", TypeNameWithoutNamespace<NormalizedType<T>>().Data(), *err.message);
        }

        return resultStorage.Get();
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

    // template <class T, typename = std::enable_if_t< FBOMStruct::isValidStructType< NormalizedType<T> > && std::is_class_v< NormalizedType<T> > > >
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
    HYP_FORCE_INLINE bool IsSequenceMatching(const FBOMType& heldType, SizeType numItems) const
    {
        return m_type.IsOrExtends(FBOMSequence(heldType, numItems));
    }

    // does the array size equal byteSize bytes?
    HYP_FORCE_INLINE bool IsSequenceOfByteSize(SizeType byteSize) const
    {
        return m_type.IsOrExtends(FBOMSequence(FBOMUInt8(), byteSize));
    }

    /*! \brief If type is an sequence, return the number of elements,
        assuming the sequence contains the given type. Note, sequence could
        contain another type, and still a result will be returned.

        If type is /not/ an sequence, return zero. */
    HYP_FORCE_INLINE SizeType NumElements(const FBOMType& heldType) const
    {
        if (!IsSequence())
        {
            return 0;
        }

        const SizeType heldTypeSize = heldType.size;

        if (heldTypeSize == 0)
        {
            return 0;
        }

        return TotalSize() / heldTypeSize;
    }

    // count is number of ELEMENTS
    HYP_FORCE_INLINE FBOMResult ReadElements(const FBOMType& heldType, SizeType numItems, void* out) const
    {
        HYP_CORE_ASSERT(out != nullptr);

        FBOM_ASSERT(IsSequence(), "Object is not an sequence");

        ReadBytes(heldType.size * numItems, out);

        FBOM_RETURN_OK;
    }

#pragma endregion Sequence

#pragma region Object
    HYP_FORCE_INLINE bool IsObject() const
    {
        return m_type.IsOrExtends(FBOMBaseObjectType());
    }

    FBOMResult ReadObject(FBOMLoadContext& context, FBOMObject& outObject, bool deserializeObject = true) const;

    static FBOMData FromObject(const FBOMObject& object, bool keepNativeObject = true);
    static FBOMData FromObject(FBOMObject&& object, bool keepNativeObject = true);

#pragma endregion Object

#pragma region Array
    HYP_FORCE_INLINE bool IsArray() const
    {
        return m_type.IsOrExtends(FBOMArrayType());
    }

    FBOMResult ReadArray(FBOMLoadContext& context, FBOMArray& outArray) const;

    static FBOMData FromArray(const FBOMArray& array);
#pragma endregion Array

    HYP_FORCE_INLINE FBOMResult ReadBytes(SizeType count, ByteBuffer& out) const
    {
        FBOM_ASSERT(count <= m_bytes.Size(), "Attempt to read past max size of object");

        out = ByteBuffer(count, m_bytes.Data());

        FBOM_RETURN_OK;
    }

    HYP_FORCE_INLINE FBOMResult ReadAsType(const FBOMType& readType, void* out) const
    {
        HYP_CORE_ASSERT(out != nullptr);

        FBOM_ASSERT(m_type.IsOrExtends(readType), "Type mismatch");

        ReadBytes(readType.size, out);

        FBOM_RETURN_OK;
    }

    FBOMResult Visit(FBOMWriter* writer, ByteWriter* out, EnumFlags<FBOMDataAttributes> attributes = FBOMDataAttributes::NONE) const
    {
        return Visit(GetUniqueID(), writer, out, attributes);
    }

    virtual FBOMResult Visit(UniqueId id, FBOMWriter* writer, ByteWriter* out, EnumFlags<FBOMDataAttributes> attributes = FBOMDataAttributes::NONE) const override;

    virtual String ToString(bool deep = true) const override;
    virtual UniqueId GetUniqueID() const override;
    virtual HashCode GetHashCode() const override;

private:
    ByteBuffer m_bytes;
    FBOMType m_type;

    EnumFlags<FBOMDataFlags> m_flags;

    RC<HypData> m_deserializedObject;
};

} // namespace serialization
} // namespace hyperion

#undef FBOM_RETURN_OK
#undef FBOM_ASSERT
