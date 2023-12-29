#include "PagingController.hpp"

#include "scene/Node.hpp"
#include "scene/Scene.hpp"

namespace hyperion::v2 {

auto PagingController::GetNeighbors(const PatchCoord &coord) -> PatchNeighbors
{
    return {
        PatchNeighbor { coord + PatchCoord { 1, 0 } },
        PatchNeighbor { coord + PatchCoord { -1, 0 } },
        PatchNeighbor { coord + PatchCoord { 0, 1 } },
        PatchNeighbor { coord + PatchCoord { 0, -1 } },
        PatchNeighbor { coord + PatchCoord { 1, -1 } },
        PatchNeighbor { coord + PatchCoord { -1, -1 } },
        PatchNeighbor { coord + PatchCoord { 1, 1 } },
        PatchNeighbor { coord + PatchCoord { -1, 1 } }
    };
}

PagingController::PagingController(
    Extent3D patch_size,
    const Vector3 &scale,
    Float max_distance
) : Controller(),
    m_patch_size(patch_size),
    m_scale(scale),
    m_max_distance(max_distance),
    m_update_timer(0.0f),
    m_queue_timer(0.0f)
{
}

void PagingController::OnAdded()
{
    const PatchCoord origin { 0, 0 };

    //AddPatch(origin);

    //for (const auto neighbor : GetNeighbors(origin)) {
    //    AddPatch(neighbor.coord);
    //}
}

void PagingController::OnRemoved()
{
    std::vector<PatchCoord> patch_coords;
    patch_coords.reserve(m_patches.Size());

    for (auto &it : m_patches) {
        patch_coords.push_back(it.first);
    }

    for (const auto &coord : patch_coords) {
        RemovePatch(coord);
    }
}

void PagingController::OnAttachedToScene(ID<Scene> id)
{
    if (auto scene = Handle<Scene>(id)) {
        if (scene->IsWorldScene()) {
            m_scene = scene;
        }
    }
}

void PagingController::OnDetachedFromScene(ID<Scene> id)
{
    if (m_scene && m_scene->GetID() == id) {
        m_scene.Reset();
    }
}

void PagingController::OnUpdate(GameCounter::TickUnit delta)
{
    if (!m_scene) {
        DebugLog(
            LogType::Warn,
            "PagingController not attached to a Scene\n"
        );

        return;
    }

    if (!m_scene->GetCamera()) {
        DebugLog(
            LogType::Warn,
            "PagingController on Scene with no Camera\n"
        );

        return;
    }

    const PatchCoord camera_coord = WorldSpaceToCoord(m_scene->GetCamera()->GetTranslation());
    // ensure a patch right under the camera exists
    // if all patches are removed we are not able to add any other linked ones otherwise
    if (GetPatch(camera_coord) == nullptr) {
        EnqueuePatch(camera_coord);
    }

    m_update_timer += delta;
    m_queue_timer += delta;

    FlatSet<PatchCoord> patch_coords_in_range;

    for (Int x = MathUtil::Floor(-m_max_distance); x <= MathUtil::Ceil(m_max_distance) + 1; x++) {
        for (Int z = MathUtil::Floor(-m_max_distance); z <= MathUtil::Ceil(m_max_distance) + 1; z++) {
            patch_coords_in_range.Insert(camera_coord + Vector2 { Float(x), Float(z) });
        }
    }

    FlatSet<PatchCoord> patch_coords_to_add = patch_coords_in_range;

    if (m_queue_timer >= queue_max) {
        while (m_queue.Any()) {
            const auto top = m_queue.Front();
            m_queue.Pop();

            Patch *patch = nullptr;

            switch (top.new_state) {
            case PageState::WAITING:
                AddPatch(top.coord);

                break;
            case PageState::UNLOADED:
                RemovePatch(top.coord);
                break;
            default:
                patch = GetPatch(top.coord);

                if (patch == nullptr) {
                    DebugLog(
                        LogType::Warn,
                        "Patch at %f, %f was not found when updating state\n",
                        top.coord.x, top.coord.y
                    );

                    break;
                }

                patch->info.state = top.new_state;

                break;
            }
        }

        m_queue_timer = 0.0f;
    }

    if (m_update_timer >= update_max) {
        for (auto &it : m_patches) {
            auto &patch = it.second;
            AssertThrow(patch != nullptr);

            const bool in_range = patch_coords_in_range.Contains(patch->info.coord);

            if (in_range) {
                patch_coords_to_add.Erase(patch->info.coord);
            }

            switch (patch->info.state) {
            case PageState::LOADED:
                patch->info.unload_timer = 0.0f;

                if (!in_range) {
                    PushUpdate({
                        .coord = patch->info.coord,
                        .new_state = PageState::UNLOADING
                    });
                }

                break;

            case PageState::UNLOADING:
                if (in_range) {
                    PushUpdate({
                        .coord = patch->info.coord,
                        .new_state = PageState::LOADED
                    });
                } else {
                    patch->info.unload_timer += m_update_timer;

                    if (patch->info.unload_timer >= patch_unload_time) {
                        PushUpdate({
                            .coord = patch->info.coord,
                            .new_state = PageState::UNLOADED
                        });
                    }
                }

                break;
            default: break;
            }
        }

        m_update_timer = 0.0f;
    }

    for (const auto &coord : patch_coords_to_add) {
        EnqueuePatch(coord);
    }
}

auto PagingController::WorldSpaceToCoord(const Vector3 &position) const -> PatchCoord
{
    Vector3 scaled = position - GetOwner()->GetTranslation();
    scaled *= Vector3::One() / (m_scale * (Vector3(m_patch_size) - 1.0f));
    scaled = MathUtil::Floor(scaled);

    return PatchCoord{scaled.x, scaled.z};
}

bool PagingController::InRange(const Patch *patch, const PatchCoord &camera_coord) const
{
    return InRange(patch->GetCenter(), camera_coord);
}

bool PagingController::InRange(const PatchCoord &patch_center, const PatchCoord &camera_coord) const
{
    return camera_coord.Distance(patch_center) <= m_max_distance;
}

UniquePtr<Patch> PagingController::CreatePatch(const PatchInfo &info)
{
    return UniquePtr<Patch>::Construct(Patch { info });
}

void PagingController::AddPatch(const PatchCoord &coord)
{
    AssertThrow(GetPatch(coord) == nullptr);

    const PatchInfo info {
        .extent    = m_patch_size,
        .coord     = coord,
        .scale     = m_scale,
        .state     = PageState::LOADED,
        .neighbors = GetNeighbors(coord)
    };

    auto patch = CreatePatch(info);
    InitPatch(patch.Get());
    
    m_patches.Insert(coord, std::move(patch));

    const auto neighbor_it = m_queued_neighbors.Find(coord);

    if (neighbor_it != m_queued_neighbors.End()) {
        m_queued_neighbors.Erase(neighbor_it);
    }
}

void PagingController::RemovePatch(const PatchCoord &coord)
{
    const auto it = FindPatch(coord);

    if (it == m_patches.End()) {
        DebugLog(
            LogType::Warn,
            "Cannot remove patch at [%f, %f] because it does not exist.\n",
            coord.x, coord.y
        );

        return;
    }

    auto &patch = it->second;

    OnPatchRemoved(patch.Get());

    m_patches.Erase(it);

    const auto neighbor_it = m_queued_neighbors.Find(coord);

    if (neighbor_it != m_queued_neighbors.End()) {
        m_queued_neighbors.Erase(neighbor_it);
    }
}

void PagingController::EnqueuePatch(const PatchCoord &coord)
{
    AssertThrow(GetPatch(coord) == nullptr);

    if (!m_queued_neighbors.Contains(coord)) {
        PushUpdate({
            .coord     = coord,
            .new_state = PageState::WAITING
        });

        m_queued_neighbors.Insert(coord);
    }
}

void PagingController::PushUpdate(const PatchUpdate &update)
{
    m_queue.Push(update);
}

void PagingController::InitPatch(Patch *patch)
{
    AssertThrow(patch != nullptr);

    patch->info.state = PageState::LOADED;
    
    OnPatchAdded(patch);
}

} // namespace hyperion::v2