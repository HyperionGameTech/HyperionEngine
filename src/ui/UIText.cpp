#include <ui/UIText.hpp>

#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>

#include <util/MeshBuilder.hpp>

#include <math/Vector3.hpp>
#include <math/Quaternion.hpp>

#include <Engine.hpp>

namespace hyperion::v2 {
struct UICharMesh
{
    Handle<Mesh>    quad_mesh;
    Transform       transform;
};

static Array<UICharMesh> BuildCharMeshes(const FontAtlas &font_atlas, const String &text)
{
    AssertThrowMsg(font_atlas.GetTexture().IsValid(), "Font atlas texture is invalid");

    Array<UICharMesh> char_meshes;
    Vec3f placement;

    const SizeType length = text.Length();

    for (SizeType i = 0; i < length; i++) {
        char ch = text[i];

        if (ch == '\n') {
            // reset placement, add room for newline
            placement.x = 0.0f;
            placement.y -= 1.5f;

            continue;
        }

        Optional<Glyph::Metrics> glyph_metrics = font_atlas.GetGlyphMetrics(ch);

        if (!glyph_metrics.HasValue()) {
            // @TODO Add a placeholder character
            continue;
        }

        UICharMesh char_mesh;
        char_mesh.transform.SetTranslation(placement);
        char_mesh.quad_mesh = MeshBuilder::Quad();

        const Vec2i char_offset = glyph_metrics->image_position;
        const Vec2f atlas_image_scaling = Vec2f::one / Vec2f(font_atlas.GetDimensions());
        const Vec2f glyph_scaling = Vec2f::one / Vec2f(glyph_metrics->metrics.width, glyph_metrics->metrics.height);

        auto streamed_mesh_data = char_mesh.quad_mesh->GetStreamedMeshData();
        AssertThrow(streamed_mesh_data != nullptr);

        auto ref = streamed_mesh_data->AcquireRef();

        Array<Vertex> vertices = ref->GetMeshData().vertices;
        const Array<uint32> &indices = ref->GetMeshData().indices;

        for (Vertex &vert : vertices) {
            vert.SetTexCoord0((Vec2f(char_offset) + (vert.GetTexCoord0() * glyph_scaling)) * atlas_image_scaling);
        }

        Mesh::SetStreamedMeshData(
            char_mesh.quad_mesh,
            StreamedMeshData::FromMeshData({
                std::move(vertices),
                indices
            })
        );

        placement.x += float(glyph_metrics->metrics.advance) * (1.0f / 64.0f);

        char_meshes.PushBack(std::move(char_mesh));
    }

    return char_meshes;
}

static Handle<Mesh> OptimizeCharMeshes(Array<UICharMesh> &&char_meshes)
{
    if (char_meshes.Empty()) {
        return { };
    }

    Transform base_transform;
    base_transform.SetTranslation(Vector3(1.0f, -1.0f, 0.0f));

    Handle<Mesh> transformed_mesh = MeshBuilder::ApplyTransform(char_meshes[0].quad_mesh.Get(), base_transform * char_meshes[0].transform);

    for (SizeType i = 1; i < char_meshes.Size(); i++) {
        transformed_mesh = MeshBuilder::Merge(
            transformed_mesh.Get(),
            char_meshes[i].quad_mesh.Get(),
            Transform(),
            base_transform * char_meshes[i].transform
        );
    }

    InitObject(transformed_mesh);

    return transformed_mesh;
}

// UIText

Handle<Mesh> UIText::BuildTextMesh(const FontAtlas &font_atlas, const String &text)
{
    return OptimizeCharMeshes(BuildCharMeshes(font_atlas, text));
}

UIText::UIText(ID<Entity> entity, UIScene *parent)
    : UIObject(entity, parent),
      m_text("No text set"),
      m_font_atlas(parent != nullptr ? parent->GetDefaultFontAtlas() : nullptr)
{
}

void UIText::Init()
{
    UIObject::Init();

    UpdateMesh();
}

void UIText::SetText(const String &text)
{
    m_text = text;

    UpdateMesh();
}

void UIText::SetFontAtlas(RC<FontAtlas> font_atlas)
{
    m_font_atlas = std::move(font_atlas);

    UpdateMesh(true);
}

void UIText::UpdateMesh(bool update_material)
{
    MeshComponent &mesh_component = GetParent()->GetScene()->GetEntityManager()->GetComponent<MeshComponent>(GetEntity());

    mesh_component.mesh = m_font_atlas != nullptr
        ? BuildTextMesh(*m_font_atlas, m_text)
        : Handle<Mesh> { };

    if (update_material || !mesh_component.material.IsValid()) {
        mesh_component.material = GetMaterial();
    }

    mesh_component.flags |= MESH_COMPONENT_FLAG_DIRTY;

    BoundingBoxComponent &bounding_box_component = GetParent()->GetScene()->GetEntityManager()->GetComponent<BoundingBoxComponent>(GetEntity());
    bounding_box_component.local_aabb = mesh_component.mesh.IsValid() ? mesh_component.mesh->GetAABB() : BoundingBox::empty;
}

Handle<Material> UIText::GetMaterial() const
{
    return g_material_system->GetOrCreate(
        MaterialAttributes {
            .shader_definition  = ShaderDefinition { HYP_NAME(UIObject), ShaderProperties(static_mesh_vertex_attributes) },
            .bucket             = Bucket::BUCKET_UI,
            .blend_mode         = BlendMode::NORMAL,
            .cull_faces         = FaceCullMode::NONE,
            .flags              = MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_NONE
        },
        {
            { Material::MATERIAL_KEY_ALBEDO, Vec4f { 1.0f, 1.0f, 1.0f, 1.0f } }
        },
        {
            { Material::MATERIAL_TEXTURE_ALBEDO_MAP, m_font_atlas != nullptr ? m_font_atlas->GetTexture() : Handle<Texture> { } }
        }
    );
}

} // namespace hyperion::v2