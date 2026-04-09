/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/reflection/Property.hpp>
#include <Core/reflection/Field.hpp>
#include <Core/reflection/Method.hpp>
#include <Core/reflection/ClassRegistry.hpp>

#include <Core/reflection/TypeInfo.hpp>

namespace Hyperion {

const Class* Property::GetClass() const
{
    if (!m_typeInfo)
    {
        return nullptr;
    }

    return m_typeInfo->GetClass();
}

} // namespace Hyperion
