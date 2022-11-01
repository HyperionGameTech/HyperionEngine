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

    const FBOMData &GetProperty(const String &key) const;

    void SetProperty(const String &key, const FBOMData &data);
    void SetProperty(const String &key, FBOMData &&data);
    void SetProperty(const String &key, const FBOMType &type, SizeType size, const void *bytes);
    void SetProperty(const String &key, const FBOMType &type, const void *bytes);
    void SetProperty(const String &key, const ByteBuffer &bytes);

    template <class T, typename = typename std::enable_if_t<!std::is_pointer_v<std::remove_reference_t<T>>>>
    void SetProperty(const String &key, const FBOMType &type, const T &value)
    {
        SetProperty(key, type, sizeof(std::remove_reference_t<T>), &value);
    }

    template <class T, typename = typename std::enable_if_t<!std::is_pointer_v<std::remove_reference_t<T>>>>
    void SetProperty(const String &key, const FBOMType &type, T &&value)
    {
        SetProperty(key, type, sizeof(std::remove_reference_t<T>), &value);
    }

    /*! \brief Add a child object to this object node.
        @param object The child object to add
        @param external If true, store the child object in a separate file,
                        To be pulled in when the final object is loaded */
    template <class T, class Marshaler = FBOMMarshaler<NormalizedType<T>>>
    typename std::enable_if_t<!std::is_same_v<FBOMObject, NormalizedType<T>>, FBOMResult>
    AddChild(const T &object, bool external = false)
    {
        static_assert(implementation_exists<Marshaler>,
            "Marshal class does not exist");

        static_assert(std::is_base_of_v<FBOMMarshalerBase, Marshaler>,
            "Marshal class must be a derived class of FBOMMarshalBase");

        Marshaler marshal;

        FBOMObject out_object(marshal.GetObjectType());
        // Set the ID of the object so we can reuse it.
        // TODO: clean this up a bit.
        if constexpr (std::is_base_of_v<EngineComponentBaseBase, NormalizedType<T>>) {
            out_object.m_unique_id = UniqueID(object.GetID());
        } else {
            out_object.m_unique_id = UniqueID(object);
        }

        auto result = marshal.Serialize(object, out_object);

        if (result.value != FBOMResult::FBOM_ERR) {
            String external_object_key;

            if (external) {
                external_object_key = String::ToString(UInt64(out_object.GetUniqueID())) + ".fbom";
            }

            // TODO: Check if external object already exists.
            // if it does, do not build the file again.

            AddChild(std::move(out_object), external_object_key);
        }

        return result;
    }
    
    UniqueID GetUniqueID() const
        { return UniqueID(GetHashCode());  }

    HashCode GetHashCode() const;
    std::string ToString() const;

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
