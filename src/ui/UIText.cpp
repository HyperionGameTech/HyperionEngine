#include <ui/UIText.hpp>
#include <builders/MeshBuilder.hpp>
#include <math/Vector3.hpp>
#include <math/Quaternion.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

#if 0
UIText::UIText(const FontMap &font_map, const String &text)
    : UIObject(),
      m_font_map(font_map),
      m_text(text)
{
}

UIText::~UIText()
{
}

void UIText::Init(Engine *engine)
{
    // if (IsInitCalled()) {
    //     return;
    // }

    // UIObject::Init(engine);

    // auto char_meshes = BuildCharMeshes(m_font_map, m_text);
    // auto mesh = OptimizeCharMeshes(std::move(char_meshes));

    // UIObject::GetEntity()->SetMesh(engine->CreateHandle<Mesh>(std::move(mesh)));

    // auto mat = engine->CreateHandle<Material>();
    // mat->SetBucket(Bucket::BUCKET_UI);
    // mat->SetTexture(Material::MATERIAL_TEXTURE_ALBEDO_MAP, Handle(m_font_map.GetTexture()));
    // mat->SetFaceCullMode(renderer::FaceCullMode::NONE);
    // mat->SetIsAlphaBlended(true);
    // UIObject::GetEntity()->SetMaterial(std::move(mat));
    // UIObject::GetEntity()->RebuildRenderableAttributes();
}

DynArray<UIText::UICharMesh> UIText::BuildCharMeshes(const FontMap &font_map, const String &text)
{
    DynArray<UIText::UICharMesh> char_meshes;
    Vector3 placement;

    const SizeType length = text.Length();

    for (SizeType i = 0; i < length; i++) {
        auto ch = text[i];

        if (ch == '\n') {
            // reset placement, add room for newline
            placement.x = 0.0f;
            placement.y -= 1.5f;

            continue;
        }

        UICharMesh char_mesh;
        char_mesh.transform.SetTranslation(placement);
        char_mesh.quad_mesh = MeshBuilder::Quad();

        const Vector2 offset = font_map.GetCharOffset(ch);
        const Vector2 scaling = font_map.GetScaling();

        auto vertices = char_mesh.quad_mesh->GetVertices();

        for (auto &vert : vertices) {
            vert.SetTexCoord0(offset + (vert.GetTexCoord0() * scaling));
        }

        char_mesh.quad_mesh->SetVertices(vertices, char_mesh.quad_mesh->GetIndices());

        placement.x += 1.5f;

        char_meshes.PushBack(std::move(char_mesh));
    }

    return char_meshes;
}

UniquePtr<Mesh> UIText::OptimizeCharMeshes(DynArray<UICharMesh> &&char_meshes)
{
    if (char_meshes.Empty()) {
        return { };
    }

    Transform base_transform;
    base_transform.SetTranslation(Vector3(1.0f, -1.0f, 0.0f));
    base_transform.SetRotation(Quaternion(Vector3::UnitX(), MathUtil::DegToRad(180.0f)));

    auto transformed_mesh = MeshBuilder::ApplyTransform(char_meshes[0].quad_mesh.Get(), base_transform * char_meshes[0].transform);

    for (SizeType i = 1; i < char_meshes.Size(); i++) {
        transformed_mesh = MeshBuilder::Merge(
            transformed_mesh.Get(),
            char_meshes[i].quad_mesh.Get(),
            Transform(),
            base_transform * char_meshes[i].transform
        );
    }

    return transformed_mesh;
}
#endif

} // namespace hyperion::v2