/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERING_SHADOW_MAP_HPP
#define HYPERION_RENDERING_SHADOW_MAP_HPP

#include <rendering/RenderResource.hpp>
#include <rendering/Shadows.hpp>

#include <core/math/Matrix4.hpp>

#include <Types.hpp>

namespace hyperion {

struct alignas(256) ShadowMapShaderData
{
    Matrix4 projection;
    Matrix4 view;
    Vec4f   aabb_max;
    Vec4f   aabb_min;
    Vec2u   dimensions;
    uint32  flags;
};

static_assert(sizeof(ShadowMapShaderData) == 256);

/* max number of shadow maps, based on size in kb */
static const SizeType max_shadow_maps = (4ull * 1024ull) / sizeof(ShadowMapShaderData);
static const SizeType max_shadow_maps_bytes = max_shadow_maps * sizeof(ShadowMapShaderData);

class WorldRenderResource;

class ShadowMapRenderResource final : public RenderResourceBase
{
public:
    ShadowMapRenderResource(ShadowMode shadow_mode, Vec2u extent);
    ShadowMapRenderResource(ShadowMapRenderResource &&other) noexcept;
    virtual ~ShadowMapRenderResource() override;

    HYP_FORCE_INLINE ShadowMode GetShadowMode() const
        { return m_shadow_mode; }

    HYP_FORCE_INLINE Vec2u GetExtent() const
        { return m_extent; }

    void SetBufferData(const ShadowMapShaderData &buffer_data);

protected:
    virtual void Initialize_Internal() override;
    virtual void Destroy_Internal() override;
    virtual void Update_Internal() override;
    
    virtual GPUBufferHolderBase *GetGPUBufferHolder() const override;

private:
    void UpdateBufferData();

    ShadowMode          m_shadow_mode;
    Vec2u               m_extent;

    ShadowMapShaderData m_buffer_data;
};

} // namespace hyperion

#endif