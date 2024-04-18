/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_RENDERABLE_ATTRIBUTES_HPP
#define HYPERION_RENDERABLE_ATTRIBUTES_HPP

#include <rendering/backend/RendererStructs.hpp>
#include <rendering/RenderBucket.hpp>
#include <rendering/Shader.hpp>

#include <core/Defines.hpp>
#include <Types.hpp>
#include <HashCode.hpp>

namespace hyperion {

using renderer::FaceCullMode;
using renderer::Topology;
using renderer::FillMode;
using renderer::StencilState;
using renderer::BlendFunction;
using renderer::BlendModeFactor;

class Framebuffer;

struct alignas(uint32) DrawableLayer
{
    uint16  object_index;
    uint16  layer_index;

    DrawableLayer()
        : object_index(0),
          layer_index(0)
    {
    }

    DrawableLayer(uint16 object_index, uint16 layer_index)
        : object_index(object_index),
          layer_index(layer_index)
    {
    }

    DrawableLayer(const DrawableLayer &other)                   = default;
    DrawableLayer &operator=(const DrawableLayer &other)        = default;
    DrawableLayer(DrawableLayer &&other) noexcept               = default;
    DrawableLayer &operator=(DrawableLayer &&other) noexcept    = default;
    ~DrawableLayer()                                            = default;

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator==(const DrawableLayer &other) const
        { return object_index == other.object_index && layer_index == other.layer_index; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator!=(const DrawableLayer &other) const
        { return !(*this == other); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator<(const DrawableLayer &other) const
    {
        // if (object_index < other.object_index) {
        //     return true;
        // } else if (object_index == other.object_index) {
        //     return layer_index < other.layer_index;
        // }

        // return false;

        // @TEMP: Fix this

        return layer_index < other.layer_index;
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(object_index);
        hc.Add(layer_index);

        return hc;
    }
};

struct MaterialAttributes
{
    using MaterialFlags = uint32;

    enum MaterialFlagBits : MaterialFlags
    {
        RENDERABLE_ATTRIBUTE_FLAGS_NONE         = 0x0,
        RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_WRITE  = 0x1,
        RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_TEST   = 0x2
    };

    ShaderDefinition    shader_definition;
    Bucket              bucket = Bucket::BUCKET_OPAQUE;
    FillMode            fill_mode = FillMode::FILL;
    BlendFunction       blend_function = BlendFunction::None();
    FaceCullMode        cull_faces = FaceCullMode::BACK;
    MaterialFlags       flags = RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_WRITE | RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_TEST;
    DrawableLayer       layer;

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator==(const MaterialAttributes &other) const
    {
        return shader_definition == other.shader_definition
            && bucket == other.bucket
            && fill_mode == other.fill_mode
            && blend_function == other.blend_function
            && cull_faces == other.cull_faces
            && flags == other.flags
            && layer == other.layer;
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator!=(const MaterialAttributes &other) const
        { return !(*this == other); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(shader_definition.GetHashCode());
        hc.Add(bucket);
        hc.Add(fill_mode);
        hc.Add(blend_function);
        hc.Add(cull_faces);
        hc.Add(flags);
        hc.Add(layer);

        return hc;
    }
};

struct MeshAttributes
{
    VertexAttributeSet vertex_attributes { static_mesh_vertex_attributes };
    Topology topology { Topology::TRIANGLES };

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator==(const MeshAttributes &other) const
    {
        return vertex_attributes == other.vertex_attributes
            && topology == other.topology;
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(vertex_attributes);
        hc.Add(topology);

        return hc;
    }
};

class RenderableAttributeSet
{
    ID<Framebuffer>     m_framebuffer_id; // only used for scenes, not per entity
    MeshAttributes      m_mesh_attributes;
    MaterialAttributes  m_material_attributes;
    StencilState        m_stencil_state { };
    uint32              m_override_flags;

    mutable HashCode    m_cached_hash_code;
    mutable bool        m_needs_hash_code_recalculation = true;

public:
    RenderableAttributeSet(
        const MeshAttributes &mesh_attributes = { },
        const MaterialAttributes &material_attributes = { },
        uint32 override_flags = 0
    ) : m_mesh_attributes(mesh_attributes),
        m_material_attributes(material_attributes),
        m_override_flags(override_flags)
    {
    }

    RenderableAttributeSet(const RenderableAttributeSet &other)                 = default;
    RenderableAttributeSet &operator=(const RenderableAttributeSet &other)      = default;
    RenderableAttributeSet(RenderableAttributeSet &&other) noexcept             = default;
    RenderableAttributeSet &operator=(RenderableAttributeSet &&other) noexcept  = default;
    ~RenderableAttributeSet()                                                   = default;

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator==(const RenderableAttributeSet &other) const
        { return GetHashCode() == other.GetHashCode(); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator!=(const RenderableAttributeSet &other) const
        { return GetHashCode() != other.GetHashCode(); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator<(const RenderableAttributeSet &other) const
        { return GetHashCode().Value() < other.GetHashCode().Value(); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    const ShaderDefinition &GetShaderDefinition() const
        { return m_material_attributes.shader_definition; }

    HYP_FORCE_INLINE
    void SetShaderDefinition(const ShaderDefinition &shader_definition)
        { m_material_attributes.shader_definition = shader_definition; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    ID<Framebuffer> GetFramebufferID() const
        { return m_framebuffer_id; }

    HYP_FORCE_INLINE
    void SetFramebufferID(ID<Framebuffer> framebuffer_id)
        { m_framebuffer_id = framebuffer_id; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    const MeshAttributes &GetMeshAttributes() const
        { return m_mesh_attributes; }

    HYP_FORCE_INLINE
    void SetMeshAttributes(const MeshAttributes &mesh_attributes)
        { m_mesh_attributes = mesh_attributes; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    const MaterialAttributes &GetMaterialAttributes() const
        { return m_material_attributes; }

    HYP_FORCE_INLINE
    void SetMaterialAttributes(const MaterialAttributes &material_attributes)
        { m_material_attributes = material_attributes; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    const StencilState &GetStencilState() const
        { return m_stencil_state; }

    HYP_FORCE_INLINE
    void SetStencilState(const StencilState &stencil_state)
        { m_stencil_state = stencil_state; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    uint32 GetOverrideFlags() const
        { return m_override_flags; }

    HYP_FORCE_INLINE
    void SetOverrideFlags(uint32 override_flags)
        { m_override_flags = override_flags; }

    HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        if (m_needs_hash_code_recalculation) {
            RecalculateHashCode();

            m_needs_hash_code_recalculation = false;
        }

        return m_cached_hash_code;
    }

private:
    void RecalculateHashCode() const
    {
        HashCode hc;
        hc.Add(m_framebuffer_id.GetHashCode());
        hc.Add(m_mesh_attributes.GetHashCode());
        hc.Add(m_material_attributes.GetHashCode());
        hc.Add(m_stencil_state.GetHashCode());
        hc.Add(m_override_flags);

        m_cached_hash_code = hc;
    }
};

} // namespace hyperion

#endif
