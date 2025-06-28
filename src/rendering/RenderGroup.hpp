/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDER_GROUP_HPP
#define HYPERION_RENDER_GROUP_HPP

#include <core/utilities/EnumFlags.hpp>
#include <core/ID.hpp>
#include <core/Defines.hpp>

#include <rendering/Shader.hpp>
#include <rendering/RenderableAttributes.hpp>
#include <rendering/CullData.hpp>
#include <rendering/DrawCall.hpp>
#include <rendering/RenderProxy.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <Constants.hpp>

namespace hyperion {

class Engine;
class Mesh;
class Material;
class Skeleton;
class Entity;
class RenderCollector;
class GPUBufferHolderBase;
class IndirectRenderer;
class RenderView;
struct RenderSetup;
struct PassData;

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

HYP_CLASS()
class HYP_API RenderGroup final : public HypObject<RenderGroup>
{
    HYP_OBJECT_BODY(RenderGroup);

public:
    RenderGroup();

    RenderGroup(
        const ShaderRef& shader,
        const RenderableAttributeSet& renderable_attributes,
        EnumFlags<RenderGroupFlags> flags = RenderGroupFlags::DEFAULT);

    RenderGroup(
        const ShaderRef& shader,
        const RenderableAttributeSet& renderable_attributes,
        const DescriptorTableRef& descriptor_table,
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
        return m_renderable_attributes;
    }

    void SetRenderableAttributes(const RenderableAttributeSet& renderable_attributes);

    HYP_FORCE_INLINE EnumFlags<RenderGroupFlags> GetFlags() const
    {
        return m_flags;
    }

    void SetDrawCallCollectionImpl(IDrawCallCollectionImpl* draw_call_collection_impl);

    HYP_FORCE_INLINE IDrawCallCollectionImpl* GetDrawCallCollectionImpl() const
    {
        return m_draw_call_collection_impl;
    }

    /*! \brief Collect drawable objects, then run the culling compute shader
     *  to mark any occluded objects as such. Must be used with indirect rendering.
     *  If nullptr is provided for cull_data, no occlusion culling will happen.
     */
    void CollectDrawCalls(DrawCallCollection& draw_call_collection);

    void PerformRendering(FrameBase* frame, const RenderSetup& render_setup, const DrawCallCollection& draw_call_collection, IndirectRenderer* indirect_renderer, ParallelRenderingState* parallel_rendering_state);

private:
    void Init() override;
    
    GraphicsPipelineRef CreateGraphicsPipeline(PassData* pd) const;

    EnumFlags<RenderGroupFlags> m_flags;

    ShaderRef m_shader;

    DescriptorTableRef m_descriptor_table;

    RenderableAttributeSet m_renderable_attributes;

    IDrawCallCollectionImpl* m_draw_call_collection_impl;
};

} // namespace hyperion

#endif
