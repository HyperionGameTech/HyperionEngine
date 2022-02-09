#include "skybox.h"
#include "skybox_shader.h"
#include "../../entity.h"
#include "../shader_manager.h"
#include "../environment.h"
#include "../../util/mesh_factory.h"
#include "../../asset/asset_manager.h"
#include "../../math/math_util.h"

namespace hyperion {
SkyboxControl::SkyboxControl(Camera *camera, const std::shared_ptr<Cubemap> &cubemap)
    : EntityControl(fbom::FBOMObjectType("SKYBOX_CONTROL"), 10.0),
      m_camera(camera),
      m_cubemap(cubemap)
{
}

void SkyboxControl::OnAdded()
{
    m_cube = std::make_shared<Entity>("Skybox");

    m_cube->SetRenderable(MeshFactory::CreateCube());

    m_cube->SetLocalScale(10);
    m_cube->SetLocalTranslation(Vector3(0, 55, 2));
    m_cube->GetRenderable()->SetShader(ShaderManager::GetInstance()->GetShader<SkyboxShader>(ShaderProperties()));
    m_cube->GetRenderable()->SetRenderBucket(Renderable::RB_SKY);
    m_cube->GetMaterial().SetTexture("SkyboxMap", m_cubemap);
    m_cube->GetMaterial().depth_test = false;
    m_cube->GetMaterial().depth_write = false;
    m_cube->GetMaterial().alpha_blended = true;

    parent->AddChild(m_cube);
}

void SkyboxControl::OnRemoved()
{
    parent->RemoveChild(m_cube);
}

void SkyboxControl::OnUpdate(double dt)
{
    if (m_cubemap == nullptr) {
        m_cubemap = Environment::GetInstance()->GetGlobalCubemap();
        m_cube->GetMaterial().SetTexture("SkyboxMap", m_cubemap);
    }
}

std::shared_ptr<EntityControl> SkyboxControl::CloneImpl()
{
    return std::make_shared<SkyboxControl>(nullptr, m_cubemap); // TODO
}
} // namespace hyperion
