/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_CLASS_UTILS_HPP
#define HYPERION_CORE_HYP_CLASS_UTILS_HPP

#include <core/object/HypClass.hpp>
#include <core/object/HypStruct.hpp>
#include <core/object/HypEnum.hpp>
#include <core/object/HypMember.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <Constants.hpp>

#include <type_traits>

namespace hyperion {

// clang-format off

#define HYP_BEGIN_STRUCT(cls, _static_index, _num_descendants, parent_class, ...)                                                             \
    static struct HypClassInitializer_##cls                                                                                                   \
    {                                                                                                                                         \
        using Type = cls;                                                                                                                     \
        using RegistrationType = ::hyperion::HypStructRegistration<Type>;                                                                     \
                                                                                                                                              \
        static RegistrationType s_class_registration;                                                                                         \
    } g_class_initializer_##cls {};                                                                                                           \
                                                                                                                                              \
    HypClassInitializer_##cls::RegistrationType HypClassInitializer_##cls::s_class_registration                                               \
    {                                                                                                                                         \
        NAME(HYP_STR(cls)), _static_index, _num_descendants, parent_class, Span<const HypClassAttribute> { { __VA_ARGS__ } }, Span<HypMember> \
        {                                                                                                                                     \
            {

#define HYP_END_STRUCT \
    }                  \
    }                  \
    };                 \

#define HYP_BEGIN_CLASS(cls, _static_index, _num_descendants, parent_class, ...)                                                              \
    static struct HypClassInitializer_##cls                                                                                                   \
    {                                                                                                                                         \
        using Type = cls;                                                                                                                     \
        using RegistrationType = ::hyperion::HypClassRegistration<Type>;                                                                      \
                                                                                                                                              \
        static RegistrationType s_class_registration;                                                                                         \
    } g_class_initializer_##cls {};                                                                                                           \
                                                                                                                                              \
    HypClassInitializer_##cls::RegistrationType HypClassInitializer_##cls::s_class_registration                                               \
    {                                                                                                                                         \
        NAME(HYP_STR(cls)), _static_index, _num_descendants, parent_class, Span<const HypClassAttribute> { { __VA_ARGS__ } }, Span<HypMember> \
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
        static RegistrationType s_class_registration;                   \
    } g_class_initializer_##cls {};                                     \
                                                                        \
    HypClassInitializer_##cls::RegistrationType HypClassInitializer_##cls::s_class_registration = { NAME(HYP_STR(cls)), _static_index, _num_descendants, Span<const HypClassAttribute> { { __VA_ARGS__ } }, Span<HypMember> { {

#define HYP_END_ENUM \
    }                \
    }                \
    };

// clang-format on
struct HYP_API HypClassRegistrationBase
{
protected:
    HypClassRegistrationBase(TypeId type_id, HypClass* hyp_class);
};

template <class T>
struct HypClassRegistration final : public HypClassRegistrationBase
{
    static constexpr EnumFlags<HypClassFlags> flags = HypClassFlags::CLASS_TYPE
        | (is_pod_type<T> ? HypClassFlags::POD_TYPE : HypClassFlags::NONE)
        | (std::is_abstract_v<T> ? HypClassFlags::ABSTRACT : HypClassFlags::NONE);

    HypClassRegistration(Name name, int static_index, uint32 num_descendants, Name parent_name, Span<const HypClassAttribute> attributes, Span<HypMember> members)
        : HypClassRegistrationBase(TypeId::ForType<T>(), &HypClassInstance<T>::GetInstance(name, static_index, num_descendants, parent_name, attributes, flags, Span<HypMember>(members.Begin(), members.End())))
    {
    }
};

template <class T>
struct HypStructRegistration final : public HypClassRegistrationBase
{
    static constexpr EnumFlags<HypClassFlags> flags = HypClassFlags::STRUCT_TYPE
        | (is_pod_type<T> ? HypClassFlags::POD_TYPE : HypClassFlags::NONE)
        | (std::is_abstract_v<T> ? HypClassFlags::ABSTRACT : HypClassFlags::NONE);

    HypStructRegistration(Name name, int static_index, uint32 num_descendants, Name parent_name, Span<const HypClassAttribute> attributes, Span<HypMember> members)
        : HypClassRegistrationBase(TypeId::ForType<T>(), &HypStructInstance<T>::GetInstance(name, static_index, num_descendants, parent_name, attributes, flags, Span<HypMember>(members.Begin(), members.End())))
    {
    }
};

template <class T>
struct HypEnumRegistration final : public HypClassRegistrationBase
{
    static constexpr EnumFlags<HypClassFlags> flags = HypClassFlags::ENUM_TYPE;

    HypEnumRegistration(Name name, int static_index, uint32 num_descendants, Span<const HypClassAttribute> attributes, Span<HypMember> members)
        : HypClassRegistrationBase(TypeId::ForType<T>(), &HypEnumInstance<T>::GetInstance(name, static_index, num_descendants, Name::Invalid(), attributes, flags, Span<HypMember>(members.Begin(), members.End())))
    {
    }
};

/*! \brief Iterate over the members of a HypEnum.
 *  \tparam EnumType The enum type to iterate over. The enum must have a HypEnum associated with it, otherwise this function will do nothing.
 *  \tparam Function The function type to call for each member.
 *  \param function The function to call for each member. The function should have the following signature:
 *  \code
 *  void Function(Name name, EnumType value, bool *stop_iteration)
 *  \endcode
 */
template <class EnumType, class Function, typename = std::enable_if_t<std::is_enum_v<EnumType>>>
void ForEachEnumMember(Function&& function)
{
    using EnumUnderlyingType = std::underlying_type_t<EnumType>;

    const HypClass* hyp_class = GetClass<EnumType>();

    if (!hyp_class || !hyp_class->IsEnumType())
    {
        return;
    }

    bool stop_iteration = false;

    for (IHypMember& member : hyp_class->GetMembers(HypMemberType::TYPE_CONSTANT))
    {
        HypConstant& member_constant = static_cast<HypConstant&>(member);

        // If the function sets stop_iteration to true, stop iteration
        function(member_constant.GetName(), static_cast<EnumType>(member_constant.Get().Get<EnumUnderlyingType>()), &stop_iteration);

        if (stop_iteration)
        {
            return;
        }
    }
}

/*! \brief Find the name of an enum member for a given HypEnum, using the members' value.
 *  \tparam EnumType The enum type the member is a part of. The enum must have a HypEnum associated with it, otherwise this function will do nothing.
 *  \param value The value of the enum member to find the name of.
 */
template <class EnumType, typename = std::enable_if_t<std::is_enum_v<EnumType>>>
Name GetEnumMemberName(EnumType value)
{
    using EnumUnderlyingType = std::underlying_type_t<EnumType>;

    const HypClass* hyp_class = GetClass<EnumType>();

    if (!hyp_class || !hyp_class->IsEnumType())
    {
        return Name::Invalid();
    }

    for (IHypMember& member : hyp_class->GetMembers(HypMemberType::TYPE_CONSTANT))
    {
        HypConstant& member_constant = static_cast<HypConstant&>(member);

        // If the function sets stop_iteration to true, stop iteration
        if (static_cast<EnumType>(member_constant.Get().Get<EnumUnderlyingType>()) == value)
        {
            return member_constant.GetName();
        }
    }

    return Name::Invalid();
}

/*! \brief Get the value of an enum member for a given HypEnum dynamically, given the name of the enum member.
 *  \tparam EnumType The enum type the member is a part of. The enum must have a HypEnum associated with it, otherwise this function will do nothing.
 *  \param name The name of the enum member to get the value of.
 *  \param error_value The value to return if the member is not found.
 */
template <class EnumType, typename = std::enable_if_t<std::is_enum_v<EnumType>>>
EnumType GetEnumMemberValue(WeakName member_name, EnumType error_value = EnumType())
{
    using EnumUnderlyingType = std::underlying_type_t<EnumType>;

    const HypClass* hyp_class = GetClass<EnumType>();

    if (!hyp_class || !hyp_class->IsEnumType())
    {
        return error_value;
    }

    if (HypConstant* member_constant = hyp_class->GetConstant(member_name))
    {
        return static_cast<EnumType>(member_constant->Get().Get<EnumUnderlyingType>());
    }

    return error_value;
}

} // namespace hyperion

#endif