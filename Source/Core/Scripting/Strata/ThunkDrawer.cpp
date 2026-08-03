/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/Scripting/Strata/ThunkDrawer.hpp>

namespace Hyperion {
namespace Strata {

ThunkDrawer& ThunkDrawer::GetInstance()
{
    static ThunkDrawer s_instance;

    return s_instance;
}

} // namespace Strata
} // namespace Hyperion
