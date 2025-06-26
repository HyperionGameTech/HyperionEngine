/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/SkySystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <scene/Mesh.hpp>
#include <scene/Material.hpp>
#include <scene/World.hpp>
#include <scene/Texture.hpp>

#include <rendering/RenderEnvironment.hpp>
#include <rendering/Shader.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderTexture.hpp>

#include <rendering/subsystems/sky/SkydomeRenderer.hpp>

#include <core/containers/HashSet.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <util/MeshBuilder.hpp>

#include <Engine.hpp>

namespace hyperion {

SkySystem::SkySystem(EntityManager& entity_manager)
    : SystemBase(entity_manager)
{
}

void SkySystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    AssertThrow(GetWorld() != nullptr);

    HYP_LOG(ECS, Debug, "Adding sky system for entity: #{}, Scene: {}", entity->GetID(), GetScene()->GetName());

    SkyComponent& sky_component = GetEntityManager().GetComponent<SkyComponent>(entity);
    MeshComponent* mesh_component = GetEntityManager().TryGetComponent<MeshComponent>(entity);

    if (sky_component.subsystem)
    {
        GetWorld()->RemoveSubsystem(sky_component.subsystem);
    }

    AddRenderSubsystemToEnvironment(GetWorld(), GetEntityManager(), entity, sky_component, mesh_component);

    GetEntityManager().AddTag<EntityTag::UPDATE_RENDER_PROXY>(entity);
}

void SkySystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    AssertThrow(GetWorld() != nullptr);

    HYP_LOG(ECS, Debug, "Removing sky system for entity: #{}, Scene: {}", entity->GetID(), GetScene()->GetName());

    SkyComponent& sky_component = GetEntityManager().GetComponent<SkyComponent>(entity);
    MeshComponent& mesh_component = GetEntityManager().GetComponent<MeshComponent>(entity);

    if (sky_component.subsystem)
    {
        GetWorld()->RemoveSubsystem(sky_component.subsystem);
    }
}

void SkySystem::Process(float delta)
{
    // /// @TODO: Remove this
    // for (auto [entity, sky_component] : GetEntityManager().GetEntitySet<SkyComponent>().GetScopedView(GetComponentInfos()))
    // {
    //     if (!sky_component.render_subsystem)
    //     {
    //         continue;
    //     }

    //     const Handle<EnvProbe>& env_probe = sky_component.render_subsystem->GetEnvProbe();

    //     if (!env_probe.IsValid() || !env_probe->IsReady())
    //     {
    //         continue;
    //     }

    //     // env_probe->Update(delta);
    // }
}

void SkySystem::AddRenderSubsystemToEnvironment(World* world, EntityManager& mgr, Entity* entity, SkyComponent& sky_component, MeshComponent* mesh_component)
{
    AssertThrow(world != nullptr);

    if (sky_component.subsystem)
    {
        world->AddSubsystem(sky_component.subsystem);
    }
    else
    {
        sky_component.subsystem = world->AddSubsystem<SkydomeRenderer>();

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
            material_attributes.bucket = RB_SKYBOX;
            material_attributes.cull_faces = FCM_FRONT;
            material_attributes.flags = MaterialAttributeFlags::DEPTH_TEST;

            material = CreateObject<Material>(NAME("SkyboxMaterial"), material_attributes);
            material->SetTexture(MaterialTextureKey::ALBEDO_MAP, Handle<SkyProbe>(sky_component.subsystem->GetEnvProbe())->GetSkyboxCubemap());

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
