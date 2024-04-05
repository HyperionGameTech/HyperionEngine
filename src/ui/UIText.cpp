#include <ui/UIText.hpp>
#include <ui/UIScene.hpp>

#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>

#include <util/MeshBuilder.hpp>

#include <math/Vector3.hpp>
#include <math/Quaternion.hpp>

#include <Engine.hpp>

namespace hyperion::v2 {
struct UICharMesh
{
    RC<StreamedMeshData>    mesh_data;
    Transform               transform;
};

class CharMeshBuilder
{
public:
    CharMeshBuilder()
    {
        quad_mesh = MeshBuilder::Quad();
        InitObject(quad_mesh);
    }

    Array<UICharMesh> BuildCharMeshes(const FontAtlas &font_atlas, const String &text) const;
    Handle<Mesh> OptimizeCharMeshes(Vec2i screen_size, Array<UICharMesh> &&char_meshes) const;

private:
    Handle<Mesh> quad_mesh;
};

Array<UICharMesh> CharMeshBuilder::BuildCharMeshes(const FontAtlas &font_atlas, const String &text) const
{
    AssertThrowMsg(font_atlas.GetTexture().IsValid(), "Font atlas texture is invalid");

    Array<UICharMesh> char_meshes;
    Vec2f placement;

    const SizeType length = text.Length();

    for (SizeType i = 0; i < length; i++) {
        const utf::u32char ch = text.GetChar(i);

        if (ch == utf::u32char(' ')) {
            // add room for space
            placement.x += 0.5f;

            continue;
        }

        if (ch == utf::u32char('\n')) {
            // reset placement, add room for newline
            placement.x = 0.0f;
            placement.y += 1.0f;

            continue;
        }

        Optional<Glyph::Metrics> glyph_metrics = font_atlas.GetGlyphMetrics(ch);

        if (!glyph_metrics.HasValue()) {
            // @TODO Add a placeholder character for missing glyphs
            continue;
        }

        if (glyph_metrics->metrics.width == 0 || glyph_metrics->metrics.height == 0) {
            // empty width or height will cause issues
            continue;
        }

        const Vec2i char_offset = glyph_metrics->image_position;

        const Vec2f atlas_pixel_size = Vec2f::one / Vec2f(font_atlas.GetDimensions());
        const Vec2f glyph_dimensions = Vec2f(glyph_metrics->metrics.width, glyph_metrics->metrics.height);
        const Vec2f glyph_scaling = Vec2f(glyph_dimensions) / Vec2f(font_atlas.GetCellDimensions());

        UICharMesh char_mesh;
        char_mesh.mesh_data = quad_mesh->GetStreamedMeshData();

        auto ref = char_mesh.mesh_data->AcquireRef();

        Array<Vertex> vertices = ref->GetMeshData().vertices;
        const Array<uint32> &indices = ref->GetMeshData().indices;

        for (Vertex &vert : vertices) {
            const Vec3f current_position = vert.GetPosition();

            Vec2f position = Vec2f { current_position.x, current_position.y } * Vec2f(0.5f, 0.5f) + Vec2f(0.5f, -0.5f);
            // adjust for bearing
            position.y += float(glyph_metrics->metrics.height - glyph_metrics->metrics.bearing_y) / float(glyph_metrics->metrics.height);
            // scale to glyph size
            position *= glyph_scaling;
            // offset from other characters
            position += placement;

            vert.SetPosition(Vec3f { position.x, position.y, current_position.z });
            vert.SetTexCoord0((Vec2f(char_offset) + (vert.GetTexCoord0() * (glyph_dimensions - 1))) * (atlas_pixel_size));
        }

        char_mesh.mesh_data = StreamedMeshData::FromMeshData({
            std::move(vertices),
            indices
        });

        placement.x += float(glyph_metrics->metrics.advance / 64) / float(font_atlas.GetCellDimensions().width);

        char_meshes.PushBack(std::move(char_mesh));
    }

    return char_meshes;
}

Handle<Mesh> CharMeshBuilder::OptimizeCharMeshes(Vec2i screen_size, Array<UICharMesh> &&char_meshes) const
{
    if (char_meshes.Empty()) {
        return { };
    }

    Transform base_transform;
    // base_transform.SetTranslation(Vec3f { 0.0f, 1.0f, 0.0f });
    // base_transform.SetScale(Vec3f { 0.5f, 0.5f, 1.0f });

    Handle<Mesh> char_mesh = CreateObject<Mesh>(char_meshes[0].mesh_data);
    Handle<Mesh> transformed_mesh = MeshBuilder::ApplyTransform(char_mesh.Get(), base_transform * char_meshes[0].transform);

    for (SizeType i = 1; i < char_meshes.Size(); i++) {
        char_mesh = CreateObject<Mesh>(char_meshes[i].mesh_data);

        transformed_mesh = MeshBuilder::Merge(
            transformed_mesh.Get(),
            char_mesh.Get(),
            Transform::identity,
            base_transform * char_meshes[i].transform
        );
    }

    InitObject(transformed_mesh);

    return transformed_mesh;
}

// UIText

Handle<Mesh> UIText::BuildTextMesh(const FontAtlas &font_atlas, const String &text) const
{
    CharMeshBuilder char_mesh_builder;

    return char_mesh_builder.OptimizeCharMeshes(m_parent->GetSurfaceSize(), char_mesh_builder.BuildCharMeshes(font_atlas, text));
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

    // Update bounding box, size
    UIObject::UpdateSize();
}

Handle<Material> UIText::GetMaterial() const
{
    return g_material_system->GetOrCreate(
        MaterialAttributes {
            .shader_definition  = ShaderDefinition { HYP_NAME(UIObject), ShaderProperties(static_mesh_vertex_attributes, { "TYPE_TEXT" }) },
            .bucket             = Bucket::BUCKET_UI,
            .blend_mode         = BlendMode::NORMAL,
            .cull_faces         = FaceCullMode::NONE
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