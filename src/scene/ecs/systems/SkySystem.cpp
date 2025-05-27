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

SkySystem::SkySystem(EntityManager& entity_manager)
    : System(entity_manager)
{
}

void SkySystem::OnEntityAdded(const Handle<Entity>& entity)
{
    SystemBase::OnEntityAdded(entity);

    SkyComponent& sky_component = GetEntityManager().GetComponent<SkyComponent>(entity);
    MeshComponent* mesh_component = GetEntityManager().TryGetComponent<MeshComponent>(entity);

    if (sky_component.render_subsystem)
    {
        sky_component.render_subsystem->RemoveFromEnvironment();
        sky_component.render_subsystem.Reset();
    }

    AddRenderSubsystemToEnvironment(GetEntityManager(), entity, sky_component, mesh_component);

    GetEntityManager().AddTag<EntityTag::UPDATE_RENDER_PROXY>(entity);
}

void SkySystem::OnEntityRemoved(ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity);

    SkyComponent& sky_component = GetEntityManager().GetComponent<SkyComponent>(entity);
    MeshComponent& mesh_component = GetEntityManager().GetComponent<MeshComponent>(entity);

    if (sky_component.render_subsystem)
    {
        sky_component.render_subsystem->RemoveFromEnvironment();
        sky_component.render_subsystem.Reset();
    }
}

void SkySystem::Process(GameCounter::TickUnit delta)
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

void SkySystem::AddRenderSubsystemToEnvironment(EntityManager& mgr, const Handle<Entity>& entity, SkyComponent& sky_component, MeshComponent* mesh_component)
{
    if (sky_component.render_subsystem)
    {
        GetScene()->GetRenderResource().GetEnvironment()->AddRenderSubsystem(sky_component.render_subsystem);
    }
    else
    {
        sky_component.render_subsystem = GetScene()->GetRenderResource().GetEnvironment()->AddRenderSubsystem<SkydomeRenderer>(Name::Unique("sky_renderer"));

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
            material = CreateObject<Material>();
            material->SetBucket(Bucket::BUCKET_SKYBOX);
            material->SetTexture(MaterialTextureKey::ALBEDO_MAP, sky_component.render_subsystem->GetCubemap());
            material->SetFaceCullMode(FaceCullMode::FRONT);
            material->SetIsDepthTestEnabled(true);
            material->SetIsDepthWriteEnabled(false);
            material->SetShader(ShaderManager::GetInstance()->GetOrCreate(
                NAME("Skybox"),
                ShaderProperties(mesh->GetVertexAttributes())));

            InitObject(material);
        }

        if (mesh_component)
        {
            mesh_component->mesh = std::move(mesh);
            mesh_component->material = std::move(material);
        }
        else
        {
            mgr.AddComponent<MeshComponent>(entity, MeshComponent { std::move(mesh), std::move(material) });
        }
    }
}

} // namespace hyperion
