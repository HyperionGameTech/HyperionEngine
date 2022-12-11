#include "FollowCameraController.hpp"
#include <Engine.hpp>

namespace hyperion::v2 {

BasicCharacterController::BasicCharacterController()
    : Controller("BasicCharacterController")
{
}

void BasicCharacterController::OnAdded()
{
}

void BasicCharacterController::OnRemoved()
{
}

void BasicCharacterController::OnDetachedFromScene(ID<Scene> id)
{
    if (auto scene = Handle<Scene>(id)) {
        if (scene->IsWorldScene()) {
            if (const auto &camera = scene->GetCamera()) {
                m_camera = camera;
            }
        }
    }
}

void BasicCharacterController::OnAttachedToScene(ID<Scene> id)
{
    if (auto scene = Handle<Scene>(id)) {
        if (scene->GetCamera() == m_camera) {
            m_camera.Reset();
        }
    }
}

void BasicCharacterController::OnUpdate(GameCounter::TickUnit)
{
    // just a simple test case, raycast down into the world

    static const float camera_height = 10.0f;
    
    if (!m_camera) {
        return;
    }

    const Ray ray {
        .position = m_camera->GetTranslation() + Vector(0, 1000, 0),
        .direction = Vector3::UnitY() * -1.0f
    };

    if (Engine::Get()->GetWorld()->GetOctree().TestRay(ray, m_ray_test_results)) {
        RayTestResults triangle_mesh_results;

        auto &hit = m_ray_test_results.Front();
            // now ray test each result as triangle mesh to find exact hit point

        Handle<Entity> entity(ID<Entity> { hit.id });

        if (entity) {
            if (auto &mesh = entity->GetMesh()) {
                ray.TestTriangleList(
                    mesh->GetVertices(),
                    mesh->GetIndices(),
                    entity->GetTransform(),
                    entity->GetID().value,
                    triangle_mesh_results
                );
            }
        }

        if (!triangle_mesh_results.Empty()) {
            auto &mesh_hit = triangle_mesh_results.Front();

            m_camera->SetNextTranslation(mesh_hit.hitpoint + Vector3(0, camera_height, 0));
        }

        m_ray_test_results.Clear();
    }
}

} // namespace hyperion::v2