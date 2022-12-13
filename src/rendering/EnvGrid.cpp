#include <rendering/EnvGrid.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

const Extent3D EnvGrid::density = Extent3D { 4, 4, 2 };

EnvGrid::EnvGrid(const BoundingBox &aabb)
    : RenderComponent(10),
      m_aabb(aabb)
{
}

EnvGrid::~EnvGrid()
{
}

void EnvGrid::Init()
{
    Handle<Scene> scene(GetParent()->GetScene()->GetID());
    AssertThrow(scene.IsValid());

    const SizeType total_count = density.Size();
    const Vector3 aabb_extent = m_aabb.GetExtent();

    m_env_probes.Resize(total_count);

    for (SizeType x = 0; x < density.width; x++) {
        for (SizeType y = 0; y < density.height; y++) {
            for (SizeType z = 0; z < density.depth; z++) {
                const SizeType index = x * density.height * density.depth
                     + y * density.depth
                     + z;

                if (m_env_probes[index]) {
                    continue;
                }

                const BoundingBox env_probe_aabb(
                    m_aabb.min + (Vector3(Float(x), Float(y), Float(z)) * (aabb_extent / Vector3(density))),
                    m_aabb.min + (Vector3(Float(x + 1), Float(y + 1), Float(z + 1)) * (aabb_extent / Vector3(density)))
                );

                m_env_probes[index] = CreateObject<EnvProbe>(scene, env_probe_aabb);
            }
        }
    }

    for (auto &env_probe : m_env_probes) {
        std::cout << "Init env probe at " << env_probe->GetAABB() << "\n";

        if (InitObject(env_probe)) {
            GetParent()->GetScene()->AddEnvProbe(env_probe);
        }
    }

    std::cout << "Total probes: " << total_count << std::endl;

}

// called from game thread
void EnvGrid::InitGame()
{
    Threads::AssertOnThread(THREAD_GAME);
}

void EnvGrid::OnRemoved()
{
    for (auto &env_probe : m_env_probes) {
        GetParent()->GetScene()->RemoveEnvProbe(env_probe->GetID());
    }
}

void EnvGrid::OnUpdate(GameCounter::TickUnit delta)
{
    Threads::AssertOnThread(THREAD_GAME);

    // for (auto &env_probe : m_env_probes) {
    //     env_probe->Update();
    // }
}

void EnvGrid::OnRender(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    // TODO: Use paging controller for env probes to dynamically deallocate them etc.
    // only render in certain range.

    for (auto &env_probe : m_env_probes) {
        env_probe->Render(frame);
    }
}

void EnvGrid::OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index /*prev_index*/)
{
    AssertThrowMsg(false, "Not implemented");
}


} // namespace hyperion::v2
