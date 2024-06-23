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
#include <core/utilities/TypeAttributes.hpp>
#include <core/utilities/UUID.hpp>

#include <core/memory/ByteBuffer.hpp>
#include <core/Name.hpp>

#include <asset/serialization/fbom/FBOMBaseTypes.hpp>
#include <asset/serialization/fbom/FBOMData.hpp>
#include <asset/serialization/fbom/FBOMMarshaler.hpp>
#include <asset/serialization/fbom/FBOMDeserializedObject.hpp>
#include <asset/serialization/fbom/FBOMInterfaces.hpp>

#include <Types.hpp>
#include <Constants.hpp>

#include <type_traits>

namespace hyperion {

class HypClass;

enum class FBOMObjectFlags : uint32
{
    NONE        = 0x0,
    EXTERNAL    = 0x1,
    KEEP_UNIQUE = 0x2
};

HYP_MAKE_ENUM_FLAGS(FBOMObjectFlags)

namespace fbom {

class FBOMNodeHolder;

struct FBOMExternalObjectInfo
{
    UUID    library_id = UUID::Invalid();
    uint32  index = ~0u;

    HYP_NODISCARD HYP_FORCE_INLINE
    UniqueID GetUniqueID() const
        { return UniqueID(GetHashCode()); }

    HYP_NODISCARD HYP_FORCE_INLINE
    bool IsLinked() const
        { return library_id != UUID::Invalid() && index != ~0u; }

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(library_id);
        hc.Add(index);

        return hc;
    }
};

class HYP_API FBOMObject : public IFBOMSerializable
{
public:
    FBOMType                            m_object_type;
    FBOMNodeHolder                      *nodes;
    FlatMap<Name, FBOMData>             properties;
    RC<FBOMDeserializedObject>          m_deserialized_object;
    Optional<FBOMExternalObjectInfo>    m_external_info;
    UniqueID                            m_unique_id;

    FBOMObject();
    FBOMObject(const FBOMType &loader_type);
    FBOMObject(const FBOMObject &other);
    FBOMObject &operator=(const FBOMObject &other);
    FBOMObject(FBOMObject &&other) noexcept;
    FBOMObject &operator=(FBOMObject &&other) noexcept;
    virtual ~FBOMObject();

    HYP_NODISCARD HYP_FORCE_INLINE
    bool IsExternal() const
        { return m_external_info.HasValue(); }

    HYP_FORCE_INLINE
    void SetIsExternal(bool is_external)
    {
        if (is_external) {
            m_external_info.Set({ });
        } else {
            m_external_info.Unset();
        }
    }

    HYP_NODISCARD HYP_FORCE_INLINE
    FBOMExternalObjectInfo *GetExternalObjectInfo()
        { return IsExternal() ? m_external_info.TryGet() : nullptr; }

    HYP_NODISCARD HYP_FORCE_INLINE
    const FBOMExternalObjectInfo *GetExternalObjectInfo() const
        { return IsExternal() ? m_external_info.TryGet() : nullptr; }

    HYP_NODISCARD HYP_FORCE_INLINE
    const FBOMType &GetType() const
        { return m_object_type; }

    HYP_NODISCARD HYP_FORCE_INLINE
    const FlatMap<Name, FBOMData> &GetProperties() const
        { return properties; }

    HYP_NODISCARD
    bool HasProperty(WeakName key) const;

    HYP_NODISCARD
    const FBOMData &GetProperty(WeakName key) const;

    FBOMObject &SetProperty(Name key, const FBOMData &data);
    FBOMObject &SetProperty(Name key, FBOMData &&data);
    FBOMObject &SetProperty(Name key, const ByteBuffer &bytes);
    FBOMObject &SetProperty(Name key, const FBOMType &type, ByteBuffer &&byte_buffer);
    FBOMObject &SetProperty(Name key, const FBOMType &type, const ByteBuffer &byte_buffer);
    FBOMObject &SetProperty(Name key, const FBOMType &type, const void *bytes);
    FBOMObject &SetProperty(Name key, const FBOMType &type, SizeType size, const void *bytes);

    HYP_FORCE_INLINE
    FBOMObject &SetProperty(Name key, const UTF8StringView &str)
        { return SetProperty(key, FBOMData::FromString(str)); }

    HYP_FORCE_INLINE
    FBOMObject &SetProperty(Name key, const ANSIStringView &str)
        { return SetProperty(key, FBOMData::FromString(str)); }

    HYP_FORCE_INLINE
    FBOMObject &SetProperty(Name key, bool value)
        { return SetProperty(key, FBOMBool(), sizeof(uint8) /* bool = 1 byte */, &value); }

    HYP_FORCE_INLINE
    FBOMObject &SetProperty(Name key, uint8 value)
        { return SetProperty(key, FBOMByte(), sizeof(uint8), &value); }

    HYP_FORCE_INLINE
    FBOMObject &SetProperty(Name key, uint32 value)
        { return SetProperty(key, FBOMUnsignedInt(), sizeof(uint32), &value); }

    HYP_FORCE_INLINE
    FBOMObject &SetProperty(Name key, uint64 value)
        { return SetProperty(key, FBOMUnsignedLong(), sizeof(uint64), &value); }

    HYP_FORCE_INLINE
    FBOMObject &SetProperty(Name key, int32 value)
        { return SetProperty(key, FBOMInt(), sizeof(int32), &value); }

    HYP_FORCE_INLINE
    FBOMObject &SetProperty(Name key, int64 value)
        { return SetProperty(key, FBOMLong(), sizeof(int64), &value); }

    HYP_FORCE_INLINE
    FBOMObject &SetProperty(Name key, float value)
        { return SetProperty(key, FBOMFloat(), sizeof(float), &value); }

    template <class T, typename = typename std::enable_if_t< !std::is_pointer_v<NormalizedType<T> > && !std::is_fundamental_v<NormalizedType<T> > > >
    HYP_FORCE_INLINE
    FBOMObject &SetProperty(Name key, const T &in)
    {
        FBOMObject subobject;

        if (FBOMResult err = Serialize(in)) {
            AssertThrowMsg(false, "Failed to serialize object: %s", *err.message);

            return *this;
        }

        return SetProperty(key, FBOMData::FromObject(std::move(subobject)));
    }

    HYP_NODISCARD
    const FBOMData &operator[](WeakName key) const;

    /*! \brief Add a child object to this object node.
        @param object The child object to add
        @param flags Options to use for loading */
    template <class T>
    typename std::enable_if_t<!std::is_same_v<FBOMObject, NormalizedType<T>>, FBOMResult>
    AddChild(const T &in, EnumFlags<FBOMObjectFlags> flags = FBOMObjectFlags::NONE)
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

    HYP_NODISCARD
    virtual String ToString(bool deep = true) const override;
    
    HYP_NODISCARD
    virtual UniqueID GetUniqueID() const override
        { return m_unique_id;  }

    HYP_NODISCARD
    virtual HashCode GetHashCode() const override;

    template <class T, typename = std::enable_if_t< !std::is_same_v< FBOMObject, NormalizedType<T> > > >
    static FBOMResult Serialize(const T &in, FBOMObject &out_object, EnumFlags<FBOMObjectFlags> flags = FBOMObjectFlags::NONE)
    {
        FBOMMarshalerBase *marshal = GetMarshal<T>();
        
        if (!marshal) {
            return { FBOMResult::FBOM_ERR, "No registered marshal class for type" };
        }
        
        ANSIString external_object_key;

        out_object = FBOMObject(marshal->GetObjectType());

        // Explicit overload call forces a linker error if the marshal class is not registered.
        if (FBOMResult err = marshal->Serialize(in, out_object)) {
            return err;
        }

        if constexpr (HasGetHashCode<NormalizedType<T>, HashCode>::value) {
            if (flags & FBOMObjectFlags::KEEP_UNIQUE) {
                out_object.m_unique_id = UniqueID::Generate();
            } else {
                const HashCode hash_code = HashCode::GetHashCode(TypeID::ForType<T>()).Add(HashCode::GetHashCode(in));

                out_object.m_unique_id = UniqueID(hash_code);
            }
        } else {
            out_object.m_unique_id = UniqueID::Generate();
        }

        if (flags & FBOMObjectFlags::EXTERNAL) {
            out_object.SetIsExternal(true);
        }

        return FBOMResult::FBOM_OK;
    }

    template <class T, typename = std::enable_if_t< !std::is_same_v< FBOMObject, NormalizedType<T> > > >
    static FBOMObject Serialize(const T &in, EnumFlags<FBOMObjectFlags> flags = FBOMObjectFlags::NONE)
    {
        FBOMObject object;

        if (FBOMResult err = Serialize(in, object, flags)) {
            HYP_FAIL("Failed to serialize object: %s", *err.message);
        }

        return object;
    }

    static FBOMResult Deserialize(const TypeAttributes &type_attributes, const FBOMObject &in, FBOMDeserializedObject &out);

    template <class T, typename = std::enable_if_t< !std::is_same_v< FBOMObject, NormalizedType<T> > > >
    static FBOMResult Deserialize(const FBOMObject &in, FBOMDeserializedObject &out)
    {
        FBOMMarshalerBase *marshal = GetMarshal<T>();
        
        if (!marshal) {
            return { FBOMResult::FBOM_ERR, "No registered marshal class for type" };
        }

        // Explicit overload call forces a linker error if the marshal class is not registered.
        if (FBOMResult err = marshal->Deserialize(in, out.m_value)) {
            HYP_FAIL("Failed to deserialize object: %s", *err.message);
        }

        return result;
    }

    template <class T, typename = std::enable_if_t< !std::is_same_v< FBOMObject, NormalizedType<T> > > >
    static FBOMDeserializedObject Deserialize(const FBOMObject &in)
    {
        FBOMDeserializedObject deserialized;

        if (FBOMResult err = Deserialize<T>(in, deserialized)) {
            HYP_FAIL("Failed to deserialize object: %s", *err.message);
        }

        return deserialized;
    }

    template <class T, typename = std::enable_if_t< !std::is_same_v< FBOMObject, NormalizedType<T> > > >
    HYP_NODISCARD HYP_FORCE_INLINE
    static bool HasMarshal()
        { return GetMarshal<T>() != nullptr; }

    /*! \brief Returns the associated HypClass for this object type, if applicable.
     *  The type must be registered using the HYP_DEFINE_CLASS() macro, and a marshal class must exist.
     *  If no HypClass has been registered for the type, nullptr will be returned. */
    HYP_NODISCARD
    const HypClass *GetHypClass() const;

private:
    HYP_NODISCARD
    static FBOMMarshalerBase *GetMarshal(const TypeAttributes &);

    template <class T>
    HYP_NODISCARD HYP_FORCE_INLINE
    static FBOMMarshalerBase *GetMarshal()
        { return GetMarshal(TypeAttributes::ForType<T>()); }
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

} // namespace fbom
} // namespace hyperion

#endif
