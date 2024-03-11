#include <scene/ecs/systems/SkySystem.hpp>
#include <scene/ecs/EntityManager.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/render_components/sky/SkydomeRenderer.hpp>
#include <util/MeshBuilder.hpp>

#include <Engine.hpp>

namespace hyperion::v2 {

void SkySystem::OnEntityAdded(EntityManager &entity_manager, ID<Entity> entity)
{
    SystemBase::OnEntityAdded(entity_manager, entity);

    SkyComponent &sky_component = entity_manager.GetComponent<SkyComponent>(entity);
    MeshComponent &mesh_component = entity_manager.GetComponent<MeshComponent>(entity);

    if (!sky_component.render_component) {
        sky_component.render_component = entity_manager.GetScene()->GetEnvironment()->AddRenderComponent<SkydomeRenderer>(
            Name::Unique("sky_renderer")
        );
    }

    if (!mesh_component.mesh) {
        mesh_component.mesh = MeshBuilder::Cube();
        InitObject(mesh_component.mesh);

        mesh_component.flags |= MESH_COMPONENT_FLAG_DIRTY;
    }

    if (!mesh_component.material) {
        Handle<Material> material = CreateObject<Material>();
        material->SetBucket(Bucket::BUCKET_SKYBOX);
        material->SetTexture(Material::TextureKey::MATERIAL_TEXTURE_ALBEDO_MAP, sky_component.render_component->GetCubemap());
        material->SetFaceCullMode(FaceCullMode::FRONT);
        material->SetIsDepthTestEnabled(false);
        material->SetIsDepthWriteEnabled(false);
        material->SetShader(g_shader_manager->GetOrCreate(
            HYP_NAME(Skybox),
            ShaderProperties(mesh_component.mesh->GetVertexAttributes())
        ));

        InitObject(material);

        mesh_component.material = std::move(material);

        mesh_component.flags |= MESH_COMPONENT_FLAG_DIRTY;
    }
}

void SkySystem::OnEntityRemoved(EntityManager &entity_manager, ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity_manager, entity);

    SkyComponent &sky_component = entity_manager.GetComponent<SkyComponent>(entity);
    MeshComponent &mesh_component = entity_manager.GetComponent<MeshComponent>(entity);

    if (sky_component.render_component) {
        entity_manager.GetScene()->GetEnvironment()->RemoveRenderComponent<SkydomeRenderer>(sky_component.render_component->GetName());

        sky_component.render_component = nullptr;
    }
}

void SkySystem::Process(EntityManager &entity_manager, GameCounter::TickUnit delta)
{
    for (auto [entity_id, sky_component, mesh_component] : entity_manager.GetEntitySet<SkyComponent, MeshComponent>()) {

    }
}

} // namespace hyperion::v2
