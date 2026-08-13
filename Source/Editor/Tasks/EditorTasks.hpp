#pragma once

#include <Editor/EditorTask.hpp>

namespace Hyperion {

class LightmapVolume;
class EnvProbe;
class World;
class Scene;

HYP_CLASS(NoScriptBindings)
class GenerateLightmapsEditorTask : public TickableEditorTask
{
    HYP_OBJECT_BODY(GenerateLightmapsEditorTask);

public:
    GenerateLightmapsEditorTask()
        : TickableEditorTask()
    {
    }

    explicit GenerateLightmapsEditorTask(const Handle<LightmapVolume>& volume);
    explicit GenerateLightmapsEditorTask(const Handle<EnvProbe>& probe);

    explicit GenerateLightmapsEditorTask(const Array<Handle<ObjectBase>>& sources);

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<World>& GetWorld() const
    {
        return m_world;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE void SetWorld(const Handle<World>& world)
    {
        m_world = world;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<Scene>& GetScene() const
    {
        return m_scene;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE void SetScene(const Handle<Scene>& scene)
    {
        m_scene = scene;
    }

    HYP_METHOD()
    virtual void Start() override;

    HYP_METHOD()
    virtual bool IsCompleted() const override;

    HYP_METHOD()
    virtual void Tick() override;

protected:
    HYP_METHOD()
    virtual void Cancel_Impl() override;

private:
    Array<Handle<ObjectBase>, EditorAllocator> m_sources;
    Handle<World> m_world;
    Handle<Scene> m_scene;

    Array<Task<void>, EditorAllocator> m_tasks;
};

HYP_CLASS(NoScriptBindings)
class GenerateBentNormalsEditorTask : public TickableEditorTask
{
    HYP_OBJECT_BODY(GenerateBentNormalsEditorTask);

public:
    GenerateBentNormalsEditorTask()
        : TickableEditorTask()
    {
    }

    explicit GenerateBentNormalsEditorTask(const Array<Handle<LightmapVolume>>& volumes);

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<World>& GetWorld() const
    {
        return m_world;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE void SetWorld(const Handle<World>& world)
    {
        m_world = world;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<Scene>& GetScene() const
    {
        return m_scene;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE void SetScene(const Handle<Scene>& scene)
    {
        m_scene = scene;
    }

    HYP_METHOD()
    virtual void Start() override;

    HYP_METHOD()
    virtual bool IsCompleted() const override;

    HYP_METHOD()
    virtual void Tick() override;

protected:
    HYP_METHOD()
    virtual void Cancel_Impl() override;

private:
    Array<Handle<LightmapVolume>, EditorAllocator> m_volumes;
    Handle<World> m_world;
    Handle<Scene> m_scene;

    Array<Task<void>, EditorAllocator> m_tasks;
};

} // namespace Hyperion