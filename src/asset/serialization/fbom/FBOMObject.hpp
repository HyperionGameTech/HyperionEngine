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
    String key;

    explicit operator bool() const
        { return key.Any(); }

    UniqueID GetUniqueID() const
        { return UniqueID(key); }

    HashCode GetHashCode() const
        { return key.GetHashCode(); }
};

class HYP_API FBOMObject : public IFBOMSerializable
{
public:
    FBOMType                            m_object_type;
    FBOMNodeHolder                      *nodes;
    FlatMap<Name, FBOMData>             properties;
    FBOMDeserializedObject              deserialized;
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

    HYP_NODISCARD HYP_FORCE_INLINE
    const String &GetExternalObjectKey() const
        { return IsExternal() ? m_external_info->key : String::empty; }

    HYP_NODISCARD HYP_FORCE_INLINE
    const FBOMExternalObjectInfo *GetExternalObjectInfo() const
        { return IsExternal() ? m_external_info.TryGet() : nullptr; }

    void SetExternalObjectInfo(const FBOMExternalObjectInfo &info)
    {
        if (info) {
            m_external_info.Set(info);
        } else {
            m_external_info.Unset();
        }
    }

    HYP_NODISCARD HYP_FORCE_INLINE
    const FBOMType &GetType() const
        { return m_object_type; }

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
    FBOMObject &SetProperty(Name key, const T &value)
    {
        FBOMMarshaler<NormalizedType<T>> *marshal = GetMarshal<T>();
        AssertThrowMsg(marshal != nullptr, "No registered marshal class for type: %s", TypeNameWithoutNamespace<NormalizedType<T>>().Data());

        FBOMObject object(marshal->GetObjectType());
        object.GenerateUniqueID(value, FBOMObjectFlags::NONE);

        if (FBOMResult err = marshal->FBOMMarshaler<NormalizedType<T>>::Serialize(value, object)) {
            AssertThrowMsg(false, "Failed to serialize object: %s", *err.message);
        }

        return SetProperty(key, FBOMData::FromObject(std::move(object)));
    }

    HYP_NODISCARD
    const FBOMData &operator[](WeakName key) const;

    /*! \brief Add a child object to this object node.
        @param object The child object to add
        @param flags Options to use for loading */
    template <class T>
    typename std::enable_if_t<!std::is_same_v<FBOMObject, NormalizedType<T>>, FBOMResult>
    AddChild(const T &object, EnumFlags<FBOMObjectFlags> flags = FBOMObjectFlags::NONE)
    {
        FBOMMarshaler<NormalizedType<T>> *marshal = GetMarshal<T>();
        AssertThrowMsg(marshal != nullptr, "No registered marshal class for type: %s", TypeNameWithoutNamespace<NormalizedType<T>>().Data());

        String external_object_key;

        FBOMObject out_object(marshal->GetObjectType());
        out_object.GenerateUniqueID(object, flags);

        if (flags & FBOMObjectFlags::EXTERNAL) {
            if constexpr (std::is_base_of_v<BasicObjectBase, NormalizedType<T>>) {
                const String class_name_lower = marshal->GetObjectType().name.ToLower();
                external_object_key = String::ToString(uint64(out_object.GetUniqueID())) + ".hyp" + class_name_lower;
            } else {
                external_object_key = String::ToString(uint64(out_object.GetUniqueID())) + ".hypdata";
            }

            AssertThrow(external_object_key.Any());
        }

        if (FBOMResult err = marshal->FBOMMarshaler<NormalizedType<T>>::Serialize(object, out_object)) {
            return err;
        }

        AddChild(std::move(out_object), external_object_key);

        return { FBOMResult::FBOM_OK };
    }

    void AddChild(FBOMObject &&object, const String &external_object_key = String::empty);

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

    template <class T>
    void GenerateUniqueID(const T &object, EnumFlags<FBOMObjectFlags> flags)
    {
        // m_unique_id = UniqueID::Generate();

        // Set the ID of the object so we can reuse it.
        // TODO: clean this up a bit.
        if constexpr (std::is_base_of_v<BasicObjectBase, NormalizedType<T>>) {
            ID<T> id = object.GetID();

            HashCode hc;
            hc.Add(String(HandleDefinition<T>::class_name).GetHashCode());
            hc.Add(id.value);

            m_unique_id = (flags & FBOMObjectFlags::KEEP_UNIQUE) ? UniqueID::Generate() : UniqueID(hc);
        } else if constexpr (HasGetHashCode<NormalizedType<T>, HashCode>::value) {
            m_unique_id = (flags & FBOMObjectFlags::KEEP_UNIQUE) ? UniqueID::Generate() : UniqueID(object);
        } else {
            m_unique_id = UniqueID::Generate();
        }
    }

    template <class T, typename = std::enable_if_t< !std::is_same_v< FBOMObject, NormalizedType<T> > > >
    static FBOMResult Serialize(const T &in, FBOMObject &out_object)
    {
        FBOMMarshaler<NormalizedType<T>> *marshal = GetMarshal<T>();
        
        if (!marshal) {
            return { FBOMResult::FBOM_ERR, "No registered marshal class for type" };
        }

        // Explicit overload call forces a linker error if the marshal class is not registered.
        if (FBOMResult err = marshal->FBOMMarshaler<NormalizedType<T>>::Serialize(in, out_object)) {
            return err;
        }

        return FBOMResult::FBOM_OK;
    }

    template <class T, typename = std::enable_if_t< !std::is_same_v< FBOMObject, NormalizedType<T> > > >
    static FBOMObject Serialize(const T &in)
    {
        FBOMObject object;

        FBOMResult result = Serialize(in, object);
        AssertThrowMsg(result.IsOK(), "Failed to serialize object: %s", *result.message);

        return object;
    }

    static FBOMResult Deserialize(TypeID type_id, const FBOMObject &in, FBOMDeserializedObject &out);

    template <class T, typename = std::enable_if_t< !std::is_same_v< FBOMObject, NormalizedType<T> > > >
    static FBOMResult Deserialize(const FBOMObject &in, FBOMDeserializedObject &out)
    {
        // return Deserialize(TypeID::ForType<NormalizedType<T>>(), in, out);

        FBOMMarshaler<NormalizedType<T>> *marshal = GetMarshal<T>();
        
        if (!marshal) {
            return { FBOMResult::FBOM_ERR, "No registered marshal class for type" };
        }

        Any any_value;

        // Explicit overload call forces a linker error if the marshal class is not registered.
        const FBOMResult result = marshal->FBOMMarshaler<NormalizedType<T>>::Deserialize(in, any_value);

        if (result.IsOK()) {
            AssertThrow(any_value.HasValue());
            
            out.m_value = std::move(any_value);
        }

        return result;
    }

    template <class T, typename = std::enable_if_t< !std::is_same_v< FBOMObject, NormalizedType<T> > > >
    static FBOMDeserializedObject Deserialize(const FBOMObject &in)
    {
        FBOMDeserializedObject deserialized;

        FBOMResult result = Deserialize<T>(in, deserialized);
        AssertThrowMsg(result.IsOK(), "Failed to deserialize object: %s", *result.message);

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
    static FBOMMarshalerBase *GetMarshal(TypeID object_type_id);

    template <class T>
    HYP_NODISCARD HYP_FORCE_INLINE
    static FBOMMarshaler<NormalizedType<T>> *GetMarshal()
    {
        FBOMMarshalerBase *marshal = GetMarshal(TypeID::ForType<NormalizedType<T>>());

        if (!marshal) {
            return nullptr;
        }

        return dynamic_cast<FBOMMarshaler<NormalizedType<T>> *>(marshal);
    }
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
