/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FBOM_DESERIALIZED_OBJECT_HPP
#define HYPERION_FBOM_DESERIALIZED_OBJECT_HPP

#include <core/memory/Any.hpp>
#include <core/containers/String.hpp>
#include <core/containers/Array.hpp>
#include <core/containers/FlatMap.hpp>
#include <core/utilities/Variant.hpp>
#include <core/memory/RefCountedPtr.hpp>
#include <core/memory/UniquePtr.hpp>
#include <core/ID.hpp>

#include <asset/AssetBatch.hpp>
#include <asset/serialization/fbom/FBOMBaseTypes.hpp>
#include <asset/serialization/fbom/FBOMData.hpp>

#include <Types.hpp>
#include <Constants.hpp>

#include <type_traits>

namespace hyperion::fbom {

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

        return *this;
    }

    FBOMDeserializedObject(FBOMDeserializedObject &&other) noexcept
        : m_value(std::move(other.m_value))
    {
    }

    FBOMDeserializedObject &operator=(FBOMDeserializedObject &&other) noexcept
    {
        m_value = std::move(other.m_value);

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
        m_value = std::move(value);
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

} // namespace hyperion::fbom

#endif
