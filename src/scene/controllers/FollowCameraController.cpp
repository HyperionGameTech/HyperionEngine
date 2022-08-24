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

void BasicCharacterController::OnUpdate(GameCounter::TickUnit)
{
    // just a simple test case, raycast down into the world

    static const float camera_height = 10.0f;

    if (auto *scene = GetOwner()->GetScene()) {
        if (auto &camera = scene->GetCamera()) {
            const Ray ray {
                .position = camera->GetTranslation() + Vector(0, 1000, 0),
                .direction = Vector3::UnitY() * -1.0f
            };

            if (GetEngine()->GetWorld().GetOctree().TestRay(ray, m_ray_test_results)) {
                RayTestResults triangle_mesh_results;

                auto &hit = m_ray_test_results.Front();
                    // now ray test each result as triangle mesh to find exact hit point

                if (auto lookup_result = GetEngine()->registry.template Lookup<Entity>(Entity::ID { TypeID::ForType<Entity>(), hit.id })) {
                    if (auto &mesh = lookup_result->GetMesh()) {
                        ray.TestTriangleList(
                            mesh->GetVertices(),
                            mesh->GetIndices(),
                            lookup_result->GetTransform(),
                            lookup_result->GetId().value,
                            triangle_mesh_results
                        );
                    }
                }

                if (!triangle_mesh_results.Empty()) {
                    auto &mesh_hit = triangle_mesh_results.Front();

                    camera->SetNextTranslation(mesh_hit.hitpoint + Vector3(0, camera_height, 0));
                }

                m_ray_test_results.Clear();
            }
        }
    }
}

} // namespace hyperion::v2