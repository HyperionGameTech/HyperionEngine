#pragma once

#include <Editor/EditorTask.hpp>

namespace Hyperion {

class LightmapVolume;
class EnvProbe;
class World;
class Scene;

namespace Baking {
struct BakeLayer;
} // namespace Baking

HYP_CLASS()
class GenerateLightmapsEditorTask : public TickableEditorTask
{
    HYP_OBJECT_BODY(GenerateLightmapsEditorTask);

public:
    GenerateLightmapsEditorTask()
        : TickableEditorTask(),
          m_bakeLayer(nullptr)
    {
    }

    GenerateLightmapsEditorTask(Baking::BakeLayer& bakeLayer, const Handle<LightmapVolume>& volume);
    GenerateLightmapsEditorTask(Baking::BakeLayer& bakeLayer, const Handle<EnvProbe>& probe);
    GenerateLightmapsEditorTask(Baking::BakeLayer& bakeLayer, const Array<Handle<ObjectBase>>& sources);

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
    Baking::BakeLayer* m_bakeLayer;

    Array<Handle<ObjectBase>, EditorAllocator> m_sources;
    Handle<World> m_world;
    Handle<Scene> m_scene;

    Array<Task<void>, EditorAllocator> m_tasks;
};

HYP_CLASS()
class GenerateBentNormalsEditorTask : public TickableEditorTask
{
    HYP_OBJECT_BODY(GenerateBentNormalsEditorTask);

public:
    GenerateBentNormalsEditorTask()
        : TickableEditorTask(),
          m_bakeLayer(nullptr)
    {
    }

    GenerateBentNormalsEditorTask(
        Baking::BakeLayer& bakeLayer,
        const Array<Handle<LightmapVolume>>& volumes);

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
    Baking::BakeLayer* m_bakeLayer;

    Array<Handle<LightmapVolume>, EditorAllocator> m_volumes;
    Handle<World> m_world;
    Handle<Scene> m_scene;

    Array<Task<void>, EditorAllocator> m_tasks;
};

} // namespace Hyperion