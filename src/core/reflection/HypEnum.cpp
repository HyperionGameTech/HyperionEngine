#include <core/reflection/HypEnum.hpp>
#include <core/reflection/HypConstant.hpp>

namespace hyperion {

HYP_API HypData GetEnumMemberValue(const IHypMember& enumMember)
{
#ifdef HYP_DEBUG_MODE
    HYP_CORE_ASSERT(enumMember.GetMemberType() == HypMemberType::TYPE_CONSTANT);
#endif

    return static_cast<const HypConstant&>(enumMember).Get();
}

} // namespace hyperion