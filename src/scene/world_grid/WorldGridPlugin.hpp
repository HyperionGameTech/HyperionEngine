/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_WORLD_GRID_PLUGIN_HPP
#define HYPERION_WORLD_GRID_PLUGIN_HPP

#include <core/Defines.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/object/HypObject.hpp>

#include <streaming/StreamingCell.hpp>

namespace hyperion {

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

    HYP_METHOD(Scriptable)
    void Initialize(WorldGrid* world_grid);
    
    HYP_METHOD(Scriptable)
    void Shutdown(WorldGrid* world_grid);

    HYP_METHOD(Scriptable)
    void Update(float delta);

    HYP_METHOD(Scriptable)
    Handle<StreamingCell> CreatePatch(WorldGrid* world_grid, const StreamingCellInfo& cell_info);

protected:
    HYP_METHOD()
    virtual void Initialize_Impl(WorldGrid* world_grid)
    {
        HYP_PURE_VIRTUAL();
    }

    HYP_METHOD()
    virtual void Shutdown_Impl(WorldGrid* world_grid)
    {
        HYP_PURE_VIRTUAL();
    }

    HYP_METHOD()
    virtual void Update_Impl(float delta)
    {
        HYP_PURE_VIRTUAL();
    }

    HYP_METHOD()
    virtual Handle<StreamingCell> CreatePatch_Impl(WorldGrid* world_grid, const StreamingCellInfo& cell_info)
    {
        HYP_PURE_VIRTUAL();
    }
};

} // namespace hyperion

#endif
