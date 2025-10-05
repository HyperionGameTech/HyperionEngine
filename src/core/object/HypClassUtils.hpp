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

#define HYP_BEGIN_STRUCT(cls, _static_index, _num_descendants, parentClass, ...)                                                                \
    HYP_API extern const HypClass* g_cls##cls;                                                                                                  \
                                                                                                                                                \
    static struct HypClassInitializer_##cls                                                                                                     \
    {                                                                                                                                           \
        using Type = cls;                                                                                                                       \
        using RegistrationType = ::hyperion::HypStructRegistration<Type>;                                                                       \
                                                                                                                                                \
        static RegistrationType s_classRegistration;                                                                                            \
                                                                                                                                                \
        HypClassInitializer_##cls ()                                                                                                            \
        {                                                                                                                                       \
        }                                                                                                                                       \
    } g_classInitializer_##cls {};                                                                                                              \
                                                                                                                                                \
    HypClassInitializer_##cls::RegistrationType HypClassInitializer_##cls::s_classRegistration = { &g_cls##cls, NAME(HYP_STR(cls)), _static_index, _num_descendants, parentClass, Span<const HypClassAttribute> { { __VA_ARGS__ } }, Span<HypMember> { {

#define HYP_END_STRUCT \
    }                  \
    }                  \
    };                 \

#define HYP_BEGIN_CLASS(cls, _static_index, _num_descendants, parentClass, ...)                                                                 \
    HYP_API extern const HypClass* g_cls##cls;                                                                                                  \
                                                                                                                                                \
    static struct HypClassInitializer_##cls                                                                                                     \
    {                                                                                                                                           \
        using Type = cls;                                                                                                                       \
        using RegistrationType = ::hyperion::HypClassRegistration<Type>;                                                                        \
                                                                                                                                                \
        static RegistrationType s_classRegistration;                                                                                            \
                                                                                                                                                \
        HypClassInitializer_##cls ()                                                                                                            \
        {                                                                                                                                       \
        }                                                                                                                                       \
    } g_classInitializer_##cls {};                                                                                                              \
                                                                                                                                                \
    HypClassInitializer_##cls::RegistrationType HypClassInitializer_##cls::s_classRegistration = { &g_cls##cls, NAME(HYP_STR(cls)), _static_index, _num_descendants, parentClass, Span<const HypClassAttribute> { { __VA_ARGS__ } }, Span<HypMember> { {

#define HYP_END_CLASS \
    }                 \
    }                 \
    };                \

#define HYP_BEGIN_ENUM(cls, _static_index, _num_descendants, ...)                                                                               \
    HYP_API extern const HypClass* g_cls##cls;                                                                                                  \
                                                                                                                                                \
    static struct HypClassInitializer_##cls                                                                                                     \
    {                                                                                                                                           \
        using Type = cls;                                                                                                                       \
        using RegistrationType = ::hyperion::HypEnumRegistration<Type>;                                                                        \
                                                                                                                                                \
        static RegistrationType s_classRegistration;                                                                                            \
                                                                                                                                                \
        HypClassInitializer_##cls ()                                                                                                            \
        {                                                                                                                                       \
        }                                                                                                                                       \
    } g_classInitializer_##cls {};                                                                                                              \
                                                                                                                                                \
    HypClassInitializer_##cls::RegistrationType HypClassInitializer_##cls::s_classRegistration = { &g_cls##cls, NAME(HYP_STR(cls)), _static_index, _num_descendants, Span<const HypClassAttribute> { { __VA_ARGS__ } }, Span<HypMember> { {

#define HYP_END_ENUM \
    }                \
    }                \
    };

// clang-format on
class HypClassRegistrationBase
{
protected:
    HypClassRegistrationBase(TypeId typeId, HypClass* hypClass);

    const HypClass* m_hypClass;
};

template <class T>
class HypClassRegistration final : public HypClassRegistrationBase
{
public:
    static constexpr EnumFlags<HypClassFlags> flags = HypClassFlags::CLASS_TYPE
        | (isPodType<T> ? HypClassFlags::POD_TYPE : HypClassFlags::NONE)
        | (std::is_abstract_v<T> ? HypClassFlags::ABSTRACT : HypClassFlags::NONE);

    HypClassRegistration(const HypClass** pGlobal, Name name, int staticIndex, uint32 numDescendants, Name parentName, Span<const HypClassAttribute> attributes, Span<HypMember> members)
        : HypClassRegistrationBase(TypeId::ForType<T>(), &HypClassInstance<T>::GetInstance(name, staticIndex, numDescendants, parentName, attributes, flags, Span<HypMember>(members.Begin(), members.End())))
    {
#ifndef HYP_BUILDTOOL
        *pGlobal = m_hypClass;
#endif
    }
};

template <class T>
class HypStructRegistration final : public HypClassRegistrationBase
{
public:
    static constexpr EnumFlags<HypClassFlags> flags = HypClassFlags::STRUCT_TYPE
        | (isPodType<T> ? HypClassFlags::POD_TYPE : HypClassFlags::NONE)
        | (std::is_abstract_v<T> ? HypClassFlags::ABSTRACT : HypClassFlags::NONE);

    HypStructRegistration(const HypClass** pGlobal, Name name, int staticIndex, uint32 numDescendants, Name parentName, Span<const HypClassAttribute> attributes, Span<HypMember> members)
        : HypClassRegistrationBase(TypeId::ForType<T>(), &HypStructInstance<T>::GetInstance(name, staticIndex, numDescendants, parentName, attributes, flags, Span<HypMember>(members.Begin(), members.End())))
    {
#ifndef HYP_BUILDTOOL
        *pGlobal = m_hypClass;
#endif
    }
};

template <class T>
class HypEnumRegistration final : public HypClassRegistrationBase
{
public:
    static constexpr EnumFlags<HypClassFlags> flags = HypClassFlags::ENUM_TYPE;

    HypEnumRegistration(const HypClass** pGlobal, Name name, int staticIndex, uint32 numDescendants, Span<const HypClassAttribute> attributes, Span<HypMember> members)
        : HypClassRegistrationBase(TypeId::ForType<T>(), &HypEnumInstance<T>::GetInstance(name, staticIndex, numDescendants, Name::Invalid(), attributes, flags, Span<HypMember>(members.Begin(), members.End())))
    {
#ifndef HYP_BUILDTOOL
        *pGlobal = m_hypClass;
#endif
    }
};

} // namespace hyperion
