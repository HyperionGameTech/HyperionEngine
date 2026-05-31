/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/HashCode.hpp>

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

namespace Hyperion {

class Scene;
class World;

HYP_ENUM()
enum class SubsystemUpdatePhase : uint8
{
    BeforeVis,
    AfterVis
};

HYP_CLASS(Abstract)
class ENGINE_API Subsystem : public ObjectBase
{
    HYP_OBJECT_BODY(Subsystem);

public:
    friend class World;

    Subsystem();
    Subsystem(const Subsystem& other) = delete;
    Subsystem& operator=(const Subsystem& other) = delete;
    Subsystem(Subsystem&& other) = delete;
    Subsystem& operator=(Subsystem&& other) = delete;
    virtual ~Subsystem();

    virtual bool RequiresUpdateOnSimThread() const
    {
        return true;
    }

    HYP_FORCE_INLINE World* GetWorld() const
    {
        return m_world;
    }

    HYP_FORCE_INLINE SubsystemUpdatePhase GetUpdatePhase() const
    {
        return m_updatePhase;
    }

    virtual void OnAddedToWorld() = 0;
    virtual void OnRemovedFromWorld() = 0;
    virtual void PreUpdate(float delta)
    {
    }
    virtual void Update(float delta) = 0;
    virtual void OnSceneAttached(const Handle<Scene>& scene) { };
    virtual void OnSceneDetached(Scene* scene) { };

protected:
    virtual void Init() override
    {
        m_updatePhase = GetUpdatePhase_Internal();

        SetReady(true);
    }

    virtual SubsystemUpdatePhase GetUpdatePhase_Internal() const
    {
        return SubsystemUpdatePhase::BeforeVis;
    }

private:
    HYP_FORCE_INLINE void SetWorld(World* world)
    {
        m_world = world;
    }

    World* m_world;
    SubsystemUpdatePhase m_updatePhase;
};

} // namespace Hyperion
