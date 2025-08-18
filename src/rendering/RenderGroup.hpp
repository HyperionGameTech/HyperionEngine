/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/object/ObjId.hpp>
#include <core/object/HypObject.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <core/Defines.hpp>

#include <rendering/ShaderManager.hpp>
#include <rendering/RenderableAttributes.hpp>
#include <rendering/CullData.hpp>
#include <rendering/DrawCall.hpp>
#include <rendering/RenderProxy.hpp>

#include <rendering/RenderObject.hpp>

#include <core/Constants.hpp>

namespace hyperion {

class Mesh;
class Material;
class Skeleton;
class Entity;
class RenderCollector;
class GpuBufferHolderBase;
class IndirectRenderer;
struct RenderSetup;
class PassData;

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
class HYP_API RenderGroup final : public HypObjectBase
{
    HYP_OBJECT_BODY(RenderGroup);

public:
    RenderGroup();

    RenderGroup(
        const ShaderRef& shader,
        const RenderableAttributeSet& renderableAttributes,
        EnumFlags<RenderGroupFlags> flags = RenderGroupFlags::DEFAULT);

    RenderGroup(
        const ShaderRef& shader,
        const RenderableAttributeSet& renderableAttributes,
        const DescriptorTableRef& descriptorTable,
        EnumFlags<RenderGroupFlags> flags = RenderGroupFlags::DEFAULT);

    RenderGroup(const RenderGroup& other) = delete;
    RenderGroup& operator=(const RenderGroup& other) = delete;
    ~RenderGroup();

    HYP_FORCE_INLINE const ShaderRef& GetShader() const
    {
        return m_shader;
    }

    void SetShader(const ShaderRef& shader);

    HYP_FORCE_INLINE const RenderableAttributeSet& GetRenderableAttributes() const
    {
        return m_renderableAttributes;
    }

    void SetRenderableAttributes(const RenderableAttributeSet& renderableAttributes);

    HYP_FORCE_INLINE EnumFlags<RenderGroupFlags> GetFlags() const
    {
        return m_flags;
    }

    void PerformRendering(FrameBase* frame, const RenderSetup& renderSetup, const DrawCallCollection& drawCallCollection, IndirectRenderer* indirectRenderer, ParallelRenderingState* parallelRenderingState);

private:
    void Init() override;

    GraphicsPipelineRef CreateGraphicsPipeline(PassData* pd, IDrawCallCollectionImpl* drawCallCollectionImpl) const;

    EnumFlags<RenderGroupFlags> m_flags;

    ShaderRef m_shader;

    DescriptorTableRef m_descriptorTable;

    RenderableAttributeSet m_renderableAttributes;
};

} // namespace hyperion

