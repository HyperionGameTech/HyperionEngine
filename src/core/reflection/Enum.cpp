#include <core/reflection/Enum.hpp>
#include <core/reflection/StaticField.hpp>

namespace hyperion {

HYP_API HypData GetEnumMemberValue(const IHypMember& enumMember)
{
    AssertDebug(enumMember.GetMemberType() == HypMemberType::TYPE_STATIC_FIELD);

    return static_cast<const StaticField&>(enumMember).Get();
}

} // namespace hyperion