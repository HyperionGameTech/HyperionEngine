/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERING_SCENE_HPP
#define HYPERION_RENDERING_SCENE_HPP

#include <rendering/RenderResource.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <core/math/Vector4.hpp>

#include <core/Handle.hpp>

#include <Types.hpp>

namespace hyperion {

class Scene;
class RenderEnvironment;
class RenderCamera;

class RenderScene final : public RenderResourceBase
{
public:
    RenderScene(Scene* scene);
    virtual ~RenderScene() override;

    HYP_FORCE_INLINE Scene* GetScene() const
    {
        return m_scene;
    }

protected:
    virtual void Initialize_Internal() override;
    virtual void Destroy_Internal() override;
    virtual void Update_Internal() override;

    virtual GPUBufferHolderBase* GetGPUBufferHolder() const override;

private:
    Scene* m_scene;

    ImageRef m_shadows_texture_array_image;
    ImageViewRef m_shadows_texture_array_image_view;
};

template <>
struct ResourceMemoryPoolInitInfo<RenderScene> : MemoryPoolInitInfo<RenderScene>
{
    static constexpr uint32 num_elements_per_block = 8;
    static constexpr uint32 num_initial_elements = 8;
};

} // namespace hyperion

#endif
