#include <core/reflection/Enum.hpp>
#include <core/reflection/StaticField.hpp>

namespace Hyperion {

HYP_API BoxedValue GetEnumMemberValue(const IMember& enumMember)
{
    AssertDebug(enumMember.GetMemberType() == MemberType::StaticField);

    return static_cast<const StaticField&>(enumMember).Get();
}

} // namespace Hyperion