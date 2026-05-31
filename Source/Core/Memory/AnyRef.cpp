/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/Memory/AnyRef.hpp>

#include <Core/Reflection/BoxedValue.hpp>
#include <Core/Reflection/TypeInfo.hpp>

namespace Hyperion {
namespace memory {

TypeId AnyRefBase::GetTypeId() const
{
    return m_typeInfo ? m_typeInfo->id : TypeId::Void();
}

const Class* AnyRefBase::GetClass() const
{
    return m_typeInfo ? m_typeInfo->GetClass() : nullptr;
}

} // namespace memory
} // namespace Hyperion
