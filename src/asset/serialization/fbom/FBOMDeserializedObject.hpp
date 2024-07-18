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

#include <asset/serialization/fbom/FBOMBaseTypes.hpp>
#include <asset/serialization/SerializationWrapper.hpp>

#include <Types.hpp>
#include <Constants.hpp>

#include <type_traits>

namespace hyperion::fbom {

struct FBOMDeserializedObject
{
    Any any_value;

    FBOMDeserializedObject()                                                    = default;

    FBOMDeserializedObject(const FBOMDeserializedObject &other)                 = delete;
    FBOMDeserializedObject &operator=(const FBOMDeserializedObject &other)      = delete;

    FBOMDeserializedObject(FBOMDeserializedObject &&other) noexcept             = default;
    FBOMDeserializedObject &operator=(FBOMDeserializedObject &&other) noexcept  = default;

    ~FBOMDeserializedObject()                                                   = default;

    template <class T>
    HYP_FORCE_INLINE
    void Set(const typename SerializationWrapper<T>::Type &value)
    {
        return any_value.Set<typename SerializationWrapper<T>::Type>(value);
    }

    template <class T>
    HYP_FORCE_INLINE
    void Set(typename SerializationWrapper<T>::Type &&value)
    {
        return any_value.Set<typename SerializationWrapper<T>::Type>(std::move(value));
    }

    /*! \brief Extracts the value held inside */
    template <class T>
    HYP_NODISCARD HYP_FORCE_INLINE
    typename SerializationWrapper<T>::Type &Get()
    {
        return any_value.Get<typename SerializationWrapper<T>::Type>();
    }

    /*! \brief Extracts the value held inside the Any */
    template <class T>
    HYP_NODISCARD HYP_FORCE_INLINE
    const typename SerializationWrapper<T>::Type &Get() const
    {
        return any_value.Get<typename SerializationWrapper<T>::Type>();
    }

    /*! \brief Extracts the value held inside. Returns nullptr if not valid */
    template <class T>
    HYP_NODISCARD HYP_FORCE_INLINE
    typename std::remove_reference_t<typename SerializationWrapper<T>::Type> *TryGet()
    {
        return any_value.TryGet<std::remove_reference_t<typename SerializationWrapper<T>::Type>>();
    }

    /*! \brief Extracts the value held inside. Returns nullptr if not valid */
    template <class T>
    HYP_NODISCARD HYP_FORCE_INLINE
    const std::remove_reference_t<typename SerializationWrapper<T>::Type> *TryGet() const
    {
        return any_value.TryGet<std::remove_reference_t<typename SerializationWrapper<T>::Type>>();
    }
};

} // namespace hyperion::fbom

#endif
