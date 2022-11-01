#include <ui/controllers/UIButtonController.hpp>
#include <ui/UIText.hpp>
#include <Engine.hpp>
#include <core/Containers.hpp>
#include <math/Transform.hpp>
#include <builders/MeshBuilder.hpp>

namespace hyperion::v2 {

struct UICharMesh
{
    UniquePtr<Mesh> quad_mesh;
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

static UniquePtr<Mesh> OptimizeCharMeshes(Array<UICharMesh> &&char_meshes)
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

UIButtonController::UIButtonController()
    : Controller("UIButtonController", false)
{
}

void UIButtonController::OnAdded()
{
    auto font_texture = GetEngine()->GetAssetManager().Load<Texture>("textures/fontmap.png");
            
    FontMap font_map(
        font_texture,
        Extent2D { 32, 32 }
    );

    // create the quad mesh for the button along with any text etc
    auto char_meshes = BuildCharMeshes(font_map, "hello test");
    auto mesh = OptimizeCharMeshes(std::move(char_meshes));

    GetOwner()->SetMesh(GetEngine()->CreateHandle<Mesh>(std::move(mesh)));
    GetOwner()->SetShader(Handle<Shader>(GetEngine()->shader_manager.GetShader(ShaderManager::Key::BASIC_UI)));

    auto mat = GetEngine()->CreateHandle<Material>();
    mat->SetBucket(Bucket::BUCKET_UI);
    mat->SetTexture(Material::MATERIAL_TEXTURE_ALBEDO_MAP, Handle(font_map.GetTexture()));
    mat->SetFaceCullMode(renderer::FaceCullMode::NONE);
    mat->SetIsAlphaBlended(true);
    GetOwner()->SetMaterial(std::move(mat));
}

void UIButtonController::OnRemoved()
{
}

void UIButtonController::OnTransformUpdate(const Transform &transform)
{
}

} // namespace hyperion::v2