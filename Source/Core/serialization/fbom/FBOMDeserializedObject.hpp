/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/memory/RefCountedPtr.hpp>
#include <Core/memory/UniquePtr.hpp>

#include <Core/reflection/ObjId.hpp>

#include <Core/serialization/fbom/FBOMBaseTypes.hpp>
#include <Core/serialization/SerializationWrapper.hpp>

#include <Core/Types.hpp>
#include <Core/Constants.hpp>

#include <type_traits>

namespace Hyperion {
struct BoxedValue;
} // namespace Hyperion

namespace Hyperion::serialization {

struct FBOMDeserializedObject
{
    UniquePtr<BoxedValue> ptr;

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
    //         ptr = MakeUnique<BoxedValue>();
    //     }

    //     return ptr->Set<typename SerializationWrapper<T>::Type>(value);
    // }

    // template <class T>
    // HYP_FORCE_INLINE void Set(typename SerializationWrapper<T>::Type &&value)
    // {
    //     if (!ptr) {
    //         ptr = MakeUnique<BoxedValue>();
    //     }

    //     return ptr->Set<typename SerializationWrapper<T>::Type>(std::move(value));
    // }

    /*! \brief Extracts the value held inside */
    template <class T>
    HYP_FORCE_INLINE decltype(auto) Get() const
    {
        HYP_CORE_ASSERT(ptr != nullptr);
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

} // namespace Hyperion::serialization
