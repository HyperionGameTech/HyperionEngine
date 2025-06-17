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

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <util/MeshBuilder.hpp>

namespace hyperion {

SkySystem::SkySystem(EntityManager& entity_manager)
    : SystemBase(entity_manager)
{
    // m_delegate_handlers.Add(
    //     NAME("OnWorldChange"),
    //     OnWorldChanged.Bind([this](World* new_world, World* previous_world)
    //         {
    //             Threads::AssertOnThread(g_game_thread);

    //             for (auto [entity_id, sky_component] : GetEntityManager().GetEntitySet<SkyComponent>().GetScopedView(GetComponentInfos()))
    //             {
    //                 if (sky_component.render_subsystem)
    //                 {
    //                     sky_component.render_subsystem->RemoveFromEnvironment();
    //                     sky_component.render_subsystem.Reset();
    //                 }

    //                 MeshComponent* mesh_component = GetEntityManager().TryGetComponent<MeshComponent>(entity_id);

    //                 AddRenderSubsystemToEnvironment(new_world, GetEntityManager(), Handle<Entity> { entity_id }, sky_component, mesh_component);
    //             }
    //         }));
}

void SkySystem::OnEntityAdded(const Handle<Entity>& entity)
{
    SystemBase::OnEntityAdded(entity);

    HYP_LOG(ECS, Debug, "Adding sky system for entity: #{}, Scene: {}", entity->GetID().Value(), GetScene()->GetName());

    SkyComponent& sky_component = GetEntityManager().GetComponent<SkyComponent>(entity);
    MeshComponent* mesh_component = GetEntityManager().TryGetComponent<MeshComponent>(entity);

    if (sky_component.render_subsystem)
    {
        sky_component.render_subsystem->RemoveFromEnvironment();
        sky_component.render_subsystem.Reset();
    }

    AddRenderSubsystemToEnvironment(GetWorld(), GetEntityManager(), entity, sky_component, mesh_component);

    GetEntityManager().AddTag<EntityTag::UPDATE_RENDER_PROXY>(entity);
}

void SkySystem::OnEntityRemoved(ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity);

    HYP_LOG(ECS, Debug, "Removing sky system for entity: #{}, Scene: {}", entity.Value(), GetScene()->GetName());

    SkyComponent& sky_component = GetEntityManager().GetComponent<SkyComponent>(entity);
    MeshComponent& mesh_component = GetEntityManager().GetComponent<MeshComponent>(entity);

    if (sky_component.render_subsystem)
    {
        sky_component.render_subsystem->RemoveFromEnvironment();
        sky_component.render_subsystem.Reset();
    }
}

void SkySystem::Process(float delta)
{
    for (auto [entity_id, sky_component] : GetEntityManager().GetEntitySet<SkyComponent>().GetScopedView(GetComponentInfos()))
    {
        if (!sky_component.render_subsystem)
        {
            continue;
        }

        const Handle<EnvProbe>& env_probe = sky_component.render_subsystem->GetEnvProbe();

        if (!env_probe.IsValid() || !env_probe->IsReady())
        {
            continue;
        }

        env_probe->Update(delta);
    }
}

void SkySystem::AddRenderSubsystemToEnvironment(World* world, EntityManager& mgr, const Handle<Entity>& entity, SkyComponent& sky_component, MeshComponent* mesh_component)
{
    if (!world)
    {
        return;
    }

    if (sky_component.render_subsystem)
    {
        world->GetRenderResource().GetEnvironment()->AddRenderSubsystem(sky_component.render_subsystem);
    }
    else
    {
        sky_component.render_subsystem = world->GetRenderResource().GetEnvironment()->AddRenderSubsystem<SkydomeRenderer>(Name::Unique("sky_renderer"));

        Handle<Mesh> mesh = mesh_component ? mesh_component->mesh : Handle<Mesh>::empty;
        Handle<Material> material = mesh_component ? mesh_component->material : Handle<Material>::empty;

        if (!mesh.IsValid())
        {
            mesh = MeshBuilder::Cube();
            // mesh_component.mesh->InvertNormals();
            InitObject(mesh);
        }

        if (!material)
        {
            MaterialAttributes material_attributes {};
            material_attributes.shader_definition = ShaderDefinition {
                NAME("Skybox"),
                ShaderProperties(mesh->GetVertexAttributes())
            };
            material_attributes.bucket = Bucket::BUCKET_SKYBOX;
            material_attributes.cull_faces = FaceCullMode::FRONT;
            material_attributes.flags = MaterialAttributeFlags::DEPTH_TEST;

            material = CreateObject<Material>(NAME("SkyboxMaterial"), material_attributes);
            /// FIXME: Change to EnvProbe's PrefilteredEnvMap
            material->SetTexture(MaterialTextureKey::ALBEDO_MAP, sky_component.render_subsystem->GetCubemap());

            InitObject(material);
        }

        if (mesh_component)
        {
            mesh_component->mesh = std::move(mesh);
            mesh_component->material = std::move(material);

            mgr.AddTag<EntityTag::UPDATE_RENDER_PROXY>(entity);
        }
        else
        {
            mgr.AddComponent<MeshComponent>(entity, MeshComponent { std::move(mesh), std::move(material) });
        }
    }
}

} // namespace hyperion
