#include <ui/UIImage.hpp>

#include <Engine.hpp>

namespace hyperion::v2 {

UIImage::UIImage(ID<Entity> entity, UIScene *parent, NodeProxy node_proxy)
    : UIObject(entity, parent, std::move(node_proxy))
{
}

void UIImage::SetTexture(Handle<Texture> texture)
{
    if (texture == m_texture) {
        return;
    }

    if (m_texture.IsValid()) {
        g_safe_deleter->SafeReleaseHandle(std::move(m_texture));
    }

    m_texture = std::move(texture);

    InitObject(m_texture);

    UpdateMaterial();
}

Handle<Material> UIImage::GetMaterial() const
{
    return g_material_system->GetOrCreate(
        MaterialAttributes {
            .shader_definition  = ShaderDefinition { HYP_NAME(UIObject), ShaderProperties(static_mesh_vertex_attributes, { "TYPE_IMAGE" }) },
            .bucket             = Bucket::BUCKET_UI,
            .blend_function     = BlendFunction(BlendModeFactor::SRC_ALPHA, BlendModeFactor::ONE_MINUS_SRC_ALPHA,
                                                BlendModeFactor::ONE, BlendModeFactor::ONE_MINUS_SRC_ALPHA),
            .cull_faces         = FaceCullMode::BACK,
            .flags              = MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_NONE,
            .z_layer            = GetDepth()
        },
        {
            { Material::MATERIAL_KEY_ALBEDO, Vec4f { 1.0f, 1.0f, 1.0f, 1.0f } }
        },
        {
            { Material::MATERIAL_TEXTURE_ALBEDO_MAP, m_texture }
        }
    );
}

} // namespace hyperion::v2
