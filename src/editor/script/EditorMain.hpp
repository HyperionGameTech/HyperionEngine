/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Types.hpp>

#include <core/reflection/ObjectBase.hpp>
#include <core/reflection/Handle.hpp>

#include <ui/UIObject.hpp>

namespace hyperion {

class World;
class Scene;
class EditorProject;

HYP_CLASS(NoScriptBindings)
class HYP_API EditorMain : public ObjectBase
{
    HYP_OBJECT_BODY(EditorMain);

public:
    EditorMain();
    virtual ~EditorMain() = default;

    HYP_METHOD()
    void BeforeAdded(World* world, Scene* scene);

    HYP_METHOD()
    void OnAdded(Entity* entity);

    HYP_METHOD()
    UIEventHandlerResult SaveClicked(const MouseEvent& event);

    HYP_METHOD()
    UIEventHandlerResult UndoClicked(const MouseEvent& event);

    HYP_METHOD()
    UIEventHandlerResult RedoClicked(const MouseEvent& event);

    HYP_METHOD()
    UIEventHandlerResult UpdateUndoMenuItem();

    HYP_METHOD()
    UIEventHandlerResult UpdateRedoMenuItem();

    HYP_METHOD()
    UIEventHandlerResult SimulateClicked(const MouseEvent& event);

    HYP_METHOD()
    UIEventHandlerResult RebuildLightmaps(const MouseEvent& event);

    HYP_METHOD()
    UIEventHandlerResult AddPointLight(const MouseEvent& event);

    HYP_METHOD()
    UIEventHandlerResult AddSpotLight(const MouseEvent& event);

    HYP_METHOD()
    UIEventHandlerResult AddAreaRectLight(const MouseEvent& event);

    HYP_METHOD()
    UIEventHandlerResult AddDirectionalLight(const MouseEvent& event);

    HYP_METHOD()
    UIEventHandlerResult AddReflectionProbe(const MouseEvent& event);

    HYP_METHOD()
    UIEventHandlerResult AddLightmapVolume(const MouseEvent& event);

    HYP_METHOD()
    UIEventHandlerResult AddParticleVolume(const MouseEvent& event);

    HYP_METHOD()
    UIEventHandlerResult AddNode(const MouseEvent& event);

    HYP_METHOD()
    UIEventHandlerResult AddEntity(const MouseEvent& event);

private:
    void HandleProjectOpened(const Handle<EditorProject>& project);
    void HandleProjectClosing(const Handle<EditorProject>& project);

    World* m_world;
    Scene* m_scene;

    DelegateHandler m_onProjectOpenedDelegate;
    DelegateHandler m_onProjectClosingDelegate;
    DelegateHandler m_onActionStackStateChangeDelegate;
};

} // namespace hyperion
