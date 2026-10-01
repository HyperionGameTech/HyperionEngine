/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Functional/Proc.hpp>

#include <Core/Math/Vector3.hpp>

#include <Rendering/RenderTypes.hpp>
#include <Rendering/RenderMemory.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

template <class AllocatorType>
class TCommandRecorder;

using CommandRecorder = TCommandRecorder<RenderAllocator>;

namespace helpers {

uint32 MipmapSize(uint32 srcSize, int lod);

// every backend supports at least this many compute groups along each dimension
static constexpr uint32 MaxComputeGroupsPerDimension = 65535;

Vec3u WrapComputeGroupCount(uint32 numGroups);

} // namespace helpers

class SingleTimeCommands
{
public:
    HYP_DEF_POOL_NEW_DELETE(g_renderPool);

    virtual ~SingleTimeCommands() = default;

    void Push(Proc<void(CommandRecorder&)>&& fn)
    {
        m_functions.PushBack(std::move(fn));
    }

    virtual RendererResult Execute() = 0;

protected:
    Array<Proc<void(CommandRecorder&)>, RenderAllocator> m_functions;
};

} // namespace Hyperion
