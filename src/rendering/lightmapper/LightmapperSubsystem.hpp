/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_LIGHTMAPPER_SUBSYSTEM_HPP
#define HYPERION_LIGHTMAPPER_SUBSYSTEM_HPP

#include <core/containers/LinkedList.hpp>

#include <core/threading/Task.hpp>

#include <scene/Subsystem.hpp>

namespace hyperion {

class Lightmapper;

HYP_CLASS()
class HYP_API LightmapperSubsystem : public SubsystemBase
{
    HYP_OBJECT_BODY(LightmapperSubsystem);

public:
    LightmapperSubsystem();
    virtual ~LightmapperSubsystem() override = default;

    virtual void Initialize() override;
    virtual void Shutdown() override;
    virtual void Update(GameCounter::TickUnit delta) override;

    Task<void> *GenerateLightmaps(const Handle<Scene> &scene);

private:
    HashMap<ID<Scene>, UniquePtr<Lightmapper>>  m_lightmappers;
    LinkedList<Task<void>>                      m_tasks;
};

} // namespace hyperion

#endif