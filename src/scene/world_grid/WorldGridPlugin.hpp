/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_WORLD_GRID_PLUGIN_HPP
#define HYPERION_WORLD_GRID_PLUGIN_HPP

#include <core/Defines.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/object/HypObject.hpp>

#include <scene/Node.hpp>

#include <GameCounter.hpp>

namespace hyperion {

class WorldGridPatch;
struct WorldGridPatchInfo;

HYP_CLASS(Abstract)
class HYP_API WorldGridPlugin : public EnableRefCountedPtrFromThis<WorldGridPlugin>
{
    HYP_OBJECT_BODY(WorldGridPlugin);

public:
    WorldGridPlugin();
    WorldGridPlugin(const WorldGridPlugin& other) = delete;
    WorldGridPlugin& operator=(const WorldGridPlugin& other) = delete;
    WorldGridPlugin(WorldGridPlugin&& other) = delete;
    WorldGridPlugin& operator=(WorldGridPlugin&& other) = delete;
    virtual ~WorldGridPlugin() = default;

    virtual void Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual void Update(GameCounter::TickUnit delta) = 0;

    virtual UniquePtr<WorldGridPatch> CreatePatch(const WorldGridPatchInfo& patch_info) = 0;
};

} // namespace hyperion

#endif
