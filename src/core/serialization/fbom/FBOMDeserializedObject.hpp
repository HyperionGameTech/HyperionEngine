/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FBOM_DESERIALIZED_OBJECT_HPP
#define HYPERION_FBOM_DESERIALIZED_OBJECT_HPP

#include <core/containers/String.hpp>
#include <core/containers/Array.hpp>
#include <core/containers/FlatMap.hpp>

#include <core/utilities/Variant.hpp>

#include <core/memory/RefCountedPtr.hpp>
#include <core/memory/UniquePtr.hpp>

#include <core/ID.hpp>

#include <core/serialization/fbom/FBOMBaseTypes.hpp>
#include <core/serialization/SerializationWrapper.hpp>

#include <Types.hpp>
#include <Constants.hpp>

#include <type_traits>

namespace hyperion {
struct HypData;
} // namespace hyperion

namespace hyperion::serialization {

struct FBOMDeserializedObject
{
    UniquePtr<HypData> ptr;

    FBOMDeserializedObject() = default;

    FBOMDeserializedObject(const FBOMDeserializedObject& other) = delete;
    FBOMDeserializedObject& operator=(const FBOMDeserializedObject& other) = delete;

    FBOMDeserializedObject(FBOMDeserializedObject&& other) noexcept = default;
    FBOMDeserializedObject& operator=(FBOMDeserializedObject&& other) noexcept = default;

    ~FBOMDeserializedObject() = default;

    // template <class T>
    // HYP_FORCE_INLINE void Set(const typename SerializationWrapper<T>::Type &value)
    // {
    //     if (!ptr) {
    //         ptr = MakeUnique<HypData>();
    //     }

    //     return ptr->Set<typename SerializationWrapper<T>::Type>(value);
    // }

    // template <class T>
    // HYP_FORCE_INLINE void Set(typename SerializationWrapper<T>::Type &&value)
    // {
    //     if (!ptr) {
    //         ptr = MakeUnique<HypData>();
    //     }

    //     return ptr->Set<typename SerializationWrapper<T>::Type>(std::move(value));
    // }

    /*! \brief Extracts the value held inside */
    template <class T>
    HYP_FORCE_INLINE decltype(auto) Get() const
    {
        AssertThrow(ptr != nullptr);
        return ptr->Get<typename SerializationWrapper<T>::Type>();
    }

    /*! \brief Extracts the value held inside. Returns nullptr if not valid */
    template <class T>
    HYP_FORCE_INLINE decltype(auto) TryGet() const
    {
        if (!ptr)
        {
            return nullptr;
        }

        return ptr->TryGet<std::remove_reference_t<typename SerializationWrapper<T>::Type>>();
    }
};

} // namespace hyperion::serialization

#endif
