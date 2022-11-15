#include <ui/controllers/UIButtonController.hpp>
#include <ui/UIText.hpp>
#include <Engine.hpp>
#include <core/Containers.hpp>
#include <math/Transform.hpp>
#include <util/MeshBuilder.hpp>

namespace hyperion::v2 {



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

    GetOwner()->SetMesh(GetEngine()->CreateHandle<Mesh>(UIText::BuildTextMesh(font_map, "hello world")));
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