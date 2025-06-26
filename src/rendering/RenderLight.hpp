/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDER_LIGHT_HPP
#define HYPERION_RENDER_LIGHT_HPP

#include <core/Base.hpp>
#include <core/Handle.hpp>

#include <core/utilities/DataMutationState.hpp>

#include <core/object/HypObject.hpp>

#include <core/math/Color.hpp>
#include <core/math/Vector3.hpp>
#include <core/math/BoundingBox.hpp>
#include <core/math/BoundingSphere.hpp>

#include <rendering/RenderResource.hpp>
#include <rendering/RenderProxy.hpp> // tmp

#include <Types.hpp>

namespace hyperion {

class Engine;
class Camera;
class Material;
class RenderMaterial;
class RenderShadowMap;

class RenderLight final : public RenderResourceBase
{
public:
    RenderLight(Light* light);
    RenderLight(RenderLight&& other) noexcept;
    virtual ~RenderLight() override;

    /*! \note Only to be called from render thread or render task */
    HYP_FORCE_INLINE Light* GetLight() const
    {
        return m_light;
    }

    void SetMaterial(const Handle<Material>& material);

    /*! \note Only to be called from render thread or render task */
    HYP_FORCE_INLINE const Handle<Material>& GetMaterial() const
    {
        return m_material;
    }

    void SetBufferData(const LightShaderData& buffer_data);

    /*! \note Only to be called from render thread or render task */
    HYP_FORCE_INLINE const LightShaderData& GetBufferData() const
    {
        return m_buffer_data;
    }

    HYP_FORCE_INLINE const TResourceHandle<RenderShadowMap>& GetShadowMap() const
    {
        return m_shadow_map;
    }

    void SetShadowMap(TResourceHandle<RenderShadowMap>&& shadow_map);

protected:
    virtual void Initialize_Internal() override;
    virtual void Destroy_Internal() override;
    virtual void Update_Internal() override;

    virtual GPUBufferHolderBase* GetGPUBufferHolder() const override;

private:
    void UpdateBufferData();

    Light* m_light;
    Handle<Material> m_material;
    TResourceHandle<RenderMaterial> m_render_material;
    TResourceHandle<RenderShadowMap> m_shadow_map;
    LightShaderData m_buffer_data;
};

} // namespace hyperion

#endif
