/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/RenderProxyUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <scene/animation/Skeleton.hpp>

#include <scene/Scene.hpp>
#include <scene/Mesh.hpp>
#include <scene/Material.hpp>

#include <rendering/ShaderGlobals.hpp>
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
        for (const RenderProxy& proxy : render_proxies)
        {
            g_engine->GetRenderData()->objects->Set(proxy.entity.GetID().ToIndex(), EntityShaderData { .model_matrix = proxy.model_matrix, .previous_model_matrix = proxy.previous_model_matrix, .world_aabb_max = Vec4f(proxy.aabb.max, 1.0f), .world_aabb_min = Vec4f(proxy.aabb.min, 1.0f), .entity_index = proxy.entity.GetID().ToIndex(), .material_index = proxy.material.IsValid() ? proxy.material->GetRenderResource().GetBufferIndex() : ~0u, .skeleton_index = proxy.skeleton.IsValid() ? proxy.skeleton->GetRenderResource().GetBufferIndex() : ~0u, .bucket = proxy.material.IsValid() ? proxy.material->GetRenderAttributes().bucket : BUCKET_NONE, .flags = proxy.skeleton.IsValid() ? ENTITY_GPU_FLAG_HAS_SKELETON : ENTITY_GPU_FLAG_NONE, .user_data = proxy.user_data.ReinterpretAs<EntityUserData>() });
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

void RenderProxyUpdaterSystem::OnEntityAdded(const Handle<Entity>& entity)
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
            entity,
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
        HYP_LOG(ECS, Warning, "Mesh or material not valid for entity #{}", entity.GetID().Value());
        HYP_BREAKPOINT;
    }
}

void RenderProxyUpdaterSystem::OnEntityRemoved(ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity);

    MeshComponent& mesh_component = GetEntityManager().GetComponent<MeshComponent>(entity);

    if (mesh_component.proxy)
    {
        delete mesh_component.proxy;
        mesh_component.proxy = nullptr;
    }
}

void RenderProxyUpdaterSystem::Process(GameCounter::TickUnit delta)
{
    HashSet<ID<Entity>> updated_entity_ids;
    Array<RenderProxy*> render_proxy_ptrs;

    for (auto [entity_id, mesh_component, transform_component, bounding_box_component, _] : GetEntityManager().GetEntitySet<MeshComponent, TransformComponent, BoundingBoxComponent, EntityTagComponent<EntityTag::UPDATE_RENDER_PROXY>>().GetScopedView(GetComponentInfos()))
    {
        HYP_NAMED_SCOPE_FMT("Update draw data for entity #{}", entity_id.Value());

        if (!mesh_component.mesh.IsValid() || !mesh_component.material.IsValid())
        {
            HYP_LOG(ECS, Warning, "Mesh or material not valid for entity #{}", entity_id.Value());

            delete mesh_component.proxy;
            mesh_component.proxy = nullptr;
        }
        else
        {
            if (!mesh_component.proxy)
            {
                mesh_component.proxy = new RenderProxy {};
            }

            const uint32 render_proxy_version = mesh_component.proxy->version + 1;

            // Update MeshComponent's proxy
            // @TODO: Include RT info on RenderProxy, add a system that will update BLAS on the render thread.
            *mesh_component.proxy = RenderProxy {
                WeakHandle<Entity>(entity_id),
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
        }

        if (mesh_component.previous_model_matrix == transform_component.transform.GetMatrix())
        {
            updated_entity_ids.Insert(entity_id);
        }
        else
        {
            mesh_component.previous_model_matrix = transform_component.transform.GetMatrix();
        }
    }

    if (updated_entity_ids.Any())
    {
        AfterProcess([this, entity_ids = std::move(updated_entity_ids)]()
            {
                for (const ID<Entity>& entity_id : entity_ids)
                {
                    GetEntityManager().RemoveTag<EntityTag::UPDATE_RENDER_PROXY>(entity_id);
                }
            });
    }

    if (render_proxy_ptrs.Any())
    {
        PUSH_RENDER_COMMAND(UpdateEntityDrawData, std::move(render_proxy_ptrs));
    }
}

} // namespace hyperion
