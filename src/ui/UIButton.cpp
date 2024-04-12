#include <ui/UIButton.hpp>

#include <Engine.hpp>

namespace hyperion::v2 {

UIButton::UIButton(ID<Entity> entity, UIScene *parent)
    : UIObject(entity, parent)
{
    SetBorderRadius(5);
}

Handle<Material> UIButton::GetMaterial() const
{
    return g_material_system->GetOrCreate(
        MaterialAttributes {
            .shader_definition  = ShaderDefinition { HYP_NAME(UIObject), ShaderProperties(static_mesh_vertex_attributes, { "TYPE_BUTTON" }) },
            .bucket             = Bucket::BUCKET_UI,
            .blend_function     = BlendFunction::AlphaBlending(),
            .cull_faces         = FaceCullMode::BACK,
            .flags              = MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_NONE,
            .z_layer            = GetDepth()
        },
        {
            { Material::MATERIAL_KEY_ALBEDO, Vec4f { 0.05f, 0.055f, 0.075f, 1.0f } }
        },
        {
            { Material::MATERIAL_TEXTURE_ALBEDO_MAP, Handle<Texture> { } }
        }
    );
}

} // namespace hyperion::v2
