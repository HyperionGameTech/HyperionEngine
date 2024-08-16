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

class HYP_API SubsystemBase : public EnableRefCountedPtrFromThis<SubsystemBase>
{
    HYP_OBJECT_BODY(SubsystemBase, Abstract);

public:
    SubsystemBase();
    SubsystemBase(const SubsystemBase &other)               = delete;
    SubsystemBase &operator=(const SubsystemBase &other)    = delete;
    SubsystemBase(SubsystemBase &&other)                    = delete;
    SubsystemBase &operator=(SubsystemBase &&other)         = delete;
    virtual ~SubsystemBase();

    virtual bool RequiresUpdateOnGameThread() const
        { return true; }

    virtual ANSIStringView GetName() const = 0;

    virtual void Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual void Update(GameCounter::TickUnit delta) = 0;

    virtual void OnSceneAttached(const Handle<Scene> &scene) = 0;
    virtual void OnSceneDetached(const Handle<Scene> &scene) = 0;

    virtual HashCode GetHashCode() const = 0;
};

template <class Derived>
class Subsystem : public SubsystemBase
{
public:
    Subsystem()                     = default;
    virtual ~Subsystem() override   = default;

    virtual ANSIStringView GetName() const override final
    {
        return { TypeNameHelper<Derived, true>::value };
    }

    virtual void Initialize() override = 0;
    virtual void Shutdown() override = 0;
    virtual void Update(GameCounter::TickUnit delta) override = 0;

    virtual void OnSceneAttached(const Handle<Scene> &scene) override { }
    virtual void OnSceneDetached(const Handle<Scene> &scene) override { }

    virtual HashCode GetHashCode() const override
    {
        return HashCode::GetHashCode(GetName());
    }
};

} // namespace hyperion

#endif
