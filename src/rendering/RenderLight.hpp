/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDER_LIGHT_HPP
#define HYPERION_RENDER_LIGHT_HPP

#include <core/Base.hpp>
#include <core/Handle.hpp>

#include <core/containers/Bitset.hpp>

#include <core/utilities/DataMutationState.hpp>

#include <core/object/HypObject.hpp>

#include <core/math/Color.hpp>
#include <core/math/Vector3.hpp>
#include <core/math/BoundingBox.hpp>
#include <core/math/BoundingSphere.hpp>

#include <rendering/RenderResource.hpp>

#include <Types.hpp>

namespace hyperion {

class Engine;
class Camera;
class Material;
class MaterialRenderResource;

struct LightShaderData
{
    uint32  light_id;
    uint32  light_type;
    uint32  color_packed;
    float   radius;
    // 16

    float   falloff;
    uint32  shadow_map_index;
    Vec2f   area_size;
    // 32

    Vec4f   position_intensity;
    Vec4f   normal;
    // 64

    Vec2f   spot_angles;
    uint32  material_index;
    uint32  _pad2;

    Vec4u   pad3;
    Vec4u   pad4;
    Vec4u   pad5;
};

static_assert(sizeof(LightShaderData) == 128);

static constexpr uint32 max_lights = (64ull * 1024ull) / sizeof(LightShaderData);

class LightRenderResource final : public RenderResourceBase
{
public:
    LightRenderResource(Light *light);
    LightRenderResource(LightRenderResource &&other) noexcept;
    virtual ~LightRenderResource() override;

    /*! \note Only to be called from render thread or render task */
    HYP_FORCE_INLINE Light *GetLight() const
        { return m_light; }

    void SetMaterial(const Handle<Material> &material);

    /*! \note Only to be called from render thread or render task */
    HYP_FORCE_INLINE const Handle<Material> &GetMaterial() const
        { return m_material; }

    void SetBufferData(const LightShaderData &buffer_data);

    /*! \note Only to be called from render thread or render task */
    HYP_FORCE_INLINE const LightShaderData &GetBufferData() const
        { return m_buffer_data; }

    void SetVisibilityBits(const Bitset &visibility_bits);
    
    /*! \note Only to be called from render thread or render task */
    HYP_FORCE_INLINE const Bitset &GetVisibilityBits() const
        { return m_visibility_bits; }

    void EnqueueUnbind();

protected:
    virtual void Initialize_Internal() override;
    virtual void Destroy_Internal() override;
    virtual void Update_Internal() override;

    virtual GPUBufferHolderBase *GetGPUBufferHolder() const override;

private:
    void UpdateBufferData();

    Light                                   *m_light;
    Bitset                                  m_visibility_bits;
    Handle<Material>                        m_material;
    TResourceHandle<MaterialRenderResource> m_material_render_resources_handle;
    LightShaderData                         m_buffer_data;
};

} // namespace hyperion

#endif
