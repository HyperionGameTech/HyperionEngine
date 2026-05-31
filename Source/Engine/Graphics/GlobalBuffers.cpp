/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <rendering/GlobalBuffers.hpp>
#include <rendering/Buffers.hpp>

namespace Hyperion {

GpuBufferHolderMap::~GpuBufferHolderMap()
{
    DeleteAll();
}
    
void GpuBufferHolderMap::DeleteAll()
{
    for (auto& pair : m_holders)
    {
        if (!pair.second)
            continue;

        PoolDelete(*g_renderPool, pair.second);
        pair.second = nullptr;
    }

    m_holders.Clear();
}

} // namespace Hyperion
