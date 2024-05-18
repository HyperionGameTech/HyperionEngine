/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIText.hpp>
#include <ui/UIStage.hpp>

#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>

#include <util/MeshBuilder.hpp>

#include <math/Vector3.hpp>
#include <math/Quaternion.hpp>

#include <Engine.hpp>

namespace hyperion {
struct UICharMesh
{
    RC<StreamedMeshData>    mesh_data;
    BoundingBox             aabb;
    Transform               transform;
};

class CharMeshBuilder
{
public:
    CharMeshBuilder(const UITextOptions &options)
        : m_options(options)
    {
        m_quad_mesh = MeshBuilder::Quad();
        InitObject(m_quad_mesh);
    }

    Array<UICharMesh> BuildCharMeshes(const FontAtlas &font_atlas, const String &text) const;
    Handle<Mesh> OptimizeCharMeshes(Vec2i screen_size, Array<UICharMesh> &&char_meshes) const;

private:
    Handle<Mesh>    m_quad_mesh;

    UITextOptions   m_options;
};

Array<UICharMesh> CharMeshBuilder::BuildCharMeshes(const FontAtlas &font_atlas, const String &text) const
{
    Array<UICharMesh> char_meshes;
    
    Vec2f placement;

    const SizeType length = text.Length();
    
    const Extent2D cell_dimensions = font_atlas.GetCellDimensions();
    AssertThrowMsg(cell_dimensions.width != 0 && cell_dimensions.height != 0, "Cell dimensions are invalid");

    AssertThrowMsg(font_atlas.GetAtlases() != nullptr, "Font atlas invalid");

    const Handle<Texture> &main_texture_atlas = font_atlas.GetAtlases()->GetMainAtlas();
    AssertThrowMsg(main_texture_atlas.IsValid(), "Main texture atlas is invalid");

    const Vec2f atlas_pixel_size = Vec2f::One() / Vec2f(Extent2D(main_texture_atlas->GetExtent()));

    for (SizeType i = 0; i < length; i++) {
        const utf::u32char ch = text.GetChar(i);

        if (ch == utf::u32char(' ')) {
            // add room for space
            placement.x += float(cell_dimensions.width) * 0.5f;

            continue;
        }

        if (ch == utf::u32char('\n')) {
            // reset placement, add room for newline
            placement.x = 0.0f;
            placement.y += float(cell_dimensions.height);

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

        const float cell_dimensions_ratio = float(cell_dimensions.width) / float(cell_dimensions.height);

        const Vec2f glyph_dimensions = Vec2f { float(glyph_metrics->metrics.width), float(glyph_metrics->metrics.height) };
        const Vec2f glyph_scaling = Vec2f(glyph_dimensions) / (Vec2f(cell_dimensions));

        UICharMesh char_mesh;
        char_mesh.mesh_data = m_quad_mesh->GetStreamedMeshData();

        auto ref = char_mesh.mesh_data->AcquireRef();

        Array<Vertex> vertices = ref->GetMeshData().vertices;
        const Array<uint32> &indices = ref->GetMeshData().indices;

        const float bearing_y = float(glyph_metrics->metrics.height - glyph_metrics->metrics.bearing_y);
        const float char_width = float(glyph_metrics->metrics.advance / 64);

        for (Vertex &vert : vertices) {
            const Vec3f current_position = vert.GetPosition();

            Vec2f position = (Vec2f { current_position.x, current_position.y } + Vec2f { 1.0f, 1.0f }) * 0.5f;
            position *= glyph_dimensions;
            position.y += float(cell_dimensions.height) - (glyph_dimensions.y);
            position.y += bearing_y;
            position += placement;

            vert.SetPosition(Vec3f { position.x, position.y, current_position.z });
            vert.SetTexCoord0((Vec2f(char_offset) + (vert.GetTexCoord0() * (glyph_dimensions))) * atlas_pixel_size);

            // char_mesh.aabb.Extend(Vec3f { position.x, position.y - bearing_y, current_position.z });
        }

        char_mesh.aabb.Extend(Vec3f(placement.x, placement.y, 0.0f));
        char_mesh.aabb.Extend(Vec3f(placement.x + float(glyph_dimensions.x), placement.y + float(cell_dimensions.height), 0.0f));

        char_mesh.mesh_data = StreamedMeshData::FromMeshData({
            std::move(vertices),
            indices
        });

        placement.x += char_width;

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

    BoundingBox aabb = char_meshes[0].aabb;

    Handle<Mesh> char_mesh = CreateObject<Mesh>(char_meshes[0].mesh_data);
    Handle<Mesh> transformed_mesh = MeshBuilder::ApplyTransform(char_mesh.Get(), base_transform * char_meshes[0].transform);

    for (SizeType i = 1; i < char_meshes.Size(); i++) {
        aabb.Extend(char_meshes[i].aabb);

        char_mesh = CreateObject<Mesh>(char_meshes[i].mesh_data);

        transformed_mesh = MeshBuilder::Merge(
            transformed_mesh.Get(),
            char_mesh.Get(),
            Transform::identity,
            base_transform * char_meshes[i].transform
        );
    }

    InitObject(transformed_mesh);

    // Override mesh AABB with custom calculated AABB
    transformed_mesh->SetAABB(aabb);

    return transformed_mesh;
}

#pragma region UIText

UIText::UIText(UIStage *parent, NodeProxy node_proxy)
    : UIObject(parent, std::move(node_proxy), UIObjectType::TEXT),
      m_text("No text set"),
      m_text_color(Vec4f { 0.0f, 0.0f, 0.0f, 1.0f })
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

FontAtlas *UIText::GetFontAtlasOrDefault() const
{
    return m_font_atlas != nullptr
        ? m_font_atlas.Get()
        : (m_parent != nullptr ? m_parent->GetDefaultFontAtlas().Get() : nullptr);
}

void UIText::SetFontAtlas(RC<FontAtlas> font_atlas)
{
    m_font_atlas = std::move(font_atlas);

    UpdateMesh();
}

void UIText::SetTextColor(const Vec4f &color)
{
    m_text_color = color;

    UpdateMaterial();
}

void UIText::UpdateMesh()
{
    Scene *scene = GetScene();

    if (scene == nullptr) {
        return;
    }

    MeshComponent &mesh_component = scene->GetEntityManager()->GetComponent<MeshComponent>(GetEntity());

    Handle<Mesh> mesh;

    if (FontAtlas *font_atlas = GetFontAtlasOrDefault()) {
        CharMeshBuilder char_mesh_builder(m_options);

        mesh = char_mesh_builder.OptimizeCharMeshes(m_parent->GetSurfaceSize(), char_mesh_builder.BuildCharMeshes(*font_atlas, m_text));
    } else {
        DebugLog(LogType::Warn, "No font atlas for UIText %s", *GetName());
    }

    if (!mesh.IsValid()) {
        mesh = GetQuadMesh();
    }

    AssertThrow(mesh.IsValid());
    m_text_aabb = mesh->GetAABB();

    g_safe_deleter->SafeRelease(std::move(mesh_component.mesh));

    mesh_component.mesh = mesh;
    mesh_component.flags |= MESH_COMPONENT_FLAG_DIRTY;
}

Handle<Material> UIText::GetMaterial() const
{
    const Vec2i actual_size = GetActualSize();
    const uint pixel_size = MathUtil::Max(actual_size.y, 1);

    Handle<Texture> font_atlas_texture;

    FontAtlas *font_atlas = GetFontAtlasOrDefault();

    if (font_atlas != nullptr && font_atlas->GetAtlases() != nullptr) {
        font_atlas_texture = font_atlas->GetAtlases()->GetAtlasForPixelSize(pixel_size);
    }

    return g_material_system->GetOrCreate(
        MaterialAttributes {
            .shader_definition  = ShaderDefinition { HYP_NAME(UIObject), ShaderProperties(static_mesh_vertex_attributes, { "TYPE_TEXT" }) },
            .bucket             = Bucket::BUCKET_UI,
            .blend_function     = BlendFunction(BlendModeFactor::SRC_ALPHA, BlendModeFactor::ONE_MINUS_SRC_ALPHA,
                                                BlendModeFactor::ONE, BlendModeFactor::ONE_MINUS_SRC_ALPHA),
            .cull_faces         = FaceCullMode::BACK,
            .flags              = MaterialAttributeFlags::NONE
        },
        {
            { Material::MATERIAL_KEY_ALBEDO, m_text_color }
        },
        {
            { Material::MATERIAL_TEXTURE_ALBEDO_MAP, font_atlas_texture }
        }
    );
}

void UIText::UpdateSize(bool update_children)
{
    UIObject::UpdateSize(update_children);

    if (NodeProxy node = GetNode()) {
        const Vec3f aabb_extent = node->GetEntityAABB().GetExtent();

        const bool was_transform_locked = node->IsTransformLocked();

        if (was_transform_locked) {
            node->UnlockTransform();
        }

        node->SetWorldScale(Vec3f {
            float(m_actual_size.x) / MathUtil::Max(aabb_extent.x, MathUtil::epsilon_f),
            float(m_actual_size.y) / MathUtil::Max(aabb_extent.y, MathUtil::epsilon_f),
            1.0f
        });

        if (was_transform_locked) {
            node->LockTransform();
        }
    }

    // Update material to get new font size if necessary
    UpdateMaterial();
}

BoundingBox UIText::CalculateAABB() const
{
    return m_text_aabb;
}

#pragma endregion UIText

} // namespace hyperion