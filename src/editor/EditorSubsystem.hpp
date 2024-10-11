/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_EDITOR_SUBSYSTEM_HPP
#define HYPERION_EDITOR_SUBSYSTEM_HPP

#include <scene/Subsystem.hpp>

#include <core/containers/Array.hpp>

#include <core/memory/UniquePtr.hpp>

namespace hyperion {

HYP_CLASS()
class HYP_API EditorSubsystem : public Subsystem
{
    HYP_OBJECT_BODY(EditorSubsystem);

public:
    EditorSubsystem();
    virtual ~EditorSubsystem() override;

    virtual void Initialize() override;
    virtual void Shutdown() override;
    virtual void Update(GameCounter::TickUnit delta) override;

    virtual void OnSceneAttached(const Handle<Scene> &scene) override;
    virtual void OnSceneDetached(const Handle<Scene> &scene) override;
};

} // namespace hyperion

#endif
