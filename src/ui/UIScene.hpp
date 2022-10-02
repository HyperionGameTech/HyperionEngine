#ifndef HYPERION_V2_UI_SCENE_H
#define HYPERION_V2_UI_SCENE_H

#include <core/Base.hpp>
#include <core/Containers.hpp>

#include <rendering/backend/RendererStructs.hpp>
#include <rendering/FullScreenPass.hpp>

#include <math/Transform.hpp>

#include <scene/Scene.hpp>

#include <core/Containers.hpp>
#include <GameCounter.hpp>

#include <vector>

namespace hyperion::v2 {

using renderer::Extent2D;

// class UIPass : public FullScreenPass
// {
// public:
//     UIPass();
//     UIPass(const UIPass &other) = delete;
//     UIPass &operator=(const UIPass &other) = delete;
//     virtual ~UIPass();

//     void CreateShader(Engine *engine);
//     virtual void CreateRenderPass(Engine *engine);
//     virtual void CreateDescriptors(Engine *engine);
//     virtual void Create(Engine *engine) override;
//     virtual void Destroy(Engine *engine) override;
//     virtual void Record(Engine *engine, UInt frame_index) override;
// };

class UIObject : public EngineComponentBase<STUB_CLASS(UIObject)>
{
public:
    UIObject();
    ~UIObject();

    Handle<Entity> &GetEntity()
        { return m_entity; }

    const Handle<Entity> &GetEntity() const
        { return m_entity; }

    const Transform &GetTransform() const
        { return m_transform; }

    void SetTransform(const Transform &transform);

    void Init(Engine *engine);

protected:
    Handle<Entity> m_entity;
    Transform m_transform;
};

class UIScene : public EngineComponentBase<STUB_CLASS(UIScene)>
{
public:
    UIScene();
    UIScene(const UIScene &other) = delete;
    UIScene &operator=(const UIScene &other) = delete;
    ~UIScene();

    Handle<Scene> &GetScene()
        { return m_scene; }

    const Handle<Scene> &GetScene() const
        { return m_scene; }

    void Add(Handle<UIObject> &&object);

    // template <class T, class ...Args>
    // typename std::enable_if_t<std::is_base_if<UIObject, T>, Handle<T>>
    // Add(Args &&... args)
    // {

    // }

    void Init(Engine *engine);
    void Update(Engine *engine, GameCounter::TickUnit delta);

    // bool Add(Handle<UIObject> &&ui_object);
    // bool Add(const Handle<UIObject> &ui_object);
    // bool Remove(const UIObject::ID &id);

private:
    DynArray<Handle<UIObject>> m_ui_objects;
    Handle<Scene> m_scene;
    // FlatMap<UIObject::ID, Handle<UIObject>> m_ui_objects;
};

} // namespace hyperion::v2

#endif