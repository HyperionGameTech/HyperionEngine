#ifndef HYPERION_V2_UI_SCENE_H
#define HYPERION_V2_UI_SCENE_H

#include <core/Base.hpp>
#include <core/Containers.hpp>

#include <scene/Node.hpp>

#include <rendering/backend/RendererStructs.hpp>
#include <rendering/FullScreenPass.hpp>

#include <math/Transform.hpp>

#include <scene/Scene.hpp>

#include <core/Containers.hpp>
#include <GameCounter.hpp>

#include <Types.hpp>

#include <vector>

namespace hyperion {

class SystemEvent;
class InputManager;

} // namespace hyperion

namespace hyperion::v2 {

using renderer::Extent2D;

// class UIObject : public EngineComponentBase<STUB_CLASS(UIObject)>
// {
// public:
//     UIObject();
//     virtual ~UIObject();

//     virtual void Init(Engine *engine);
// };

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

    // void Add(Handle<UIObject> &&object);

    bool OnInputEvent(
        InputManager *input_manager,
        const SystemEvent &event
    );

    /*! \brief Ray test the UI scene using screen space mouse coordinates */
    bool TestRay(const Vector2 &position, RayHit &out_first_hit);

    void Init(Engine *engine);
    void Update(Engine *engine, GameCounter::TickUnit delta);

private:
    // DynArray<Handle<UIObject>> m_ui_objects;
    Handle<Scene> m_scene;
};

} // namespace hyperion::v2

#endif