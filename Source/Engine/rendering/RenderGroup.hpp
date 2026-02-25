/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>
#include <Core/Constants.hpp>

#include <Core/reflection/ObjectBase.hpp>
#include <Core/reflection/Handle.hpp>

#include <Core/utilities/EnumFlags.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/RenderableAttributes.hpp>
#include <rendering/GraphicsPipelineCache.hpp>

namespace Hyperion {

class Mesh;
class Material;
class Skeleton;
class Entity;
class RenderCollector;
class GpuBufferHolderBase;
class IndirectRenderer;
struct RenderSetup;
class PassData;
struct DrawCallCollection;
class EntityBatchAllocatorBase;
struct ParallelRenderingState;

enum class RenderGroupFlags : uint32
{
    NONE = 0x0,
    OCCLUSION_CULLING = 0x1,
    INDIRECT_RENDERING = 0x2,
    PARALLEL_RENDERING = 0x4,

    DEFAULT = OCCLUSION_CULLING | INDIRECT_RENDERING | PARALLEL_RENDERING
};

HYP_MAKE_ENUM_FLAGS(RenderGroupFlags)

struct ParallelRenderingState;

HYP_CLASS(NoScriptBindings)
class HYP_API RenderGroup final : public ObjectBase
{
    HYP_OBJECT_BODY(RenderGroup);

public:
    static Pool* GetAllocator() { return g_renderPool; }

    RenderGroup();

    explicit RenderGroup(
        const RenderableAttributeSet& renderableAttributes,
        EnumFlags<RenderGroupFlags> flags = RenderGroupFlags::DEFAULT);

    RenderGroup(
        const RenderableAttributeSet& renderableAttributes,
        const DescriptorTableRef& descriptorTable,
        EnumFlags<RenderGroupFlags> flags = RenderGroupFlags::DEFAULT);

    RenderGroup(const RenderGroup& other) = delete;
    RenderGroup& operator=(const RenderGroup& other) = delete;
    ~RenderGroup();

    HYP_FORCE_INLINE const RenderableAttributeSet& GetRenderableAttributes() const
    {
        return renderableAttributes;
    }

    void SetRenderableAttributes(const RenderableAttributeSet& renderableAttributes);

    HYP_FORCE_INLINE EnumFlags<RenderGroupFlags> GetFlags() const
    {
        return flags;
    }

    void PerformRendering(
        Frame* frame,
        const RenderSetup& renderSetup,
        DrawCallCollection& drawCallCollection,
        IndirectRenderer* indirectRenderer,
        ParallelRenderingState* parallelRenderingState);
        
    RenderableAttributeSet renderableAttributes;
    EnumFlags<RenderGroupFlags> flags;
};

} // namespace Hyperion
