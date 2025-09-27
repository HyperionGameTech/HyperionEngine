/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/object/HypClass.hpp>
#include <core/object/HypStruct.hpp>
#include <core/object/HypEnum.hpp>
#include <core/object/HypMember.hpp>

#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/FormatFwd.hpp>

#include <core/Constants.hpp>

#include <type_traits>

namespace hyperion {

// clang-format off

#define HYP_BEGIN_STRUCT(cls, _static_index, _num_descendants, parentClass, ...)                                                             \
    static struct HypClassInitializer_##cls                                                                                                   \
    {                                                                                                                                         \
        using Type = cls;                                                                                                                     \
        using RegistrationType = ::hyperion::HypStructRegistration<Type>;                                                                     \
                                                                                                                                              \
        static RegistrationType s_classRegistration;                                                                                         \
    } g_class_initializer_##cls {};                                                                                                           \
                                                                                                                                              \
    HypClassInitializer_##cls::RegistrationType HypClassInitializer_##cls::s_classRegistration                                               \
    {                                                                                                                                         \
        NAME(HYP_STR(cls)), _static_index, _num_descendants, parentClass, Span<const HypClassAttribute> { { __VA_ARGS__ } }, Span<HypMember> \
        {                                                                                                                                     \
            {

#define HYP_END_STRUCT \
    }                  \
    }                  \
    };                 \

#define HYP_BEGIN_CLASS(cls, _static_index, _num_descendants, parentClass, ...)                                                              \
    static struct HypClassInitializer_##cls                                                                                                   \
    {                                                                                                                                         \
        using Type = cls;                                                                                                                     \
        using RegistrationType = ::hyperion::HypClassRegistration<Type>;                                                                      \
                                                                                                                                              \
        static RegistrationType s_classRegistration;                                                                                         \
    } g_class_initializer_##cls {};                                                                                                           \
                                                                                                                                              \
    HypClassInitializer_##cls::RegistrationType HypClassInitializer_##cls::s_classRegistration                                               \
    {                                                                                                                                         \
        NAME(HYP_STR(cls)), _static_index, _num_descendants, parentClass, Span<const HypClassAttribute> { { __VA_ARGS__ } }, Span<HypMember> \
        {                                                                                                                                     \
            {

#define HYP_END_CLASS \
    }                 \
    }                 \
    };                \

#define HYP_BEGIN_ENUM(cls, _static_index, _num_descendants, ...)       \
    static struct HypClassInitializer_##cls                             \
    {                                                                   \
        using Type = cls;                                               \
                                                                        \
        using RegistrationType = ::hyperion::HypEnumRegistration<Type>; \
                                                                        \
        static RegistrationType s_classRegistration;                   \
    } g_class_initializer_##cls {};                                     \
                                                                        \
    HypClassInitializer_##cls::RegistrationType HypClassInitializer_##cls::s_classRegistration = { NAME(HYP_STR(cls)), _static_index, _num_descendants, Span<const HypClassAttribute> { { __VA_ARGS__ } }, Span<HypMember> { {

#define HYP_END_ENUM \
    }                \
    }                \
    };

// clang-format on
struct HYP_API HypClassRegistrationBase
{
protected:
    HypClassRegistrationBase(TypeId typeId, HypClass* hypClass);
};

template <class T>
struct HypClassRegistration final : public HypClassRegistrationBase
{
    static constexpr EnumFlags<HypClassFlags> flags = HypClassFlags::CLASS_TYPE
        | (isPodType<T> ? HypClassFlags::POD_TYPE : HypClassFlags::NONE)
        | (std::is_abstract_v<T> ? HypClassFlags::ABSTRACT : HypClassFlags::NONE);

    HypClassRegistration(Name name, int staticIndex, uint32 numDescendants, Name parentName, Span<const HypClassAttribute> attributes, Span<HypMember> members)
        : HypClassRegistrationBase(TypeId::ForType<T>(), &HypClassInstance<T>::GetInstance(name, staticIndex, numDescendants, parentName, attributes, flags, Span<HypMember>(members.Begin(), members.End())))
    {
    }
};

template <class T>
struct HypStructRegistration final : public HypClassRegistrationBase
{
    static constexpr EnumFlags<HypClassFlags> flags = HypClassFlags::STRUCT_TYPE
        | (isPodType<T> ? HypClassFlags::POD_TYPE : HypClassFlags::NONE)
        | (std::is_abstract_v<T> ? HypClassFlags::ABSTRACT : HypClassFlags::NONE);

    HypStructRegistration(Name name, int staticIndex, uint32 numDescendants, Name parentName, Span<const HypClassAttribute> attributes, Span<HypMember> members)
        : HypClassRegistrationBase(TypeId::ForType<T>(), &HypStructInstance<T>::GetInstance(name, staticIndex, numDescendants, parentName, attributes, flags, Span<HypMember>(members.Begin(), members.End())))
    {
    }
};

template <class T>
struct HypEnumRegistration final : public HypClassRegistrationBase
{
    static constexpr EnumFlags<HypClassFlags> flags = HypClassFlags::ENUM_TYPE;

    HypEnumRegistration(Name name, int staticIndex, uint32 numDescendants, Span<const HypClassAttribute> attributes, Span<HypMember> members)
        : HypClassRegistrationBase(TypeId::ForType<T>(), &HypEnumInstance<T>::GetInstance(name, staticIndex, numDescendants, Name::Invalid(), attributes, flags, Span<HypMember>(members.Begin(), members.End())))
    {
    }
};

} // namespace hyperion
