/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/SkySystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <scene/Mesh.hpp>
#include <scene/Material.hpp>
#include <scene/World.hpp>

#include <rendering/RenderEnvironment.hpp>
#include <rendering/Shader.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderWorld.hpp>

#include <rendering/subsystems/sky/SkydomeRenderer.hpp>

#include <core/containers/HashSet.hpp>

#include <util/MeshBuilder.hpp>

namespace hyperion {

SkySystem::SkySystem(EntityManager &entity_manager)
    : System(entity_manager)
{
    m_delegate_handlers.Add(NAME("OnWorldChange"), OnWorldChanged.Bind([this](World *new_world, World *previous_world)
    {
        Threads::AssertOnThread(g_game_thread);

        HashSet<ID<Entity>> entities_to_add_mesh_component;

        for (auto [entity_id, sky_component] : GetEntityManager().GetEntitySet<SkyComponent>().GetScopedView(GetComponentInfos())) {
            if (!GetEntityManager().HasComponent<MeshComponent>(entity_id)) {
                entities_to_add_mesh_component.Insert(entity_id);
            }
        }

        for (ID<Entity> entity_id : entities_to_add_mesh_component) {
            GetEntityManager().AddComponent<MeshComponent>(entity_id, MeshComponent { });
        }

        HashSet<ID<Entity>> updated_entity_ids;

        // Trigger removal and addition of render subsystems
        for (auto [entity_id, sky_component, mesh_component] : GetEntityManager().GetEntitySet<SkyComponent, MeshComponent>().GetScopedView(GetComponentInfos())) {
            if (sky_component.render_subsystem) {
                sky_component.render_subsystem->RemoveFromEnvironment();
            }

            if (!new_world) {
                sky_component.render_subsystem.Reset();
            }

            AddRenderSubsystemToEnvironment(sky_component, mesh_component, new_world);

            updated_entity_ids.Insert(entity_id);
        }

        for (ID<Entity> entity_id : updated_entity_ids) {
            GetEntityManager().AddTag<EntityTag::UPDATE_RENDER_PROXY>(entity_id);
        }
    }));
}

void SkySystem::OnEntityAdded(const Handle<Entity> &entity)
{
    SystemBase::OnEntityAdded(entity);

    SkyComponent &sky_component = GetEntityManager().GetComponent<SkyComponent>(entity);
    
    MeshComponent *mesh_component = GetEntityManager().TryGetComponent<MeshComponent>(entity);

    if (!mesh_component) {
        mesh_component = &GetEntityManager().AddComponent<MeshComponent>(entity, MeshComponent { });
    }

    if (World *world = GetWorld()) {
        AddRenderSubsystemToEnvironment(sky_component, *mesh_component, world);

        GetEntityManager().AddTag<EntityTag::UPDATE_RENDER_PROXY>(entity);
    }
}

void SkySystem::OnEntityRemoved(ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity);

    SkyComponent &sky_component = GetEntityManager().GetComponent<SkyComponent>(entity);
    MeshComponent &mesh_component = GetEntityManager().GetComponent<MeshComponent>(entity);

    if (sky_component.render_subsystem) {
        sky_component.render_subsystem->RemoveFromEnvironment();

        sky_component.render_subsystem = nullptr;
    }

    if (mesh_component.mesh) {
        mesh_component.mesh.Reset();
    }

    if (mesh_component.material) {
        mesh_component.material.Reset();
    }
}

void SkySystem::Process(GameCounter::TickUnit delta)
{
    // do nothing
}

void SkySystem::AddRenderSubsystemToEnvironment(SkyComponent &sky_component, MeshComponent &mesh_component, World *world)
{
    AssertThrow(world != nullptr);
    AssertThrow(world->IsReady());

    if (sky_component.render_subsystem) {
        world->GetRenderResource().GetEnvironment()->AddRenderSubsystem(sky_component.render_subsystem);
    } else {
        sky_component.render_subsystem = GetWorld()->GetRenderResource().GetEnvironment()->AddRenderSubsystem<SkydomeRenderer>(Name::Unique("sky_renderer"));

        if (!mesh_component.mesh) {
            mesh_component.mesh = MeshBuilder::Cube();
            // mesh_component.mesh->InvertNormals();
            InitObject(mesh_component.mesh);
        }

        if (!mesh_component.material) {
            Handle<Material> material = CreateObject<Material>();
            material->SetBucket(Bucket::BUCKET_SKYBOX);
            material->SetTexture(MaterialTextureKey::ALBEDO_MAP, sky_component.render_subsystem->GetCubemap());
            material->SetFaceCullMode(FaceCullMode::FRONT);
            material->SetIsDepthTestEnabled(true);
            material->SetIsDepthWriteEnabled(false);
            material->SetShader(ShaderManager::GetInstance()->GetOrCreate(
                NAME("Skybox"),
                ShaderProperties(mesh_component.mesh->GetVertexAttributes())
            ));

            InitObject(material);

            mesh_component.material = std::move(material);
        }
    }
}

} // namespace hyperion
