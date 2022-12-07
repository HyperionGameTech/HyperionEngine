#include <ui/UIText.hpp>
#include <util/MeshBuilder.hpp>
#include <math/Vector3.hpp>
#include <math/Quaternion.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {
struct UICharMesh
{
    Handle<Mesh> quad_mesh;
    Transform transform;
};

static Array<UICharMesh> BuildCharMeshes(const FontMap &font_map, const String &text)
{
    Array<UICharMesh> char_meshes;
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

static Handle<Mesh> OptimizeCharMeshes(Array<UICharMesh> &&char_meshes)
{
    if (char_meshes.Empty()) {
        return { };
    }

    Transform base_transform;
    base_transform.SetTranslation(Vector3(1.0f, -1.0f, 0.0f));

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

Handle<Mesh> UIText::BuildTextMesh(const FontMap &font_map, const String &text)
{
    auto char_meshes = BuildCharMeshes(font_map, text);

    return OptimizeCharMeshes(std::move(char_meshes));
}

} // namespace hyperion::v2