/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Core/Utilities/Time.hpp>

using namespace Hyperion;

extern "C"
{

    HYP_EXPORT uint64 Time_Now()
    {
        return uint64(Time::Now());
    }

} // extern "C"
