/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/SkySystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <rendering/RenderEnvironment.hpp>
#include <rendering/Shader.hpp>
#include <rendering/render_components/sky/SkydomeRenderer.hpp>

#include <util/MeshBuilder.hpp>

namespace hyperion {

void SkySystem::OnEntityAdded(ID<Entity> entity)
{
    SystemBase::OnEntityAdded(entity);

    SkyComponent &sky_component = GetEntityManager().GetComponent<SkyComponent>(entity);
    MeshComponent &mesh_component = GetEntityManager().GetComponent<MeshComponent>(entity);

    if (!sky_component.render_component) {
        sky_component.render_component = GetEntityManager().GetScene()->GetEnvironment()->AddRenderComponent<SkydomeRenderer>(
            Name::Unique("sky_renderer")
        );
    }

    if (!mesh_component.mesh) {
        mesh_component.mesh = MeshBuilder::Cube();
        // mesh_component.mesh->InvertNormals();
        InitObject(mesh_component.mesh);

        mesh_component.flags |= MESH_COMPONENT_FLAG_DIRTY;
    }

    if (!mesh_component.material) {
        Handle<Material> material = CreateObject<Material>();
        material->SetBucket(Bucket::BUCKET_SKYBOX);
        material->SetTexture(MaterialTextureKey::ALBEDO_MAP, sky_component.render_component->GetCubemap());
        material->SetFaceCullMode(FaceCullMode::FRONT);
        material->SetIsDepthTestEnabled(true);
        material->SetIsDepthWriteEnabled(false);
        material->SetShader(ShaderManagerSystem::GetInstance()->GetOrCreate(
            NAME("Skybox"),
            ShaderProperties(mesh_component.mesh->GetVertexAttributes())
        ));

        InitObject(material);

        mesh_component.material = std::move(material);

        mesh_component.flags |= MESH_COMPONENT_FLAG_DIRTY;
    }
}

void SkySystem::OnEntityRemoved(ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity);

    SkyComponent &sky_component = GetEntityManager().GetComponent<SkyComponent>(entity);
    MeshComponent &mesh_component = GetEntityManager().GetComponent<MeshComponent>(entity);

    if (sky_component.render_component) {
        GetEntityManager().GetScene()->GetEnvironment()->RemoveRenderComponent<SkydomeRenderer>(sky_component.render_component->GetName());

        sky_component.render_component = nullptr;
    }
}

void SkySystem::Process(GameCounter::TickUnit delta)
{
    // do nothing
}

} // namespace hyperion
