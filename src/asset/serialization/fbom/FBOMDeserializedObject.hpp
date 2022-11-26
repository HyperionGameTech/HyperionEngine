#ifndef HYPERION_V2_FBOM_DESERIALIZED_OBJECT_HPP
#define HYPERION_V2_FBOM_DESERIALIZED_OBJECT_HPP

#include <core/lib/Any.hpp>
#include <core/lib/String.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/FlatMap.hpp>
#include <core/lib/Variant.hpp>
#include <core/lib/RefCountedPtr.hpp>
#include <core/lib/UniquePtr.hpp>
#include <core/HandleID.hpp>

#include <asset/AssetBatch.hpp>
#include <asset/serialization/fbom/FBOMBaseTypes.hpp>
#include <asset/serialization/fbom/FBOMLoadable.hpp>
#include <asset/serialization/fbom/FBOMData.hpp>

#include <Types.hpp>
#include <Constants.hpp>

#include <vector>
#include <string>
#include <type_traits>
#include <map>

namespace hyperion::v2::fbom {

class FBOMDeserializedObject
{
public:
    AssetValue m_value;


    FBOMDeserializedObject()
    {
    }

    FBOMDeserializedObject(const FBOMDeserializedObject &other)
        : m_value(other.m_value)
    {
    }

    FBOMDeserializedObject &operator=(const FBOMDeserializedObject &other)
    {
        m_value = other.m_value;
        AssertThrow(m_value.GetTypeID() == other.m_value.GetTypeID());

        return *this;
    }

    FBOMDeserializedObject(FBOMDeserializedObject &&other) noexcept
        : m_value(std::move(other.m_value))
    {
    }

    FBOMDeserializedObject &operator=(FBOMDeserializedObject &&other) noexcept
    {
        auto type_value = other.m_value.GetTypeID();
        m_value = std::move(other.m_value);
        AssertThrow(m_value.GetTypeID() == type_value);

        return *this;
    }

    ~FBOMDeserializedObject() = default;

    /*! \brief Extracts the value held inside */
    template <class T>
    auto Get() -> typename AssetLoaderWrapper<T>::CastedType
    {
        return AssetLoaderWrapper<T>::ExtractAssetValue(m_value);
    }

    /*! \brief Extracts the value held inside the Any */
    template <class T>
    auto Get() const -> const typename AssetLoaderWrapper<T>::CastedType
    {
        return const_cast<FBOMDeserializedObject *>(this)->template Get<T>();
    }

    template <class T>
    void Set(typename AssetLoaderWrapper<T>::ResultType &&value)
    {
        m_value.Set(std::move(value));
    }

    /*! \brief Drops ownership of the object held.
        Be sure to call delete on it when no longer needed!

        If no value is currently held, no changes to the underlying object will
        occur. Otherwise, the held value is set to nullptr, and nullptr is returned.
    */
    // template <class T>
    // [[nodiscard]] T *Release()
    // {
    //     // AssertThrow(false);
    //     return nullptr;
    //     // auto *ptr = RefCountedPtr<void>::Get();
        
    //     // if (!ptr) {
    //     //     return nullptr;
    //     // }

    //     // auto *ptr = any_ptr->Release<NormalizedType<T>>();

    //     // RefCountedPtr<Any>::Reset();

    //     // return held_ptr;
    // }
};

} // namespace hyperion::v2::fbom

#endif
