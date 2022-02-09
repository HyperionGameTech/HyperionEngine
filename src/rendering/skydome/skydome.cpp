#include "skydome.h"
#include "../shader_manager.h"
#include "../environment.h"
#include "../../util/mesh_factory.h"
#include "../../asset/asset_manager.h"
#include "../../math/math_util.h"

namespace hyperion {
const bool SkydomeControl::clouds_in_dome = false;

SkydomeControl::SkydomeControl(Camera *camera)
    : EntityControl(fbom::FBOMObjectType("SKYDOME_CONTROL"), 10.0),
      camera(camera),
      global_time(0.0)
{
}

void SkydomeControl::OnAdded()
{
    shader = ShaderManager::GetInstance()->GetShader<SkydomeShader>(ShaderProperties()
        .Define("CLOUDS", clouds_in_dome));

    dome = AssetManager::GetInstance()->LoadFromFile<Entity>("models/dome.obj");

    if (dome == nullptr) {
        throw std::runtime_error("Could not load skydome model!");
    }

    dome->SetLocalScale(50);
    dome->GetChild(0)->GetRenderable()->SetShader(shader);
    dome->GetChild(0)->GetRenderable()->SetRenderBucket(Renderable::RB_SKY);
    dome->GetChild(0)->GetMaterial().depth_test = false;
    dome->GetChild(0)->GetMaterial().depth_write = false;
    // dome->GetChild(0)->GetMaterial().alpha_blended = true;

    if (!clouds_in_dome) {
        clouds_shader = ShaderManager::GetInstance()->GetShader<CloudsShader>(ShaderProperties());
        clouds_shader->SetCloudColor(Vector4(0.5));

        clouds_quad = MeshFactory::CreateQuad();
        clouds_quad->SetShader(clouds_shader);
        clouds_quad->SetRenderBucket(Renderable::RB_SKY);

        auto clouds_node = std::make_shared<Entity>("clouds");
        clouds_node->Rotate(Quaternion(Vector3::UnitX(), MathUtil::PI / -2.0f));
        clouds_node->Scale(Vector3(250.0));
        clouds_node->SetRenderable(clouds_quad);
        clouds_node->GetMaterial().depth_test = false;
        clouds_node->GetMaterial().depth_write = false;
        clouds_node->GetMaterial().alpha_blended = true;
        clouds_node->GetMaterial().cull_faces = MaterialFaceCull::MaterialFace_None;
        clouds_node->Move(Vector3(0, 10, 0));
        dome->AddChild(clouds_node);
    }

    parent->AddChild(dome);
}

void SkydomeControl::OnRemoved()
{
    parent->RemoveChild(dome);
}

void SkydomeControl::OnUpdate(double dt)
{
    global_time += 0.01;
    if (!clouds_in_dome) {
        clouds_shader->SetGlobalTime(global_time);
        // clouds_shader->SetCloudColor(Environment::GetInstance()->GetSun().GetColor());
    } else {
        shader->SetGlobalTime(global_time);
    }
}

std::shared_ptr<EntityControl> SkydomeControl::CloneImpl()
{
    return std::make_shared<SkydomeControl>(nullptr); // TODO
}
} // namespace hyperion
