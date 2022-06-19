#ifndef HYPERION_V2_PAGING_CONTROLLER_H
#define HYPERION_V2_PAGING_CONTROLLER_H

#include "../controller.h"

#include <rendering/backend/renderer_structs.h>
#include <core/lib/flat_set.h>

#include <math/vector2.h>
#include <math/vector3.h>

#include <types.h>

#include <array>

namespace hyperion::v2 {

using renderer::Extent3D;

enum class PageState {
    UNLOADED,
    UNLOADING,
    WAITING,
    LOADED
};

class PagingController : public Controller {
    static constexpr GameCounter::TickUnit update_max{1.0f};
    static constexpr GameCounter::TickUnit queue_max{1.0f};
    static constexpr GameCounter::TickUnit patch_unload_time{2.0f};
    static constexpr float                 max_distance{1.0f};

    using Coord = Vector2;

public:
    PagingController(const char *name, Extent3D patch_size, const Vector3 &scale);
    virtual ~PagingController() override = default;

    virtual void OnAdded() override;
    virtual void OnRemoved() override;
    virtual void OnUpdate(GameCounter::TickUnit delta) override;

protected:
    struct Patch;

    virtual void OnPatchAdded(Patch *patch)   = 0;
    virtual void OnPatchRemoved(Patch *patch) = 0;

    struct PatchNeighbor {
        Coord coord{};
        bool  is_queued{false};

        Vector2 GetCenter() const { return coord - 0.5f; }
    };

    using PatchNeighbors = std::array<PatchNeighbor, 8>;
    
    struct PatchInfo {
        Extent3D       extent;
        Coord          coord{0.0f};
        Vector3        scale{1.0f};
        PageState      state{PageState::UNLOADED};
        float          unload_timer{0.0f};
        PatchNeighbors neighbors{};
    };

    struct Patch {
        PatchInfo info;

        Vector2 GetCenter() const { return info.coord - 0.5f; }
    };
    
    using PatchList = std::vector<std::unique_ptr<Patch>>;

    struct PatchUpdate {
        Coord     coord;
        PageState new_state;
    };

    static PatchNeighbors GetNeighbors(const Coord &coord);

    PatchList::iterator FindPatch(const Coord &coord)
    {
        return std::find_if(
            m_patches.begin(),
            m_patches.end(),
            [&coord](const auto &item) {
                return item->info.coord == coord;
            }
        );
    }

    Patch *GetPatch(const Coord &coord)
    {
        const auto it = FindPatch(coord);

        if (it == m_patches.end()) {
            return nullptr;
        }

        return it->get();
    }

    Coord WorldSpaceToCoord(const Vector3 &position) const;
    bool InRange(const Patch *patch, const Coord &camera_coord) const;
    bool InRange(const Coord &patch_center, const Coord &camera_coord) const;

    virtual std::unique_ptr<Patch> CreatePatch(const PatchInfo &info);
    void AddPatch(const Coord &coord);
    void RemovePatch(const Coord &coord);
    void EnqueuePatch(const Coord &coord);
    void PushUpdate(const PatchUpdate &update);
    void InitPatch(Patch *patch);

    PatchList               m_patches;
    std::queue<PatchUpdate> m_queue;
    FlatSet<Coord>          m_queued_neighbors; // neighbors queued for addition, so we don't add duplicates
    Extent3D                m_patch_size;
    Vector3                 m_scale;
    GameCounter::TickUnit   m_update_timer;
    GameCounter::TickUnit   m_queue_timer;
};

} // namespace hyperion::v2

#endif
