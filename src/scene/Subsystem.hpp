/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_SUBSYSTEM_HPP
#define HYPERION_SUBSYSTEM_HPP

#include <core/Handle.hpp>
#include <core/Defines.hpp>

#include <core/object/HypObject.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/utilities/StringView.hpp>

#include <HashCode.hpp>
#include <GameCounter.hpp>

namespace hyperion {

class Scene;
class World;

HYP_CLASS(Abstract)
class HYP_API Subsystem : public EnableRefCountedPtrFromThis<Subsystem>
{
    HYP_OBJECT_BODY(Subsystem);

public:
    friend class World;

    Subsystem();
    Subsystem(const Subsystem &other)               = delete;
    Subsystem &operator=(const Subsystem &other)    = delete;
    Subsystem(Subsystem &&other)                    = delete;
    Subsystem &operator=(Subsystem &&other)         = delete;
    virtual ~Subsystem();

    virtual bool RequiresUpdateOnGameThread() const
        { return true; }

    virtual void Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual void Update(GameCounter::TickUnit delta) = 0;

    virtual void OnSceneAttached(const Handle<Scene> &scene) { };
    virtual void OnSceneDetached(const Handle<Scene> &scene) { };

protected:
    HYP_FORCE_INLINE World *GetWorld() const
        { return m_world; }

private:
    HYP_FORCE_INLINE void SetWorld(World *world)
        { m_world = world; }

    World   *m_world;
};

} // namespace hyperion

#endif
