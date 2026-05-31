/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Scene/Entity.hpp>

using namespace Hyperion;

extern "C"
{

    HYP_EXPORT uint64 Entity_GetID(const Entity* entity)
    {
        if (!entity)
        {
            return 0;
        }

        uint64 value = 0;
        value |= entity->Id().Value();
        value |= (uint64(entity->Id().GetTypeId().Value()) << 32);

        return value;
    }

} // extern "C"
