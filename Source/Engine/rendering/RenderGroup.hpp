/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>
#include <Core/Constants.hpp>

#include <Core/utilities/EnumFlags.hpp>

#include <rendering/RenderableAttributes.hpp>
#include <rendering/RenderObject.hpp>

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
    PARALLEL_RENDERING = 0x4,

    DEFAULT = OCCLUSION_CULLING | INDIRECT_RENDERING | PARALLEL_RENDERING
};

HYP_MAKE_ENUM_FLAGS(RenderGroupFlags);

class RenderGroup
{
public:
    bool valid = false;
    RenderableAttributeSet renderableAttributes;
    EnumFlags<RenderGroupFlags> flags;

    void PerformRendering(
        Frame* frame,
        const RenderSetup& renderSetup,
        DrawCallCollection& drawCallCollection,
        IndirectRenderer* indirectRenderer,
        ParallelRenderingState* parallelRenderingState) const;
};

} // namespace Hyperion
