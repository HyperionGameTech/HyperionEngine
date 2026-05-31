/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/Constants.hpp>

#include <Core/utilities/EnumFlags.hpp>

#include <Rendering/RenderableAttributes.hpp>
#include <Rendering/RenderTypes.hpp>

namespace Hyperion {

class IndirectRenderer;
struct RenderSetup;
struct DrawCallCollection;
struct ParallelRenderingState;

enum class RenderGroupFlags : uint32
{
    NONE = 0x0,
    OCCLUSION_CULLING = 0x1,
    INDIRECT_RENDERING = 0x2,
    PARALLEL_COLLECTION = 0x4,

    DEFAULT = OCCLUSION_CULLING | INDIRECT_RENDERING | PARALLEL_COLLECTION
};

HYP_MAKE_ENUM_FLAGS(RenderGroupFlags);

class RenderGroup
{
public:
    bool valid = false;
    RenderableAttributeSet renderableAttributes;
};

} // namespace Hyperion
