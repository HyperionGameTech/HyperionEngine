/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/reflection/ClassUtils.hpp>
#include <Core/reflection/ClassRegistry.hpp>

namespace Hyperion {

ClassRegistrationBase::ClassRegistrationBase(const TypeId& typeId, Class* cls)
    : m_class(cls)
{
    ClassRegistry::GetInstance().Register(typeId, cls);
}

} // namespace Hyperion
