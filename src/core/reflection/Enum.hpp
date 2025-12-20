/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/reflection/Class.hpp>
#include <core/reflection/BoxedValue.hpp>
#include <core/reflection/StaticField.hpp>

namespace hyperion {

template <class T>
struct EnumRegistration;

class Enum : protected Class
{
public:
    Enum(TypeId typeId, Name name, int staticIndex, uint32 numDescendants, Name parentName, Span<const ClassAttribute> attributes, EnumFlags<ClassFlags> flags, Span<HypMember> members)
        : Class(typeId, name, staticIndex, numDescendants, parentName, attributes, flags, members)
    {
    }

    virtual ~Enum() override = default;

    virtual bool IsValid() const override
    {
        return true;
    }

    virtual ClassAllocationMethod GetAllocationMethod() const override
    {
        return ClassAllocationMethod::NONE;
    }

#ifdef HYP_DOTNET
    virtual bool GetManagedObject(const void* objectPtr, dotnet::ObjectReference& outObjectReference) const override = 0;
#endif

    virtual bool CanCreateInstance() const override = 0;

    virtual TypeId GetUnderlyingTypeId() const override = 0;

protected:
    virtual bool CreateInstance_Internal(BoxedValue& out) const override = 0;
    virtual bool CreateInstanceArray_Internal(Span<BoxedValue> elements, BoxedValue& out) const override = 0;
};

template <class T>
class EnumInstance final : protected Enum
{
public:
    template <class U>
    friend struct EnumRegistration;

    static EnumInstance& GetInstance(Name name, int staticIndex, uint32 numDescendants, Name parentName, Span<const ClassAttribute> attributes, EnumFlags<ClassFlags> flags, Span<HypMember> members)
    {
        static EnumInstance s_instance { name, staticIndex, numDescendants, parentName, attributes, flags, members };

        return s_instance;
    }

    EnumInstance(Name name, int staticIndex, uint32 numDescendants, Name parentName, Span<const ClassAttribute> attributes, EnumFlags<ClassFlags> flags, Span<HypMember> members)
        : Enum(TypeId::ForType<T>(), name, staticIndex, numDescendants, parentName, attributes, flags, members)
    {
        m_size = sizeof(T);
        m_alignment = alignof(T);
    }

    virtual ~EnumInstance() override = default;

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
    virtual bool CreateInstance_Internal(BoxedValue& out) const override
    {
        out = BoxedValue(T {});

        return true;
    }

    virtual bool CreateInstanceArray_Internal(Span<BoxedValue> elements, BoxedValue& out) const override
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

        out = BoxedValue(std::move(array));

        return true;
    }
};

#pragma region Utility functions

HYP_API extern BoxedValue GetEnumMemberValue(const IHypMember& enumMember);

/*! \brief Iterate over the members of an enum Class.
 *  \tparam EnumType The enum type to iterate over. The enum must have a Class associated with it, otherwise this function will do nothing.
 *  \tparam Function The function type to call for each member.
 *  \param function The function to call for each member. The function should have the following signature:
 *  \code
 *  void Function(Name name, EnumType value, bool *stopIteration)
 *  \endcode
 */
template <class EnumType, class Function, typename = std::enable_if_t<std::is_enum_v<NormalizedType<EnumType>>>>
void ForEachEnumMember(Function&& function)
{
    using EnumUnderlyingType = std::underlying_type_t<NormalizedType<EnumType>>;

    const Class* cls = GetClass<NormalizedType<EnumType>>();

    if (!cls || !cls->IsEnumType())
    {
        return;
    }

    bool stopIteration = false;

    for (StaticField* pStaticField : cls->GetStaticFields())
    {
        // If the function sets stopIteration to true, stop iteration
        function(pStaticField->GetName(), static_cast<NormalizedType<EnumType>>(pStaticField->Get().Get<EnumUnderlyingType>()), &stopIteration);

        if (stopIteration)
        {
            return;
        }
    }
}

/*! \brief Find the name of an enum member for a given Class, using the members' value. If the enum value is found,
 *  the name is written to \p outName and the function returns true. If the member is not found, the function returns false.
 *  \tparam EnumType The enum type the member is a part of.
 *  \param value The string value of the enum member to find the name of, or EnumName(value) if the member is not found.
 *  \param outName Reference to a Name where the found name will be written.
 *  \returns True if the member was found, false otherwise.
 */
template <class EnumType, typename = std::enable_if_t<std::is_enum_v<NormalizedType<EnumType>>>>
bool EnumMemberName(EnumType value, Name& outName)
{
    using EnumUnderlyingType = std::underlying_type_t<EnumType>;

    outName = Name();

    const Class* cls = GetClass<NormalizedType<EnumType>>();

    if (!cls || !cls->IsEnumType())
    {
        return false;
    }

    for (StaticField* pStaticField : cls->GetStaticFields())
    {
        if (static_cast<NormalizedType<EnumType>>(pStaticField->Get().Get<EnumUnderlyingType>()) == value)
        {
            outName = pStaticField->GetName();
            return true;
        }
    }

    // If no member found return a string of the value
    return false;
}

/*! \brief Find the name of an enum member for a given Class, using the members' value. If the enum value is found,
 *  the name is returned as a String. If the member is not found, a string representation of the enum value is returned.
 *  \tparam EnumType The enum type the member is a part of.
 *  \param value The string value of the enum member to find the name of, or EnumName(value) if the member is not found.
 *  \returns The name of the enum member, or a string representation of the enum value if the member is not found.
 */
template <class EnumType, typename = std::enable_if_t<std::is_enum_v<NormalizedType<EnumType>>>>
String EnumToString(EnumType value)
{
    using EnumUnderlyingType = std::underlying_type_t<NormalizedType<EnumType>>;

    constexpr bool IsFlags = IsEnumFlags<NormalizedType<EnumType>>::value;

    if (Name name; EnumMemberName(value, name))
    {
        return name.LookupString();
    }

    return HYP_FORMAT("{}", EnumUnderlyingType(value));
}

/*! \brief Builds a string by concatenating the names of the set flags for \p value.
 *  \tparam EnumType The enum type the member is a part of.
 *  \param value The string value of the enum member to find the name of, or EnumName(value) if the member is not found.
 *  \returns The name of the enum member, or a string representation of the enum value if the member is not found.
 */
template <class EnumType, typename = std::enable_if_t<std::is_enum_v<NormalizedType<EnumType>>>>
String EnumToString(EnumFlags<EnumType> value)
{
    using EnumUnderlyingType = std::underlying_type_t<NormalizedType<EnumType>>;

    // Set each bit that is set in value
    Array<Name, InlineAllocator<8>> flagNames;

    // loop over the set bits
    FOR_EACH_BIT(EnumUnderlyingType(value), bit)
    {
        EnumType flagValue = static_cast<NormalizedType<EnumType>>(EnumUnderlyingType(1) << bit);

        if (Name flagName; EnumMemberName(flagValue, flagName))
        {
            flagNames.PushBack(flagName);
        }
    }

    if (!flagNames.Empty())
    {
        return String::Join(flagNames, " | ");
    }

    return HYP_FORMAT("{}", EnumUnderlyingType(value));
}

/*! \brief Get the value of an enum member for a given Class dynamically, given the name of the enum member.
 *  \tparam EnumType The enum type the member is a part of. The enum must have a Class associated with it, otherwise this function will do nothing.
 *  \param name The name of the enum member to get the value of.
 *  \param errorValue The value to return if the member is not found.
 */
template <class EnumType, typename = std::enable_if_t<std::is_enum_v<NormalizedType<EnumType>>>>
EnumType EnumValue(StringHash memberName, EnumType errorValue = EnumType())
{
    using EnumUnderlyingType = std::underlying_type_t<NormalizedType<EnumType>>;

    const Class* cls = GetClass<NormalizedType<EnumType>>();

    if (!cls || !cls->IsEnumType())
    {
        return errorValue;
    }

    if (StaticField* pStaticField = cls->GetStaticField(memberName))
    {
        return static_cast<NormalizedType<EnumType>>(pStaticField->Get().Get<EnumUnderlyingType>());
    }

    return errorValue;
}

#pragma endregion Utility functions

} // namespace hyperion
