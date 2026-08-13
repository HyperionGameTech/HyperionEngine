#include <Core/Reflection/Enum.hpp>
#include <Core/Reflection/StaticField.hpp>

namespace Hyperion {

CORE_API bool EnumMemberName(const Class* enumClass, uint64 value, Name& outName)
{
    if (!enumClass || !enumClass->IsEnumType())
    {
        return false;
    }

    for (StaticField* pStaticField : enumClass->GetStaticFields())
    {
        if (uint64(pStaticField->Get().Get<uint64>()) == value)
        {
            outName = pStaticField->GetName();
            return true;
        }
    }

    // If no member found return a string of the value
    return false;
}

CORE_API BoxedValue GetEnumMemberValue(const IMember& enumMember)
{
    AssertDebug(enumMember.GetMemberType() == MemberType::StaticField);

    return static_cast<const StaticField&>(enumMember).Get();
}

} // namespace Hyperion
