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

/*! \brief Iterate over the members of a HypEnum.
 *  \tparam EnumType The enum type to iterate over. The enum must have a HypEnum associated with it, otherwise this function will do nothing.
 *  \tparam Function The function type to call for each member.
 *  \param function The function to call for each member. The function should have the following signature:
 *  \code
 *  void Function(Name name, EnumType value, bool *stopIteration)
 *  \endcode
 */
template <class EnumType, class Function, typename = std::enable_if_t<std::is_enum_v<EnumType>>>
void ForEachEnumMember(Function&& function)
{
    using EnumUnderlyingType = std::underlying_type_t<EnumType>;

    const HypClass* hypClass = GetClass<EnumType>();

    if (!hypClass || !hypClass->IsEnumType())
    {
        return;
    }

    bool stopIteration = false;

    for (IHypMember& member : hypClass->GetMembers(HypMemberType::TYPE_CONSTANT))
    {
        HypConstant& memberConstant = static_cast<HypConstant&>(member);

        // If the function sets stopIteration to true, stop iteration
        function(memberConstant.GetName(), static_cast<EnumType>(memberConstant.Get().Get<EnumUnderlyingType>()), &stopIteration);

        if (stopIteration)
        {
            return;
        }
    }
}

/*! \brief Find the name of an enum member for a given HypEnum, using the members' value.
 *  \tparam EnumType The enum type the member is a part of. The enum must have a HypEnum associated with it, otherwise this function will return an empty String.
 *  If the member is not found in the registered HypEnum, this function will return a default string (e.g "EnumType(value)").
 *  \param value The string value of the enum member to find the name of, or EnumName(value) if the member is not found.
 */
template <class EnumType, typename = std::enable_if_t<std::is_enum_v<EnumType>>>
String EnumToString(EnumType value)
{
    using EnumUnderlyingType = std::underlying_type_t<EnumType>;

    const HypClass* hypClass = GetClass<EnumType>();

    if (!hypClass || !hypClass->IsEnumType())
    {
        return String::empty;
    }

    for (HypConstant* memberConstant : hypClass->GetConstants())
    {
        // If the function sets stopIteration to true, stop iteration
        if (static_cast<EnumType>(memberConstant->Get().Get<EnumUnderlyingType>()) == value)
        {
            return *memberConstant->GetName();
        }
    }

    // If no member found return a string in the format EnumType(value)
    return HYP_FORMAT("{}({})", TypeNameHelper<EnumType, true>::value.Data(), EnumUnderlyingType(value));
}

/*! \brief Get the value of an enum member for a given HypEnum dynamically, given the name of the enum member.
 *  \tparam EnumType The enum type the member is a part of. The enum must have a HypEnum associated with it, otherwise this function will do nothing.
 *  \param name The name of the enum member to get the value of.
 *  \param errorValue The value to return if the member is not found.
 */
template <class EnumType, typename = std::enable_if_t<std::is_enum_v<EnumType>>>
EnumType EnumValue(WeakName memberName, EnumType errorValue = EnumType())
{
    using EnumUnderlyingType = std::underlying_type_t<EnumType>;

    const HypClass* hypClass = GetClass<EnumType>();

    if (!hypClass || !hypClass->IsEnumType())
    {
        return errorValue;
    }

    if (HypConstant* memberConstant = hypClass->GetConstant(memberName))
    {
        return static_cast<EnumType>(memberConstant->Get().Get<EnumUnderlyingType>());
    }

    return errorValue;
}

} // namespace hyperion
