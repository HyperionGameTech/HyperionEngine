#include <core/reflection/Enum.hpp>
#include <core/reflection/HypConstant.hpp>

namespace hyperion {

HYP_API HypData GetEnumMemberValue(const IHypMember& enumMember)
{
    AssertDebug(enumMember.GetMemberType() == HypMemberType::TYPE_CONSTANT);

    return static_cast<const HypConstant&>(enumMember).Get();
}

} // namespace hyperion