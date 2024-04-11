#include <ui/UIPanel.hpp>

#include <Engine.hpp>

namespace hyperion::v2 {

UIPanel::UIPanel(ID<Entity> entity, UIScene *parent)
    : UIObject(entity, parent)
{
}

Handle<Material> UIPanel::GetMaterial() const
{
    return g_material_system->GetOrCreate(
        MaterialAttributes {
            .shader_definition  = ShaderDefinition { HYP_NAME(UIObject), ShaderProperties(static_mesh_vertex_attributes, { "TYPE_PANEL" }) },
            .bucket             = Bucket::BUCKET_UI,
            .blend_mode         = BlendMode::NORMAL,
            .cull_faces         = FaceCullMode::NONE,
            .flags              = MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_NONE,
            .z_layer            = GetDepth()
        }
    );
}

} // namespace hyperion::v2
