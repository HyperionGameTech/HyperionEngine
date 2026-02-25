/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <scene/System.hpp>

namespace Hyperion {

/*! \brief A base type for Systems in C# to derive to implement system behavior. */
HYP_CLASS(Abstract)
class ScriptableSystem final : public SystemBase
{
    HYP_OBJECT_BODY(ScriptableSystem);

public:
    ScriptableSystem()
        : SystemBase()
    {
    }

    virtual ~ScriptableSystem() override = default;

    HYP_METHOD(Scriptable)
    bool AllowParallelExecution() const override;

    HYP_METHOD(Scriptable)
    bool RequiresSimThread() const override;

    HYP_METHOD(Scriptable)
    bool AllowUpdate() const override;

    HYP_METHOD(Scriptable)
    void OnEntityAdded(Entity* entity) override;

    HYP_METHOD(Scriptable)
    void OnEntityRemoved(Entity* entity) override;

    HYP_METHOD(Scriptable)
    void Init() override;

    HYP_METHOD(Scriptable)
    void Shutdown() override;

    void Process(float delta, Span<Handle<Scene>> scenes) override final
    {
        for (const Handle<Scene>& scene : scenes)
        {
            ProcessScene(scene, delta);
        }
    }

    HYP_METHOD(Scriptable)
    void ProcessScene(const Handle<Scene>& scene, float delta);

private:
    virtual SystemComponentDescriptors GetComponentDescriptors() const override
    {
        Array<ComponentInfo> componentInfos = GetComponentInfos();

        return SystemComponentDescriptors(componentInfos.ToSpan());
    }

    HYP_METHOD(Scriptable)
    Array<ComponentInfo> GetComponentInfos() const;

    Array<ComponentInfo> GetComponentInfos_Impl() const
    {
        HYP_PURE_VIRTUAL();
    }

    HYP_METHOD()
    bool AllowParallelExecution_Impl() const
    {
        return SystemBase::AllowParallelExecution();
    }

    HYP_METHOD()
    bool RequiresSimThread_Impl() const
    {
        return SystemBase::RequiresSimThread();
    }

    HYP_METHOD()
    bool AllowUpdate_Impl() const
    {
        return SystemBase::AllowUpdate();
    }

    HYP_METHOD()
    void OnEntityAdded_Impl(Entity* entity)
    {
        SystemBase::OnEntityAdded(entity);
    }

    HYP_METHOD()
    void OnEntityRemoved_Impl(Entity* entity)
    {
        SystemBase::OnEntityRemoved(entity);
    }

    HYP_METHOD()
    void Init_Impl()
    {
        SystemBase::Init();
    }

    HYP_METHOD()
    void Shutdown_Impl()
    {
        SystemBase::Shutdown();
    }

    void ProcessScene_Impl(const Handle<Scene>& scene, float delta)
    {
        HYP_PURE_VIRTUAL();
    }
};

} // namespace Hyperion
