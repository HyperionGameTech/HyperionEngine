/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FBOM_OBJECT_HPP
#define HYPERION_FBOM_OBJECT_HPP

#include <core/memory/Any.hpp>

#include <core/containers/String.hpp>
#include <core/containers/Array.hpp>
#include <core/containers/FlatMap.hpp>

#include <core/utilities/Optional.hpp>
#include <core/utilities/UniqueID.hpp>
#include <core/utilities/StringView.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/UUID.hpp>

#include <core/memory/ByteBuffer.hpp>

#include <asset/serialization/fbom/FBOMBaseTypes.hpp>
#include <asset/serialization/fbom/FBOMData.hpp>
#include <asset/serialization/fbom/FBOMInterfaces.hpp>
#include <asset/serialization/fbom/FBOMMarshaler.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Types.hpp>
#include <Constants.hpp>

#include <type_traits>

namespace hyperion {

class HypClass;
struct HypData;

enum class FBOMObjectSerializeFlags : uint32
{
    NONE        = 0x0,
    EXTERNAL    = 0x1,
    KEEP_UNIQUE = 0x2
};

HYP_MAKE_ENUM_FLAGS(FBOMObjectSerializeFlags)

namespace fbom {

class FBOMNodeHolder;
class FBOMMarshalerBase;

namespace detail {

template <class T, class T2 = void>
struct FBOMObjectSerialize_Impl;

} // namespace detail

struct FBOMExternalObjectInfo
{
    UUID    library_id = UUID::Invalid();
    uint32  index = ~0u;

    HYP_FORCE_INLINE UniqueID GetUniqueID() const
        { return UniqueID(GetHashCode()); }

    HYP_FORCE_INLINE bool IsLinked() const
        { return library_id != UUID::Invalid() && index != ~0u; }

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(library_id);
        hc.Add(index);

        return hc;
    }
};

class HYP_API FBOMObject final : public IFBOMSerializable
{
public:
    FBOMType                            m_object_type;
    FBOMNodeHolder                      *nodes;
    FlatMap<ANSIString, FBOMData>       properties;
    RC<HypData>                         m_deserialized_object;
    Optional<FBOMExternalObjectInfo>    m_external_info;
    UniqueID                            m_unique_id;

    FBOMObject();
    FBOMObject(const FBOMType &loader_type);
    FBOMObject(const FBOMObject &other);
    FBOMObject &operator=(const FBOMObject &other);
    FBOMObject(FBOMObject &&other) noexcept;
    FBOMObject &operator=(FBOMObject &&other) noexcept;
    virtual ~FBOMObject() override;

    HYP_FORCE_INLINE bool IsExternal() const
        { return m_external_info.HasValue(); }

    HYP_FORCE_INLINE void SetIsExternal(bool is_external)
    {
        if (is_external) {
            m_external_info.Set({ });
        } else {
            m_external_info.Unset();
        }
    }

    HYP_FORCE_INLINE FBOMExternalObjectInfo *GetExternalObjectInfo()
        { return IsExternal() ? m_external_info.TryGet() : nullptr; }

    HYP_FORCE_INLINE const FBOMExternalObjectInfo *GetExternalObjectInfo() const
        { return IsExternal() ? m_external_info.TryGet() : nullptr; }

    HYP_FORCE_INLINE const FBOMType &GetType() const
        { return m_object_type; }

    HYP_FORCE_INLINE const FlatMap<ANSIString, FBOMData> &GetProperties() const
        { return properties; }

    bool HasProperty(ANSIStringView key) const;

    const FBOMData &GetProperty(ANSIStringView key) const;

    FBOMObject &SetProperty(ANSIStringView key, const FBOMData &data);
    FBOMObject &SetProperty(ANSIStringView key, FBOMData &&data);
    FBOMObject &SetProperty(ANSIStringView key, const FBOMType &type, SizeType size, const void *bytes);

    HYP_FORCE_INLINE FBOMObject &SetProperty(ANSIStringView key, const UTF8StringView &str)
        { return SetProperty(key, FBOMData::FromString(str)); }

    HYP_FORCE_INLINE FBOMObject &SetProperty(ANSIStringView key, const ANSIStringView &str)
        { return SetProperty(key, FBOMData::FromString(str)); }

    HYP_FORCE_INLINE FBOMObject &SetProperty(ANSIStringView key, bool value)
        { return SetProperty(key, FBOMBool(), sizeof(uint8) /* bool = 1 byte */, &value); }

    HYP_FORCE_INLINE FBOMObject &SetProperty(ANSIStringView key, uint8 value)
        { return SetProperty(key, FBOMUInt8(), sizeof(uint8), &value); }

    HYP_FORCE_INLINE FBOMObject &SetProperty(ANSIStringView key, uint16 value)
        { return SetProperty(key, FBOMUInt16(), sizeof(uint16), &value); }

    HYP_FORCE_INLINE FBOMObject &SetProperty(ANSIStringView key, uint32 value)
        { return SetProperty(key, FBOMUInt32(), sizeof(uint32), &value); }

    HYP_FORCE_INLINE FBOMObject &SetProperty(ANSIStringView key, uint64 value)
        { return SetProperty(key, FBOMUInt64(), sizeof(uint64), &value); }

    HYP_FORCE_INLINE FBOMObject &SetProperty(ANSIStringView key, int8 value)
        { return SetProperty(key, FBOMInt8(), sizeof(int8), &value); }

    HYP_FORCE_INLINE FBOMObject &SetProperty(ANSIStringView key, int16 value)
        { return SetProperty(key, FBOMInt16(), sizeof(int16), &value); }

    HYP_FORCE_INLINE FBOMObject &SetProperty(ANSIStringView key, int32 value)
        { return SetProperty(key, FBOMInt32(), sizeof(int32), &value); }

    HYP_FORCE_INLINE FBOMObject &SetProperty(ANSIStringView key, int64 value)
        { return SetProperty(key, FBOMInt64(), sizeof(int64), &value); }

    HYP_FORCE_INLINE FBOMObject &SetProperty(ANSIStringView key, char value)
        { return SetProperty(key, FBOMChar(), sizeof(char), &value); }

    HYP_FORCE_INLINE FBOMObject &SetProperty(ANSIStringView key, float value)
        { return SetProperty(key, FBOMFloat(), sizeof(float), &value); }

    HYP_FORCE_INLINE FBOMObject &SetProperty(ANSIStringView key, double value)
        { return SetProperty(key, FBOMDouble(), sizeof(double), &value); }

    const FBOMData &operator[](ANSIStringView key) const;

    /*! \brief Add a child object to this object node.
        @param object The child object to add
        @param flags Options to use for loading */
    template <class T>
    typename std::enable_if_t<!std::is_same_v<FBOMObject, NormalizedType<T>>, FBOMResult>
    AddChild(const T &in, EnumFlags<FBOMObjectSerializeFlags> flags = FBOMObjectSerializeFlags::NONE)
    {
        FBOMObject subobject;

        if (FBOMResult err = Serialize(in, subobject, flags)) {
            return err;
        }

        AddChild(std::move(subobject));

        return FBOMResult::FBOM_OK;
    }

    void AddChild(FBOMObject &&object);

    FBOMResult Visit(FBOMWriter *writer, ByteWriter *out, EnumFlags<FBOMDataAttributes> attributes = FBOMDataAttributes::NONE) const
        { return Visit(GetUniqueID(), writer, out, attributes); }

    virtual FBOMResult Visit(UniqueID id, FBOMWriter *writer, ByteWriter *out, EnumFlags<FBOMDataAttributes> attributes = FBOMDataAttributes::NONE) const override;

    virtual String ToString(bool deep = true) const override;
    
    virtual UniqueID GetUniqueID() const override
        { return m_unique_id;  }

    virtual HashCode GetHashCode() const override;

    template <class T, typename = std::enable_if_t< !std::is_same_v< FBOMObject, NormalizedType<T> > > >
    static FBOMResult Serialize(const T &in, FBOMObject &out_object, EnumFlags<FBOMObjectSerializeFlags> flags = FBOMObjectSerializeFlags::NONE)
    {
        return detail::FBOMObjectSerialize_Impl<T>{}.template Serialize<HypData>(in, out_object, flags);
    }

    template <class T, typename = std::enable_if_t< !std::is_same_v< FBOMObject, NormalizedType<T> > > >
    static FBOMObject Serialize(const T &in, EnumFlags<FBOMObjectSerializeFlags> flags = FBOMObjectSerializeFlags::NONE)
    {
        FBOMObject object;

        if (FBOMResult err = Serialize(in, object, flags)) {
            HYP_FAIL("Failed to serialize object: %s", *err.message);
        }

        return object;
    }

    static FBOMResult Deserialize(TypeID type_id, const FBOMObject &in, HypData &out);

    template <class T, typename = std::enable_if_t< !std::is_same_v< FBOMObject, NormalizedType<T> > > >
    static FBOMResult Deserialize(const FBOMObject &in, HypData &out)
    {
        return detail::FBOMObjectSerialize_Impl<T>{}.Deserialize(in, out);
    }

    template <class T, typename = std::enable_if_t< !std::is_same_v< FBOMObject, NormalizedType<T> > > >
    HYP_FORCE_INLINE static bool HasMarshal()
        { return GetMarshal<T>() != nullptr; }
    
    static FBOMMarshalerBase *GetMarshal(TypeID type_id);

    template <class T>
    HYP_FORCE_INLINE static FBOMMarshalerBase *GetMarshal()
        { return GetMarshal(TypeID::ForType<T>()); }

    /*! \brief Returns the associated HypClass for this object type, if applicable.
     *  The type must be registered using the "HYP_CLASS" macro.
     *  
     *  If this object's FBOMType has no native TypeID (e.g it is a FBOM-only type like `seq`), or if
     *  no HypClass has been registered for the type, nullptr will be returned. */
    HYP_FORCE_INLINE const HypClass *GetHypClass() const
        { return m_object_type.GetHypClass(); }
};

class FBOMNodeHolder : public Array<FBOMObject>
{
public:
    FBOMNodeHolder()
        : Array<FBOMObject>()
    {
    }

    FBOMNodeHolder(const Array<FBOMObject> &other)
        : Array<FBOMObject>(other)
    {
    }

    FBOMNodeHolder &operator=(const Array<FBOMObject> &other)
    {
        Array<FBOMObject>::operator=(other);

        return *this;
    }

    FBOMNodeHolder(Array<FBOMObject> &&other) noexcept
        : Array<FBOMObject>(std::move(other))
    {
    }

    FBOMNodeHolder &operator=(Array<FBOMObject> &&other) noexcept
    {
        Array<FBOMObject>::operator=(std::move(other));

        return *this;
    }

    ~FBOMNodeHolder() = default;

    // HYP_DEF_STL_BEGIN_END(
    //     reinterpret_cast<typename Array<FBOMObject>::ValueType *>(&Array<FBOMObject>::m_buffer[Array<FBOMObject>::m_start_offset]),
    //     reinterpret_cast<typename Array<FBOMObject>::ValueType *>(&Array<FBOMObject>::m_buffer[Array<FBOMObject>::m_size])
    // )
};

namespace detail {

template <class T>
struct FBOMObjectSerialize_Impl<T, std::enable_if_t< !std::is_same_v< FBOMObject, NormalizedType<T> > > >
{
    template <class HypDataType>
    FBOMResult Serialize(const T &in, FBOMObject &out_object, EnumFlags<FBOMObjectSerializeFlags> flags = FBOMObjectSerializeFlags::NONE)
    {
        HYP_SCOPE;

        FBOMMarshalerBase *marshal = FBOMObject::GetMarshal<T>();
        
        if (!marshal) {
            return { FBOMResult::FBOM_ERR, "No registered marshal class for type" };
        }
        
        ANSIString external_object_key;

        out_object = FBOMObject(marshal->GetObjectType());

        if (FBOMResult err = marshal->Serialize(ConstAnyRef(in), out_object)) {
            return err;
        }

        if constexpr (HYP_HAS_METHOD(NormalizedType<T>, GetHashCode)) {
            if (flags & FBOMObjectSerializeFlags::KEEP_UNIQUE) {
                out_object.m_unique_id = UniqueID::Generate();
            } else {
                const HashCode hash_code = HashCode::GetHashCode(TypeID::ForType<T>()).Add(HashCode::GetHashCode(in));

                out_object.m_unique_id = UniqueID(hash_code);
            }
        } else {
            out_object.m_unique_id = UniqueID::Generate();
        }

        if (flags & FBOMObjectSerializeFlags::EXTERNAL) {
            out_object.SetIsExternal(true);
        }

        return FBOMResult::FBOM_OK;
    }

    FBOMResult Deserialize(const FBOMObject &in, HypData &out)
    {
        FBOMMarshalerBase *marshal = FBOMObject::GetMarshal<T>();
        
        if (!marshal) {
            return { FBOMResult::FBOM_ERR, "No registered marshal class for type" };
        }

        if (FBOMResult err = marshal->Deserialize(in, out)) {
            HYP_FAIL("Failed to deserialize object: %s", *err.message);
        }

        return { FBOMResult::FBOM_OK };
    }
};

} // namespace detail

} // namespace fbom
} // namespace hyperion

#endif
