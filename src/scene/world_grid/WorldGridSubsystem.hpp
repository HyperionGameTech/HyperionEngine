/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_WORLD_GRID_SUBSYSTEM_HPP
#define HYPERION_WORLD_GRID_SUBSYSTEM_HPP

#include <scene/Subsystem.hpp>

#include <core/containers/Array.hpp>
#include <core/memory/UniquePtr.hpp>

namespace hyperion {

class WorldGrid;

HYP_CLASS()

class HYP_API WorldGridSubsystem : public Subsystem
{
    HYP_OBJECT_BODY(WorldGridSubsystem);

public:
    WorldGridSubsystem();
    virtual ~WorldGridSubsystem() override;

    WorldGrid* GetWorldGrid(ID<Scene> scene_id);

    virtual void Initialize() override;
    virtual void Shutdown() override;
    virtual void Update(GameCounter::TickUnit delta) override;

    virtual void OnSceneAttached(const Handle<Scene>& scene) override;
    virtual void OnSceneDetached(const Handle<Scene>& scene) override;

private:
    void UpdateScene(const Handle<Scene>& scene);

    Array<UniquePtr<WorldGrid>> m_world_grids;
};

} // namespace hyperion

#endif
