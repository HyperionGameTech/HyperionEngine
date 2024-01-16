#include <scene/ecs/systems/SkySystem.hpp>
#include <scene/ecs/EntityManager.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/render_components/sky/SkydomeRenderer.hpp>
#include <util/MeshBuilder.hpp>

#include <Engine.hpp>

namespace hyperion::v2 {

void SkySystem::Process(EntityManager &entity_manager, GameCounter::TickUnit delta)
{
    for (auto [entity_id, sky_component] : entity_manager.GetEntitySet<SkyComponent>()) {
        if (!sky_component.render_component) {
            // @TODO way to get rid of it
            sky_component.render_component = entity_manager.GetScene()->GetEnvironment()->AddRenderComponent<SkydomeRenderer>(
                Name::Unique("sky_renderer")
            );
        }

        if (!entity_manager.HasComponent<MeshComponent>(entity_id)) {
            MeshComponent mesh_component;

            mesh_component.mesh = MeshBuilder::Cube();
            InitObject(mesh_component.mesh);

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

            entity_manager.AddComponent(entity_id, std::move(mesh_component));
        }
    }
}

} // namespace hyperion::v2