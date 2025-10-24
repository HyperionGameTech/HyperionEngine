/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/reflection/HypObject.hpp>
#include <core/Types.hpp>

#include <ui/UIObject.hpp>

namespace hyperion {

class World;
class Scene;

HYP_CLASS()
class HYP_API EditorMain : public HypObjectBase
{
    HYP_OBJECT_BODY(EditorMain);

public:
    EditorMain();
    virtual ~EditorMain() = default;

    HYP_METHOD()
    void BeforeInit(World* world, Scene* scene);

    HYP_METHOD()
    void Init() override;

    HYP_METHOD()
    UIEventHandlerResult OpenProjectClicked(const MouseEvent& event);

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
    UIEventHandlerResult GenerateLightmapsClicked(const MouseEvent& event);

    HYP_METHOD()
    UIEventHandlerResult AddPointLight(const MouseEvent& event);

    HYP_METHOD()
    UIEventHandlerResult AddAreaRectLight(const MouseEvent& event);

    HYP_METHOD()
    UIEventHandlerResult AddReflectionProbe(const MouseEvent& event);

    HYP_METHOD()
    UIEventHandlerResult AddNode(const MouseEvent& event);

    HYP_METHOD()
    UIEventHandlerResult AddEntity(const MouseEvent& event);

private:
    World* m_world;
    Scene* m_scene;
};

} // namespace hyperion
