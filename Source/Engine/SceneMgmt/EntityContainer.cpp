/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <scene/EntityContainer.hpp>

namespace Hyperion {

EntityContainer& EntityContainer::GetDefaultInstance()
{
    static EntityContainer s_defaultInstance;
    return s_defaultInstance;
}

} // namespace Hyperion
