/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/memory/Any.hpp>

#include <core/containers/String.hpp>
#include <core/containers/Array.hpp>
#include <core/containers/FlatMap.hpp>

#include <core/utilities/Optional.hpp>
#include <core/utilities/StringView.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/Uuid.hpp>
#include <core/reflection/TypeInfoFwd.hpp>

#include <core/memory/ByteBuffer.hpp>

#include <core/serialization/fbom/FBOMBaseTypes.hpp>
#include <core/serialization/fbom/FBOMData.hpp>
#include <core/serialization/fbom/FBOMInterfaces.hpp>
#include <core/serialization/fbom/FBOMMarshaler.hpp>
#include <core/serialization/fbom/util/UniqueId.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/Types.hpp>
#include <core/Constants.hpp>

#include <type_traits>

namespace hyperion {

class Class;
struct HypData;

HYP_API extern TypeId GetTypeIdForClass(const Class* cls);

enum class FBOMObjectSerializeFlags : uint32
{
    NONE = 0x0,
    EXTERNAL = 0x1,
    KEEP_UNIQUE = 0x2
};

HYP_MAKE_ENUM_FLAGS(FBOMObjectSerializeFlags)

namespace serialization {

class FBOMNodeHolder;
class FBOMMarshalerBase;

template <class T, class T2 = void>
struct FBOMObjectSerialize_Impl;

struct FBOMExternalObjectInfo
{
    Uuid libraryId = Uuid::Invalid();
    uint32 index = ~0u;

    HYP_FORCE_INLINE UniqueId GetUniqueID() const
    {
        return UniqueId(GetHashCode());
    }

    HYP_FORCE_INLINE bool IsLinked() const
    {
        return libraryId != Uuid::Invalid() && index != ~0u;
    }

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(libraryId);
        hc.Add(index);

        return hc;
    }
};

class HYP_API FBOMObject final : public FBOMSerializableBase
{
public:
    friend class FBOMReader;
    friend class FBOMWriter;

    FBOMType m_objectType;
    Array<FBOMObject, DynamicAllocator> m_children;
    FlatMap<ANSIString, FBOMData> properties;
    RC<HypData> m_deserializedObject;
    Optional<FBOMExternalObjectInfo> m_externalInfo;
    UniqueId m_uniqueId;

    FBOMObject();
    FBOMObject(const FBOMType& loaderType);
    FBOMObject(const FBOMObject& other);
    FBOMObject& operator=(const FBOMObject& other);
    FBOMObject(FBOMObject&& other) noexcept;
    FBOMObject& operator=(FBOMObject&& other) noexcept;
    virtual ~FBOMObject() override;

    HYP_FORCE_INLINE bool IsExternal() const
    {
        return m_externalInfo.HasValue();
    }

    HYP_FORCE_INLINE void SetIsExternal(bool isExternal)
    {
        if (isExternal)
        {
            m_externalInfo.Set({});
        }
        else
        {
            m_externalInfo.Unset();
        }
    }

    HYP_FORCE_INLINE FBOMExternalObjectInfo* GetExternalObjectInfo()
    {
        return IsExternal() ? m_externalInfo.TryGet() : nullptr;
    }

    HYP_FORCE_INLINE const FBOMExternalObjectInfo* GetExternalObjectInfo() const
    {
        return IsExternal() ? m_externalInfo.TryGet() : nullptr;
    }

    HYP_FORCE_INLINE const FBOMType& GetType() const
    {
        return m_objectType;
    }

    HYP_FORCE_INLINE const Array<FBOMObject, DynamicAllocator>& GetChildren() const
    {
        return m_children;
    }

    HYP_FORCE_INLINE FlatMap<ANSIString, FBOMData>& GetProperties()
    {
        return properties;
    }

    HYP_FORCE_INLINE const FlatMap<ANSIString, FBOMData>& GetProperties() const
    {
        return properties;
    }

    bool HasProperty(ANSIStringView key) const;

    const FBOMData& GetProperty(ANSIStringView key) const;

    FBOMObject& SetProperty(ANSIStringView key, const FBOMData& data);
    FBOMObject& SetProperty(ANSIStringView key, FBOMData&& data);
    FBOMObject& SetProperty(ANSIStringView key, const FBOMType& type, SizeType size, const void* bytes);

    HYP_FORCE_INLINE FBOMObject& SetProperty(ANSIStringView key, const UTF8StringView& str)
    {
        return SetProperty(key, FBOMData::FromString(str));
    }

    HYP_FORCE_INLINE FBOMObject& SetProperty(ANSIStringView key, const ANSIStringView& str)
    {
        return SetProperty(key, FBOMData::FromString(str));
    }

    HYP_FORCE_INLINE FBOMObject& SetProperty(ANSIStringView key, bool value)
    {
        return SetProperty(key, FBOMBool(), sizeof(uint8) /* bool = 1 byte */, &value);
    }

    HYP_FORCE_INLINE FBOMObject& SetProperty(ANSIStringView key, uint8 value)
    {
        return SetProperty(key, FBOMUInt8(), sizeof(uint8), &value);
    }

    HYP_FORCE_INLINE FBOMObject& SetProperty(ANSIStringView key, uint16 value)
    {
        return SetProperty(key, FBOMUInt16(), sizeof(uint16), &value);
    }

    HYP_FORCE_INLINE FBOMObject& SetProperty(ANSIStringView key, uint32 value)
    {
        return SetProperty(key, FBOMUInt32(), sizeof(uint32), &value);
    }

    HYP_FORCE_INLINE FBOMObject& SetProperty(ANSIStringView key, uint64 value)
    {
        return SetProperty(key, FBOMUInt64(), sizeof(uint64), &value);
    }

    HYP_FORCE_INLINE FBOMObject& SetProperty(ANSIStringView key, int8 value)
    {
        return SetProperty(key, FBOMInt8(), sizeof(int8), &value);
    }

    HYP_FORCE_INLINE FBOMObject& SetProperty(ANSIStringView key, int16 value)
    {
        return SetProperty(key, FBOMInt16(), sizeof(int16), &value);
    }

    HYP_FORCE_INLINE FBOMObject& SetProperty(ANSIStringView key, int32 value)
    {
        return SetProperty(key, FBOMInt32(), sizeof(int32), &value);
    }

    HYP_FORCE_INLINE FBOMObject& SetProperty(ANSIStringView key, int64 value)
    {
        return SetProperty(key, FBOMInt64(), sizeof(int64), &value);
    }

    HYP_FORCE_INLINE FBOMObject& SetProperty(ANSIStringView key, char value)
    {
        return SetProperty(key, FBOMChar(), sizeof(char), &value);
    }

    HYP_FORCE_INLINE FBOMObject& SetProperty(ANSIStringView key, float value)
    {
        return SetProperty(key, FBOMFloat(), sizeof(float), &value);
    }

    HYP_FORCE_INLINE FBOMObject& SetProperty(ANSIStringView key, double value)
    {
        return SetProperty(key, FBOMDouble(), sizeof(double), &value);
    }

    const FBOMData& operator[](ANSIStringView key) const;

    /*! \brief Add a child object to this object node.
        @param object The child object to add
        @param flags Options to use for loading */
    template <class T>
    typename std::enable_if_t<!std::is_same_v<FBOMObject, NormalizedType<T>>, FBOMResult>
    AddChild(const T& in, EnumFlags<FBOMObjectSerializeFlags> flags = FBOMObjectSerializeFlags::NONE)
    {
        FBOMObject subobject;

        if (FBOMResult err = Serialize(in, subobject, flags))
        {
            return err;
        }

        AddChild(std::move(subobject));

        return FBOMResult::FBOM_OK;
    }

    void AddChild(FBOMObject&& object);

    FBOMResult Visit(FBOMWriter* writer, ByteWriter* out, EnumFlags<FBOMDataAttributes> attributes = FBOMDataAttributes::NONE) const
    {
        return Visit(GetUniqueID(), writer, out, attributes);
    }

    virtual FBOMResult Visit(UniqueId id, FBOMWriter* writer, ByteWriter* out, EnumFlags<FBOMDataAttributes> attributes = FBOMDataAttributes::NONE) const override;

    virtual String ToString(bool deep = true) const override;

    virtual UniqueId GetUniqueID() const override
    {
        return m_uniqueId;
    }

    virtual HashCode GetHashCode() const override;

    template <class T, typename = std::enable_if_t<!std::is_same_v<FBOMObject, NormalizedType<T>>>>
    static FBOMResult Serialize(const T& in, FBOMObject& outObject, EnumFlags<FBOMObjectSerializeFlags> flags = FBOMObjectSerializeFlags::NONE)
    {
        return FBOMObjectSerialize_Impl<T> {}.template Serialize<HypData>(in, outObject, flags);
    }

    template <class T, typename = std::enable_if_t<!std::is_same_v<FBOMObject, NormalizedType<T>>>>
    static FBOMObject Serialize(const T& in, EnumFlags<FBOMObjectSerializeFlags> flags = FBOMObjectSerializeFlags::NONE)
    {
        FBOMObject object;

        if (FBOMResult err = Serialize(in, object, flags))
        {
            HYP_FAIL("Failed to serialize object: %s", *err.message);
        }

        return object;
    }

    static FBOMResult Deserialize(FBOMLoadContext& context, TypeId typeId, const FBOMObject& in, HypData& out);

    template <class T, typename = std::enable_if_t<!std::is_same_v<FBOMObject, NormalizedType<T>>>>
    static FBOMResult Deserialize(FBOMLoadContext& context, const FBOMObject& in, HypData& out)
    {
        return FBOMObjectSerialize_Impl<T> {}.Deserialize(context, in, out);
    }

    template <class T, typename = std::enable_if_t<!std::is_same_v<FBOMObject, NormalizedType<T>>>>
    HYP_FORCE_INLINE static bool HasMarshal()
    {
        return GetMarshal<T>() != nullptr;
    }

    static FBOMMarshalerBase* GetMarshal(TypeId typeId);

    template <class T>
    HYP_FORCE_INLINE static FBOMMarshalerBase* GetMarshal()
    {
        return GetMarshal(TypeId::ForType<T>());
    }

    /*! \brief Returns the associated Class for this object type, if applicable.
     *  The type must be registered using the "HYP_CLASS" macro.
     *
     *  If this object's FBOMType has no native TypeId (e.g it is a FBOM-only type like `seq`), or if
     *  no Class has been registered for the type, nullptr will be returned. */
    HYP_FORCE_INLINE const Class* GetClass() const
    {
        return m_objectType.GetClass();
    }
};

class FBOMNodeHolder : public Array<FBOMObject>
{
public:
    FBOMNodeHolder()
        : Array<FBOMObject>()
    {
    }

    FBOMNodeHolder(const Array<FBOMObject>& other)
        : Array<FBOMObject>(other)
    {
    }

    FBOMNodeHolder& operator=(const Array<FBOMObject>& other)
    {
        Array<FBOMObject>::operator=(other);

        return *this;
    }

    FBOMNodeHolder(Array<FBOMObject>&& other) noexcept
        : Array<FBOMObject>(std::move(other))
    {
    }

    FBOMNodeHolder& operator=(Array<FBOMObject>&& other) noexcept
    {
        Array<FBOMObject>::operator=(std::move(other));

        return *this;
    }

    ~FBOMNodeHolder() = default;
};

template <class T>
struct FBOMObjectSerialize_Impl<T, std::enable_if_t<!std::is_same_v<FBOMObject, NormalizedType<T>>>>
{
    template <class HypDataType>
    FBOMResult Serialize(const T& in, FBOMObject& outObject, EnumFlags<FBOMObjectSerializeFlags> flags = FBOMObjectSerializeFlags::NONE)
    {
        HYP_SCOPE;

        FBOMMarshalerBase* marshal = nullptr;

        ConstAnyRef ref = ConstAnyRef(in);

        /// @TODO: Move Marshal to Class.

        if constexpr (std::is_base_of_v<ObjectBase, NormalizedType<T>>)
        {
            const Class* instanceClass = in.InstanceClass();
            HYP_CORE_ASSERT(instanceClass != nullptr);

            marshal = FBOMObject::GetMarshal(GetTypeIdForClass(instanceClass));
            ref = ConstAnyRef(&TypeInfo_ForClass(instanceClass), &in);
        }
        else
        {
            marshal = FBOMObject::GetMarshal<T>();
        }

        if (!marshal)
        {
            return { FBOMResult::FBOM_ERR, "No registered marshal class for type" };
        }

        ANSIString externalObjectKey;

        outObject = FBOMObject(marshal->GetObjectType());

        if (FBOMResult err = marshal->Serialize(ref, outObject))
        {
            return err;
        }

        if constexpr (HYP_HAS_METHOD(NormalizedType<T>, GetHashCode))
        {
            /// @FIXME: Will have issue with derived types if method is not virtual!
            if (flags & FBOMObjectSerializeFlags::KEEP_UNIQUE)
            {
                outObject.m_uniqueId = UniqueId::Generate();
            }
            else
            {
                const HashCode hashCode = HashCode::GetHashCode(ref.GetTypeId()).Add(HashCode::GetHashCode(in));

                outObject.m_uniqueId = UniqueId(hashCode);
            }
        }
        else
        {
            outObject.m_uniqueId = UniqueId::Generate();
        }

        if (flags & FBOMObjectSerializeFlags::EXTERNAL)
        {
            outObject.SetIsExternal(true);
        }

        return FBOMResult::FBOM_OK;
    }

    FBOMResult Deserialize(FBOMLoadContext& context, const FBOMObject& in, HypData& out)
    {
        FBOMMarshalerBase* marshal = FBOMObject::GetMarshal<T>();

        if (!marshal)
        {
            return { FBOMResult::FBOM_ERR, "No registered marshal class for type" };
        }

        if (FBOMResult err = marshal->Deserialize(context, in, out))
        {
            HYP_FAIL("Failed to deserialize object: %s", *err.message);
        }

        return { FBOMResult::FBOM_OK };
    }
};

} // namespace serialization
} // namespace hyperion
