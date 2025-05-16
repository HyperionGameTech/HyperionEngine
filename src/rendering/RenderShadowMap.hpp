/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERING_SHADOW_MAP_HPP
#define HYPERION_RENDERING_SHADOW_MAP_HPP

#include <rendering/RenderResource.hpp>
#include <rendering/Shadows.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <core/math/Matrix4.hpp>

#include <Types.hpp>

namespace hyperion {

struct ShadowMapShaderData
{
    Matrix4 projection;
    Matrix4 view;
    Vec4f   aabb_max;
    Vec4f   aabb_min;
    Vec4f   dimensions_scale; // xy = shadow map dimensions in pixels, zw = shadow map dimensions relative to the atlas dimensions
    Vec2f   offset_uv; // offset in the atlas texture array
    uint32  layer_index; // index of the atlas in the shadow map texture array, or cubemap index for point lights
    uint32  flags;

    Vec4f   _pad1;
    Vec4f   _pad2;
    Vec4f   _pad3;
    Vec4f   _pad4;
};

static_assert(sizeof(ShadowMapShaderData) == 256);

/* max number of shadow maps, based on size in kb */
static const SizeType max_shadow_maps = (4ull * 1024ull) / sizeof(ShadowMapShaderData);
static const SizeType max_shadow_maps_bytes = max_shadow_maps * sizeof(ShadowMapShaderData);

class WorldRenderResource;

class ShadowMapRenderResource final : public RenderResourceBase
{
public:
    ShadowMapRenderResource(ShadowMapType type, ShadowMapFilterMode filter_mode, const ShadowMapAtlasElement &atlas_element, const ImageViewRef &image_view);
    ShadowMapRenderResource(ShadowMapRenderResource &&other) noexcept;
    virtual ~ShadowMapRenderResource() override;

    HYP_FORCE_INLINE ShadowMapType GetShadowMapType() const
        { return m_type; }

    HYP_FORCE_INLINE ShadowMapFilterMode GetFilterMode() const
        { return m_filter_mode; }

    HYP_FORCE_INLINE const Vec2u &GetExtent() const
        { return m_atlas_element.dimensions; }

    HYP_FORCE_INLINE const ShadowMapAtlasElement &GetAtlasElement() const
        { return m_atlas_element; }

    HYP_FORCE_INLINE const ImageViewRef &GetImageView() const
        { return m_image_view; }

    void SetBufferData(const ShadowMapShaderData &buffer_data);

protected:
    virtual void Initialize_Internal() override;
    virtual void Destroy_Internal() override;
    virtual void Update_Internal() override;
    
    virtual GPUBufferHolderBase *GetGPUBufferHolder() const override;

private:
    void UpdateBufferData();

    ShadowMapType           m_type;
    ShadowMapFilterMode     m_filter_mode;
    ShadowMapAtlasElement   m_atlas_element;
    ImageViewRef            m_image_view;
    ShadowMapShaderData     m_buffer_data;
};

} // namespace hyperion

#endif