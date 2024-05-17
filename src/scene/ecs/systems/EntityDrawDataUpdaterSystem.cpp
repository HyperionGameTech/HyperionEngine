/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/EntityDrawDataUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <rendering/ShaderGlobals.hpp>

#include <rendering/backend/RenderCommand.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region Render commands

struct RENDER_COMMAND(UpdateRenderProxies) : renderer::RenderCommand
{
    Array<RC<RenderProxy>>  render_proxies;

    RENDER_COMMAND(UpdateRenderProxies)(Array<RC<RenderProxy>> &&render_proxies)
        : render_proxies(std::move(render_proxies))
    {
    }

    virtual ~RENDER_COMMAND(UpdateRenderProxies)() override = default;

    virtual Result operator()() override
    {
        for (const RC<RenderProxy> &proxy_ptr : render_proxies) {
            const RenderProxy &proxy = *proxy_ptr;

            g_engine->GetRenderData()->objects.Set(proxy.entity.ToIndex(), EntityShaderData {
                .model_matrix           = proxy.model_matrix,
                .previous_model_matrix  = proxy.previous_model_matrix,
                .world_aabb_max         = Vec4f(proxy.aabb.max, 1.0f),
                .world_aabb_min         = Vec4f(proxy.aabb.min, 1.0f),
                .entity_index           = proxy.entity.ToIndex(),
                .material_index         = proxy.material.GetID().ToIndex(),
                .skeleton_index         = proxy.skeleton.GetID().ToIndex(),
                .bucket                 = proxy.material.IsValid() ? proxy.material->GetRenderAttributes().bucket : BUCKET_INVALID,
                .flags                  = proxy.skeleton.IsValid() ? ENTITY_GPU_FLAG_HAS_SKELETON : ENTITY_GPU_FLAG_NONE,
                .user_data              = proxy.user_data.ReinterpretAs<Vec4u>()
            });
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

void EntityDrawDataUpdaterSystem::OnEntityAdded(EntityManager &entity_manager, ID<Entity> entity)
{
    SystemBase::OnEntityAdded(entity_manager, entity);

    MeshComponent &mesh_component = entity_manager.GetComponent<MeshComponent>(entity);

    InitObject(mesh_component.mesh);
    InitObject(mesh_component.material);
    InitObject(mesh_component.skeleton);

    if (!mesh_component.proxy) {
        mesh_component.proxy.Reset(new RenderProxy {
            entity,
            mesh_component.mesh,
            mesh_component.material,
            mesh_component.skeleton,
            Matrix4::Identity(),
            Matrix4::Identity(),
            BoundingBox::Empty(),
            mesh_component.user_data
        });
    }

    mesh_component.flags |= MESH_COMPONENT_FLAG_DIRTY;
}

void EntityDrawDataUpdaterSystem::OnEntityRemoved(EntityManager &entity_manager, ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity_manager, entity);

    MeshComponent &mesh_component = entity_manager.GetComponent<MeshComponent>(entity);
}

void EntityDrawDataUpdaterSystem::Process(EntityManager &entity_manager, GameCounter::TickUnit delta)
{
    Array<RC<RenderProxy>> render_proxies;

    for (auto [entity, mesh_component, transform_component, bounding_box_component] : entity_manager.GetEntitySet<MeshComponent, TransformComponent, BoundingBoxComponent>()) {
        if (!(mesh_component.flags & MESH_COMPONENT_FLAG_DIRTY)) {
            continue;
        }

        const ID<Mesh> mesh_id = mesh_component.mesh.GetID();
        const ID<Material> material_id = mesh_component.material.GetID();
        const ID<Skeleton> skeleton_id = mesh_component.skeleton.GetID();

        // Update MeshComponent's proxy
        *mesh_component.proxy = RenderProxy {
            entity,
            mesh_component.mesh,
            mesh_component.material,
            mesh_component.skeleton,
            transform_component.transform.GetMatrix(),
            mesh_component.previous_model_matrix,
            bounding_box_component.world_aabb,
            mesh_component.user_data
        };

        render_proxies.PushBack(mesh_component.proxy);

        if (mesh_component.previous_model_matrix == transform_component.transform.GetMatrix()) {
            mesh_component.flags &= ~MESH_COMPONENT_FLAG_DIRTY;
        } else {
            mesh_component.previous_model_matrix = transform_component.transform.GetMatrix();
        }
    }

    if (render_proxies.Any()) {
        PUSH_RENDER_COMMAND(
            UpdateRenderProxies,
            std::move(render_proxies)
        );
    }
}

} // namespace hyperion
