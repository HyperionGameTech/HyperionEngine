/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/RenderProxyUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <scene/animation/Skeleton.hpp>

#include <scene/Scene.hpp>

#include <rendering/ShaderGlobals.hpp>
#include <rendering/Skeleton.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>

#include <rendering/backend/RenderCommand.hpp>

#include <core/containers/HashSet.hpp>

#include <core/utilities/Format.hpp>

#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(ECS);
HYP_DEFINE_LOG_SUBCHANNEL(RenderProxy, ECS);

#pragma region Render commands

struct RENDER_COMMAND(UpdateEntityDrawData) : renderer::RenderCommand
{
    Array<RC<RenderProxy>>  render_proxies;

    RENDER_COMMAND(UpdateEntityDrawData)(Array<RC<RenderProxy>> &&render_proxies)
        : render_proxies(std::move(render_proxies))
    {
    }

    virtual ~RENDER_COMMAND(UpdateEntityDrawData)() override = default;

    virtual RendererResult operator()() override
    {
        for (const RC<RenderProxy> &proxy_ptr : render_proxies) {
            const RenderProxy &proxy = *proxy_ptr;

            g_engine->GetRenderData()->objects->Set(proxy.entity.GetID().ToIndex(), EntityShaderData {
                .model_matrix           = proxy.model_matrix,
                .previous_model_matrix  = proxy.previous_model_matrix,
                .world_aabb_max         = Vec4f(proxy.aabb.max, 1.0f),
                .world_aabb_min         = Vec4f(proxy.aabb.min, 1.0f),
                .entity_index           = proxy.entity.GetID().ToIndex(),
                .material_index         = proxy.material.IsValid() ? proxy.material->GetRenderResources().GetBufferIndex() : ~0u,
                .skeleton_index         = proxy.skeleton.IsValid() ? proxy.skeleton->GetRenderResources().GetBufferIndex() : ~0u,
                .bucket                 = proxy.material.IsValid() ? proxy.material->GetRenderAttributes().bucket : BUCKET_INVALID,
                .flags                  = proxy.skeleton.IsValid() ? ENTITY_GPU_FLAG_HAS_SKELETON : ENTITY_GPU_FLAG_NONE,
                .user_data              = proxy.user_data.ReinterpretAs<EntityUserData>()
            });
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

EnumFlags<SceneFlags> RenderProxyUpdaterSystem::GetRequiredSceneFlags() const
{
    return SceneFlags::NONE;
}

void RenderProxyUpdaterSystem::OnEntityAdded(const Handle<Entity> &entity)
{
    SystemBase::OnEntityAdded(entity);

    MeshComponent &mesh_component = GetEntityManager().GetComponent<MeshComponent>(entity);

    InitObject(mesh_component.mesh);
    InitObject(mesh_component.material);
    InitObject(mesh_component.skeleton);

    if (!mesh_component.proxy) {
        mesh_component.proxy.Emplace(RenderProxy {
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
        });
    }

    GetEntityManager().RemoveTag<EntityTag::UPDATE_RENDER_PROXY>(entity);
}

void RenderProxyUpdaterSystem::OnEntityRemoved(ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity);

    MeshComponent &mesh_component = GetEntityManager().GetComponent<MeshComponent>(entity);
}

void RenderProxyUpdaterSystem::Process(GameCounter::TickUnit delta)
{
    HashSet<ID<Entity>> updated_entity_ids;
    Array<RC<RenderProxy>> render_proxies;

    for (auto [entity_id, mesh_component, transform_component, bounding_box_component, _] : GetEntityManager().GetEntitySet<MeshComponent, TransformComponent, BoundingBoxComponent, EntityTagComponent<EntityTag::UPDATE_RENDER_PROXY>>().GetScopedView(GetComponentInfos())) {
        HYP_NAMED_SCOPE_FMT("Update draw data for entity #{}", entity_id.Value());

        const uint32 render_proxy_version = mesh_component.proxy != nullptr
            ? mesh_component.proxy->version + 1
            : 0;

        // Update MeshComponent's proxy
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

        render_proxies.PushBack(mesh_component.proxy);

        if (mesh_component.previous_model_matrix == transform_component.transform.GetMatrix()) {
            updated_entity_ids.Insert(entity_id);
        } else {
            mesh_component.previous_model_matrix = transform_component.transform.GetMatrix();
        }
    }

    for (const ID<Entity> &entity_id : updated_entity_ids) {
        GetEntityManager().RemoveTag<EntityTag::UPDATE_RENDER_PROXY>(entity_id);
    }

    if (render_proxies.Any()) {
        PUSH_RENDER_COMMAND(UpdateEntityDrawData, std::move(render_proxies));
    }
}

} // namespace hyperion
