#ifndef HYPERION_V2_PAGING_CONTROLLER_H
#define HYPERION_V2_PAGING_CONTROLLER_H

#include "../Controller.hpp"
#include <scene/Entity.hpp>

#include <rendering/backend/RendererStructs.hpp>
#include <core/lib/FlatSet.hpp>
#include <core/lib/FlatMap.hpp>
#include <core/lib/Queue.hpp>

#include <math/Vector2.hpp>
#include <math/Vector3.hpp>

#include <Types.hpp>

#include <array>

namespace hyperion::v2 {

using renderer::Extent3D;

using PatchCoord = Vector2;

struct PatchNeighbor
{
    PatchCoord coord{};

    Vector2 GetCenter() const { return coord - 0.5f; }
};

using PatchNeighbors = std::array<PatchNeighbor, 8>;

enum class PageState
{
    UNLOADED,
    UNLOADING,
    WAITING,
    LOADED
};

struct PatchInfo
{
    Extent3D       extent;
    PatchCoord     coord { 0.0f };
    Vector3        scale { 1.0f };
    PageState      state { PageState::UNLOADED };
    float          unload_timer { 0.0f };
    PatchNeighbors neighbors{};
};

struct Patch
{
    PatchInfo info;
    Ref<Entity> entity;

    Vector2 GetCenter() const { return info.coord - 0.5f; }
};

class PagingController : public Controller
{
    static constexpr GameCounter::TickUnit update_max { 0.0f };
    static constexpr GameCounter::TickUnit queue_max { 0.0f };
    static constexpr GameCounter::TickUnit patch_unload_time { 0.0f };

public:
    PagingController(
        const char *name,
        Extent3D patch_size,
        const Vector3 &scale,
        Float max_distance
    );
    virtual ~PagingController() override = default;

    virtual void OnAdded() override;
    virtual void OnRemoved() override;
    virtual void OnUpdate(GameCounter::TickUnit delta) override;

protected:
    virtual void OnPatchAdded(Patch *patch) = 0;
    virtual void OnPatchRemoved(Patch *patch) = 0;

    struct PatchUpdate {
        PatchCoord coord;
        PageState new_state;
    };

    static PatchNeighbors GetNeighbors(const PatchCoord &coord);

    auto FindPatch(const PatchCoord &coord) -> FlatMap<PatchCoord, std::unique_ptr<Patch>>::Iterator
    {
        return m_patches.Find(coord);
    }

    Patch *GetPatch(const PatchCoord &coord)
    {
        const auto it = FindPatch(coord);

        if (it == m_patches.End()) {
            return nullptr;
        }

        return it->second.get();
    }

    PatchCoord WorldSpaceToCoord(const Vector3 &position) const;
    bool InRange(const Patch *patch, const PatchCoord &camera_coord) const;
    bool InRange(const PatchCoord &patch_center, const PatchCoord &camera_coord) const;

    virtual std::unique_ptr<Patch> CreatePatch(const PatchInfo &info);
    void AddPatch(const PatchCoord &coord);
    void RemovePatch(const PatchCoord &coord);
    void EnqueuePatch(const PatchCoord &coord);
    void PushUpdate(const PatchUpdate &update);
    void InitPatch(Patch *patch);

    FlatMap<PatchCoord, std::unique_ptr<Patch>> m_patches;
    Queue<PatchUpdate>                          m_queue;
    FlatSet<PatchCoord>                         m_queued_neighbors; // neighbors queued for addition, so we don't add duplicates
    Extent3D                                    m_patch_size;
    Vector3                                     m_scale;
    Float                                       m_max_distance;
    GameCounter::TickUnit                       m_update_timer;
    GameCounter::TickUnit                       m_queue_timer;
};

} // namespace hyperion::v2

#endif
