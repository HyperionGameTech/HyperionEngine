#ifndef HYPERION_V2_FBOM_OBJECT_HPP
#define HYPERION_V2_FBOM_OBJECT_HPP

#include <core/lib/Any.hpp>
#include <core/lib/String.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/FlatMap.hpp>
#include <core/lib/Optional.hpp>
#include <core/lib/UniqueID.hpp>

#include <asset/serialization/fbom/FBOMBaseTypes.hpp>
#include <asset/serialization/fbom/FBOMLoadable.hpp>
#include <asset/serialization/fbom/FBOMData.hpp>
#include <asset/serialization/fbom/FBOMMarshaler.hpp>
#include <asset/serialization/fbom/FBOMDeserializedObject.hpp>

#include <Types.hpp>
#include <Constants.hpp>

#include <vector>
#include <string>
#include <map>
#include <type_traits>

namespace hyperion::v2::fbom {

class FBOMNodeHolder;

struct FBOMExternalObjectInfo
{
    String key;

    explicit operator bool() const { return key.Any(); }

    UniqueID GetUniqueID() const
        { return UniqueID(key); }

    HashCode GetHashCode() const
        { return key.GetHashCode(); }
};

using FBOMObjectFlags = UInt;

enum FBOMObjectFlagBits : FBOMObjectFlags
{
    FBOM_OBJECT_FLAGS_NONE = 0x0,
    FBOM_OBJECT_FLAGS_EXTERNAL = 0x1,
    FBOM_OBJECT_FLAGS_KEEP_UNIQUE = 0x2
};

class FBOMObject
{
public:
    FBOMType m_object_type;
    FBOMNodeHolder *nodes;
    FlatMap<String, FBOMData> properties;
    FBOMDeserializedObject deserialized;
    Optional<FBOMExternalObjectInfo> m_external_info;
    UniqueID m_unique_id;

    FBOMObject();
    FBOMObject(const FBOMType &loader_type);
    FBOMObject(const FBOMObject &other);
    FBOMObject &operator=(const FBOMObject &other);
    FBOMObject(FBOMObject &&other) noexcept;
    FBOMObject &operator=(FBOMObject &&other) noexcept;
    ~FBOMObject();

    bool IsExternal() const
        { return m_external_info.Any(); }

    const String &GetExternalObjectKey() const
        { return IsExternal() ? m_external_info.Get().key : String::empty; }

   const FBOMExternalObjectInfo *GetExternalObjectInfo() const
        { return IsExternal() ? &m_external_info.Get() : nullptr; }

    void SetExternalObjectInfo(const FBOMExternalObjectInfo &info)
    {
        if (info) {
            m_external_info = info;
        } else {
            m_external_info.Unset();
        }
    }

    const FBOMType &GetType() const
        { return m_object_type; }

    bool HasProperty(const String &key) const;

    const FBOMData &GetProperty(const String &key) const;

    void SetProperty(const String &key, const FBOMData &data);
    void SetProperty(const String &key, FBOMData &&data);
    void SetProperty(const String &key, const FBOMType &type, SizeType size, const void *bytes);
    void SetProperty(const String &key, const FBOMType &type, const void *bytes);
    void SetProperty(const String &key, const ByteBuffer &bytes);

    template <class T, typename = typename std::enable_if_t<!std::is_pointer_v<NormalizedType<T>>>>
    void SetProperty(const String &key, const FBOMType &type, const T &value)
    {
        SetProperty(key, type, sizeof(NormalizedType<T>), &value);
    }

    template <class T, typename = typename std::enable_if_t<!std::is_pointer_v<NormalizedType<T>>>>
    void SetProperty(const String &key, const FBOMType &type, T &&value)
    {
        SetProperty(key, type, sizeof(NormalizedType<T>), &value);
    }

    /*! \brief Add a child object to this object node.
        @param object The child object to add
        @param external If true, store the child object in a separate file,
                        To be pulled in when the final object is loaded */
    template <class T, class MarshalClass = FBOMMarshaler<NormalizedType<T>>>
    typename std::enable_if_t<!std::is_same_v<FBOMObject, NormalizedType<T>>, FBOMResult>
    AddChild(const T &object, FBOMObjectFlags flags = FBOM_OBJECT_FLAGS_NONE)
    {
        static_assert(implementation_exists<MarshalClass>, "Marshal class does not exist");
        static_assert(std::is_base_of_v<FBOMMarshalerBase, MarshalClass>, "Marshal class must be a derived class of FBOMMarshalBase");

        MarshalClass marshal;

        String external_object_key;

        FBOMObject out_object(marshal.GetObjectType());
        out_object.GenerateUniqueID(object, flags);

        if (flags & FBOM_OBJECT_FLAGS_EXTERNAL) {
            if constexpr (std::is_base_of_v<BasicObjectBase, NormalizedType<T>>) {
                const String class_name_lower(StringUtil::ToLower(marshal.GetObjectType().name.Data()).c_str());
                external_object_key = String::ToString(UInt64(out_object.GetUniqueID())) + ".hyp" + class_name_lower;
            } else {
                external_object_key = String::ToString(UInt64(out_object.GetUniqueID())) + ".hypdata";
            }
        }

        auto result = marshal.Serialize(object, out_object);

        if (result.value != FBOMResult::FBOM_ERR) {
            // TODO: Check if external object already exists.
            // if it does, do not build the file again.
            AddChild(std::move(out_object), external_object_key);
        }

        return result;
    }
    
    UniqueID GetUniqueID() const
        { return m_unique_id;  }

    HashCode GetHashCode() const;
    String ToString() const;

    template <class T>
    void GenerateUniqueID(const T &object, FBOMObjectFlags flags)
    {
        // m_unique_id = UniqueID::Generate();

        // Set the ID of the object so we can reuse it.
        // TODO: clean this up a bit.
        if constexpr (std::is_base_of_v<BasicObjectBase, NormalizedType<T>>) {
            ID<T> id = object.GetID();

            HashCode hc;
            hc.Add(String(HandleDefinition<T>::class_name).GetHashCode());
            hc.Add(id.value);

            m_unique_id = (flags & FBOM_OBJECT_FLAGS_KEEP_UNIQUE) ? UniqueID::Generate() : UniqueID(hc);
        } else if constexpr (HasGetHashCode<NormalizedType<T>, HashCode>::value) {
            m_unique_id = (flags & FBOM_OBJECT_FLAGS_KEEP_UNIQUE) ? UniqueID::Generate() : UniqueID(object);
        } else {
            m_unique_id = UniqueID::Generate();
        }
    }

private:
    void AddChild(FBOMObject &&object, const String &external_object_key = String::empty);
};

// class FBOMNodeHolder {
// public:
//     FBOMNodeHolder
//         : Array<FBOMObject>()
//     {
//     }

//     FBOMNodeHolder(const Array<FBOMObject> &other)
//         : Array<FBOMObject>(other)
//     {
//     }

//     FBOMNodeHolder &FBOMNodeHolder::operator=(const Array<FBOMObject> &other)
//     {
//         Array<FBOMObject>::operator=(other);

//         return *this;
//     }

//     FBOMNodeHolder(Array<FBOMObject> &&other) noexcept
//         : Array<FBOMObject>(std::move(other))
//     {
//     }

//     FBOMNodeHolder &FBOMNodeHolder::operator=(Array<FBOMObject> &&other) noexcept
//     {
//         Array<FBOMObject>::operator=(std::move(other));

//         return *this;
//     }

// private:
//     Array<FBOMObject> objects;
// };

class FBOMNodeHolder : public Array<FBOMObject> {
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

} // namespace hyperion::v2::fbom

#endif
