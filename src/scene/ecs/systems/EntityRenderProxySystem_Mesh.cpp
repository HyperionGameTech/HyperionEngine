/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/EntityRenderProxySystem_Mesh.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <scene/animation/Skeleton.hpp>

#include <scene/Scene.hpp>
#include <scene/Mesh.hpp>
#include <scene/Material.hpp>

#include <rendering/RenderGlobalState.hpp>
#include <rendering/RenderSkeleton.hpp>
#include <rendering/RenderMaterial.hpp>

#include <rendering/backend/RenderCommand.hpp>

#include <core/containers/HashSet.hpp>

#include <core/utilities/Format.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region Render commands

struct RENDER_COMMAND(UpdateEntityDrawData)
    : renderer::RenderCommand
{
    Array<RenderProxy> render_proxies;

    RENDER_COMMAND(UpdateEntityDrawData)(Array<RenderProxy*>&& render_proxy_ptrs)
    {
        render_proxies.Reserve(render_proxy_ptrs.Size());

        for (RenderProxy* render_proxy_ptr : render_proxy_ptrs)
        {
            render_proxies.PushBack(*render_proxy_ptr);
        }
    }

    virtual ~RENDER_COMMAND(UpdateEntityDrawData)() override = default;

    virtual RendererResult operator()() override
    {
        uint32 max_entity_id_index = 0;

        for (const RenderProxy& proxy : render_proxies)
        {
            if (!proxy.entity.IsValid())
            {
                continue;
            }

            max_entity_id_index = MathUtil::Max(max_entity_id_index, proxy.entity.GetID().ToIndex());
        }

        g_render_global_state->Entities->EnsureCapacity(max_entity_id_index);

        for (const RenderProxy& proxy : render_proxies)
        {
            if (!proxy.entity.IsValid())
            {
                continue;
            }

            EntityShaderData entity_shader_data {
                .model_matrix = proxy.model_matrix,
                .previous_model_matrix = proxy.previous_model_matrix,
                .world_aabb_max = Vec4f(proxy.aabb.max, 1.0f),
                .world_aabb_min = Vec4f(proxy.aabb.min, 1.0f),
                .entity_index = proxy.entity.GetID().ToIndex(),
                .material_index = proxy.material.IsValid() ? proxy.material->GetRenderResource().GetBufferIndex() : ~0u,
                .skeleton_index = proxy.skeleton.IsValid() ? proxy.skeleton->GetRenderResource().GetBufferIndex() : ~0u,
                .bucket = proxy.material.IsValid() ? proxy.material->GetRenderAttributes().bucket : RB_NONE,
                .flags = proxy.skeleton.IsValid() ? ENTITY_GPU_FLAG_HAS_SKELETON : ENTITY_GPU_FLAG_NONE,
                .user_data = proxy.user_data.ReinterpretAs<EntityUserData>()
            };

            // @TODO: Refactor this to instead acquire indices from `objects` to use, rather than using the entity ID index.
            // This will allow us to remove empty blocks when unused.
            g_render_global_state->Entities->Set(proxy.entity.GetID().ToIndex(), entity_shader_data);
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

void EntityRenderProxySystem_Mesh::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    MeshComponent& mesh_component = GetEntityManager().GetComponent<MeshComponent>(entity);

    InitObject(mesh_component.mesh);
    InitObject(mesh_component.material);
    InitObject(mesh_component.skeleton);

    AssertThrow(mesh_component.proxy == nullptr);

    if (mesh_component.mesh.IsValid() && mesh_component.material.IsValid())
    {
        mesh_component.proxy = new RenderProxy {
            entity->WeakHandleFromThis(),
            mesh_component.mesh,
            mesh_component.material,
            mesh_component.skeleton,
            Matrix4::Identity(),
            Matrix4::Identity(),
            BoundingBox::Empty(),
            mesh_component.user_data,
            mesh_component.instance_data,
            /* version */ 0
        };

        PUSH_RENDER_COMMAND(UpdateEntityDrawData, Array<RenderProxy*> { mesh_component.proxy });

        GetEntityManager().RemoveTag<EntityTag::UPDATE_RENDER_PROXY>(entity);
    }
    else
    {
        if (!mesh_component.mesh.IsValid())
        {
            HYP_LOG(ECS, Warning, "Mesh not valid for entity #{}!", entity->GetID());
        }

        if (!mesh_component.material.IsValid())
        {
            HYP_LOG(ECS, Warning, "Material not valid for entity #{}!", entity->GetID());
        }
    }
}

void EntityRenderProxySystem_Mesh::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    MeshComponent& mesh_component = GetEntityManager().GetComponent<MeshComponent>(entity);

    if (mesh_component.proxy)
    {
        delete mesh_component.proxy;
        mesh_component.proxy = nullptr;
    }
}

void EntityRenderProxySystem_Mesh::Process(float delta)
{
    HashSet<WeakHandle<Entity>> updated_entities;
    Array<RenderProxy*> render_proxy_ptrs;

    for (auto [entity, mesh_component, transform_component, bounding_box_component, _] : GetEntityManager().GetEntitySet<MeshComponent, TransformComponent, BoundingBoxComponent, EntityTagComponent<EntityTag::UPDATE_RENDER_PROXY>>().GetScopedView(GetComponentInfos()))
    {
        HYP_NAMED_SCOPE_FMT("Update draw data for entity #{}", entity->GetID());

        if (!mesh_component.mesh.IsValid() || !mesh_component.material.IsValid())
        {
            if (mesh_component.proxy)
            {
                HYP_LOG(ECS, Warning, "Mesh or material not valid for entity #{}!", entity->GetID());

                delete mesh_component.proxy;
                mesh_component.proxy = nullptr;
            }

            continue;
        }

        if (!mesh_component.proxy)
        {
            mesh_component.proxy = new RenderProxy {};
        }

        const uint32 render_proxy_version = mesh_component.proxy->version + 1;

        // Update MeshComponent's proxy
        // @TODO: Include RT info on RenderProxy, add a system that will update BLAS on the render thread.
        // @TODO Add Lightmap volume info
        *mesh_component.proxy = RenderProxy {
            entity->WeakHandleFromThis(),
            mesh_component.mesh,
            mesh_component.material,
            mesh_component.skeleton,
            transform_component.transform.GetMatrix(),
            mesh_component.previous_model_matrix,
            bounding_box_component.world_aabb,
            mesh_component.user_data,
            mesh_component.instance_data,
            render_proxy_version
        };

        render_proxy_ptrs.PushBack(mesh_component.proxy);

        if (mesh_component.previous_model_matrix == transform_component.transform.GetMatrix())
        {
            updated_entities.Insert(entity->WeakHandleFromThis());
        }
        else
        {
            mesh_component.previous_model_matrix = transform_component.transform.GetMatrix();
        }
    }

    if (updated_entities.Any())
    {
        AfterProcess([this, updated_entities = std::move(updated_entities)]()
            {
                for (const WeakHandle<Entity>& entity_weak : updated_entities)
                {
                    GetEntityManager().RemoveTag<EntityTag::UPDATE_RENDER_PROXY>(entity_weak.GetUnsafe());
                }
            });
    }

    if (render_proxy_ptrs.Any())
    {
        PUSH_RENDER_COMMAND(UpdateEntityDrawData, std::move(render_proxy_ptrs));
    }
}

} // namespace hyperion
