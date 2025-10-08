/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/object/HypClass.hpp>
#include <core/object/HypData.hpp>

namespace hyperion {

template <class T>
struct HypEnumRegistration;

class HypEnum : protected HypClass
{
public:
    HypEnum(TypeId typeId, Name name, int staticIndex, uint32 numDescendants, Name parentName, Span<const HypClassAttribute> attributes, EnumFlags<HypClassFlags> flags, Span<HypMember> members)
        : HypClass(typeId, name, staticIndex, numDescendants, parentName, attributes, flags, members)
    {
    }

    virtual ~HypEnum() override = default;

    virtual bool IsValid() const override
    {
        return true;
    }

    virtual HypClassAllocationMethod GetAllocationMethod() const override
    {
        return HypClassAllocationMethod::NONE;
    }

#ifdef HYP_DOTNET
    virtual bool GetManagedObject(const void* objectPtr, dotnet::ObjectReference& outObjectReference) const override = 0;
#endif

    virtual bool CanCreateInstance() const override = 0;

    virtual TypeId GetUnderlyingTypeId() const override = 0;

protected:
    virtual bool CreateInstance_Internal(HypData& out) const override = 0;
    virtual bool CreateInstanceArray_Internal(Span<HypData> elements, HypData& out) const override = 0;
};

template <class T>
class HypEnumInstance final : protected HypEnum
{
public:
    template <class U>
    friend struct HypEnumRegistration;

    static HypEnumInstance& GetInstance(Name name, int staticIndex, uint32 numDescendants, Name parentName, Span<const HypClassAttribute> attributes, EnumFlags<HypClassFlags> flags, Span<HypMember> members)
    {
        static HypEnumInstance instance { name, staticIndex, numDescendants, parentName, attributes, flags, members };

        return instance;
    }

    HypEnumInstance(Name name, int staticIndex, uint32 numDescendants, Name parentName, Span<const HypClassAttribute> attributes, EnumFlags<HypClassFlags> flags, Span<HypMember> members)
        : HypEnum(TypeId::ForType<T>(), name, staticIndex, numDescendants, parentName, attributes, flags, members)
    {
        m_size = sizeof(T);
        m_alignment = alignof(T);
    }

    virtual ~HypEnumInstance() override = default;

#ifdef HYP_DOTNET
    virtual bool GetManagedObject(const void* objectPtr, dotnet::ObjectReference& outObjectReference) const override
    {
        HYP_NOT_IMPLEMENTED();
    }
#endif

    virtual bool CanCreateInstance() const override
    {
        return true;
    }

    virtual TypeId GetUnderlyingTypeId() const override
    {
        static const TypeId typeId = TypeId::ForType<std::underlying_type_t<T>>();

        return typeId;
    }

protected:
    virtual bool CreateInstance_Internal(HypData& out) const override
    {
        out = HypData(T {});

        return true;
    }

    virtual bool CreateInstanceArray_Internal(Span<HypData> elements, HypData& out) const override
    {
        Array<T> array;
        array.ResizeUninitialized(elements.Size());

        for (SizeType i = 0; i < elements.Size(); i++)
        {
            // strict = false to allow any numeric type.
            if (!elements[i].Is<std::underlying_type_t<T>>(/* strict */ false))
            {
                return false;
            }

            array[i] = elements[i].Get<T>();
        }

        out = HypData(std::move(array));

        return true;
    }
};

#pragma region Utility functions

HYP_API extern HypData GetEnumMemberValue(const IHypMember& enumMember);

/*! \brief Iterate over the members of an enum HypClass.
 *  \tparam EnumType The enum type to iterate over. The enum must have a HypClass associated with it, otherwise this function will do nothing.
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
        // If the function sets stopIteration to true, stop iteration
        function(member.GetName(), static_cast<EnumType>(GetEnumMemberValue(member).Get<EnumUnderlyingType>()), &stopIteration);

        if (stopIteration)
        {
            return;
        }
    }
}

/*! \brief Find the name of an enum member for a given HypClass, using the members' value.
 *  \tparam EnumType The enum type the member is a part of. The enum must have a HypClass associated with it, otherwise this function will return an empty String.
 *  If the member is not found in the registered HypClass, this function will return a default string (e.g "EnumType(value)").
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

    for (IHypMember& member : hypClass->GetMembers(HypMemberType::TYPE_CONSTANT))
    {
        // If the function sets stopIteration to true, stop iteration
        if (static_cast<EnumType>(GetEnumMemberValue(member).Get<EnumUnderlyingType>()) == value)
        {
            return *member.GetName();
        }
    }

    // If no member found return a string of the value
    return HYP_FORMAT("{}", EnumUnderlyingType(value));
}

/*! \brief Get the value of an enum member for a given HypClass dynamically, given the name of the enum member.
 *  \tparam EnumType The enum type the member is a part of. The enum must have a HypClass associated with it, otherwise this function will do nothing.
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

    if (IHypMember* pMember = hypClass->GetMember(memberName, HypMemberType::TYPE_CONSTANT))
    {
        return static_cast<EnumType>(GetEnumMemberValue(*pMember).Get<EnumUnderlyingType>());
    }

    return errorValue;
}

#pragma endregion Utility functions

} // namespace hyperion
