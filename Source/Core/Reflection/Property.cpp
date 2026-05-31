/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/Reflection/Property.hpp>
#include <Core/Reflection/Field.hpp>
#include <Core/Reflection/Method.hpp>
#include <Core/Reflection/ClassRegistry.hpp>

#include <Core/Reflection/TypeInfo.hpp>

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
