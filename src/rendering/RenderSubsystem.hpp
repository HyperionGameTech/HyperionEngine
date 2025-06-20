/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDER_SUBSYSTEM_HPP
#define HYPERION_RENDER_SUBSYSTEM_HPP

#include <core/Base.hpp>

#include <core/threading/AtomicVar.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/object/HypObject.hpp>

#include <core/math/MathUtil.hpp>

#include <scene/Entity.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <GameCounter.hpp>
#include <Constants.hpp>
#include <Types.hpp>

#include <type_traits>

namespace hyperion {

class RenderEnvironment;
struct RenderSetup;

HYP_CLASS(NoScriptBindings)
class HYP_API RenderSubsystem : public HypObject<RenderSubsystem>
{
    HYP_OBJECT_BODY(RenderSubsystem);

public:
    friend class RenderEnvironment;

    RenderSubsystem(Name name)
        : m_name(name),
          m_parent(nullptr)
    {
    }

    RenderSubsystem(const RenderSubsystem& other) = delete;
    RenderSubsystem& operator=(const RenderSubsystem& other) = delete;
    virtual ~RenderSubsystem() = default;

    HYP_FORCE_INLINE Name GetName() const
    {
        return m_name;
    }

    virtual void OnUpdate(float delta)
    {
    }

    virtual void OnRemoved()
    {
    }

    /*! \brief Thread-safe way to remove the RenderSubsystem from the RenderEnvironment, if applicable.
     *  A render command will be issued to remove the RenderSubsystem from the RenderEnvironment. */
    void RemoveFromEnvironment();

protected:
    virtual void Init() override
    {
    }

    RenderEnvironment* GetParent() const;

    Name m_name;

private:
    void SetParent(RenderEnvironment* parent);

    RenderEnvironment* m_parent;
};

} // namespace hyperion

#endif