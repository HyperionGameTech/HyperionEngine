#include <Core/Reflection/Enum.hpp>
#include <Core/Reflection/StaticField.hpp>

namespace Hyperion {

CORE_API BoxedValue GetEnumMemberValue(const IMember& enumMember)
{
    AssertDebug(enumMember.GetMemberType() == MemberType::StaticField);

    return static_cast<const StaticField&>(enumMember).Get();
}

} // namespace Hyperion
