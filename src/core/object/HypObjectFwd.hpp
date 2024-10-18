/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_OBJECT_FWD_HPP
#define HYPERION_CORE_HYP_OBJECT_FWD_HPP

#include <core/Defines.hpp>

#include <core/utilities/TypeID.hpp>

#ifdef HYP_DEBUG_MODE
#include <core/threading/Threads.hpp>
#endif

#include <type_traits>

namespace hyperion {

class HypClass;

extern HYP_API const HypClass *GetClass(TypeID type_id);

template <class T, class T2 = void>
struct IsHypObject
{
    static constexpr bool value = false;
};

template <class T>
struct IsHypObject<T, std::enable_if_t< T::HypObjectData::is_hyp_object && std::is_same_v<T, typename T::HypObjectData::Type > > >
{
    static constexpr bool value = true;
};

struct HypObjectInitializerGuardBase
{
    HYP_API HypObjectInitializerGuardBase(const HypClass *hyp_class, void *address);
    HYP_API ~HypObjectInitializerGuardBase();

    const HypClass  *hyp_class;
    void            *address;

#ifdef HYP_DEBUG_MODE
    ThreadID        initializer_thread_id;
#else
    uint32          count;
#endif
};

template <class T>
struct HypObjectInitializerGuard : HypObjectInitializerGuardBase
{
    HypObjectInitializerGuard(void *address)
        : HypObjectInitializerGuardBase(GetClassAndEnsureValid(), address)
    {
    }

    static const HypClass *GetClassAndEnsureValid()
    {
        static const HypClass *hyp_class = GetClass(TypeID::ForType<T>());

        if constexpr (IsHypObject<T>::value) {
            AssertThrowMsg(hyp_class != nullptr, "HypClass not registered for type %s -- is the \"HYP_CLASS\" macro missing above the class definition?", TypeNameWithoutNamespace<T>().Data());
        }

        return hyp_class;
    }
};

namespace detail {

template <class T, class T2>
struct HypObjectType_Impl;

template <class T>
struct HypObjectType_Impl<T, std::false_type>;

template <class T>
struct HypObjectType_Impl<T, std::true_type>
{
    using Type = T;
};

} // namespace detail

template <class T>
using HypObjectType = typename detail::HypObjectType_Impl<T, std::bool_constant< IsHypObject<T>::value > >::Type;

} // namespace hyperion

#endif