/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <EditorPch.hpp>

#include <editor/script/EditorMain.hpp>

#include <editor/EditorSubsystem.hpp>
#include <editor/EditorProject.hpp>
#include <editor/EditorActionStack.hpp>
#include <editor/EditorAction.hpp>
#include <editor/EditorState.hpp>

#include <ui/UIMenuBar.hpp>
#include <ui/UISubsystem.hpp>
#include <ui/UIStage.hpp>

#include <scene/World.hpp>
#include <scene/Scene.hpp>
#include <scene/EntityManager.hpp>
#include <scene/Light.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/LightmapVolume.hpp>

#include <engine/GameState.hpp>

#include <scene/components/BoundingBoxComponent.hpp>

#include <rendering/shadows/ShadowMap.hpp>

#include <scene/ParticleVolume.hpp>

#include <system/MessageBox.hpp>

#include <asset/Assets.hpp>

// test
#include <editor/nativeui/TestNativeUI.hpp>

#include <engine/EngineDriver.hpp>

#include <EditorMain.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

EditorMain::EditorMain()
    : m_world(nullptr),
      m_scene(nullptr)
{
}

void EditorMain::BeforeAdded(World* world, Scene* scene)
{
    m_world = world;
    m_scene = scene;

    Assert(m_world != nullptr && m_scene != nullptr);
}

void EditorMain::OnAdded(Entity* entity)
{
    HYP_SCOPE;

    HYP_LOG(Editor, Verbose, "EditorMain OnAdded()");

    EditorSubsystem* editorSubsystem = m_world->GetSubsystem<EditorSubsystem>();
    if (!editorSubsystem)
    {
        return;
    }

    // Bind to project opened/closing events
    m_onProjectOpenedDelegate = editorSubsystem->OnProjectOpened.Bind([this](const Handle<EditorProject>& project)
        {
            HandleProjectOpened(project);
        });

    m_onProjectClosingDelegate = editorSubsystem->OnProjectClosing.Bind([this](const Handle<EditorProject>& project)
        {
            HandleProjectClosing(project);
        });

    // If a project is already open, handle it
    const Handle<EditorProject>& currentProject = editorSubsystem->GetCurrentProject();
    if (currentProject.IsValid())
    {
        HandleProjectOpened(currentProject);
    }
}

void EditorMain::HandleProjectOpened(const Handle<EditorProject>& project)
{
    HYP_SCOPE;

    HYP_LOG(Editor, Verbose, "HandleProjectOpened invoked with project: {}", project->GetName());

    // Remove existing action stack state change delegate if any
    m_onActionStackStateChangeDelegate.Reset();

    // Bind to action stack state changes to update undo/redo menu items
    m_onActionStackStateChangeDelegate = project->GetActionStack()->OnStateChange.Bind([this](EnumFlags<EditorActionStackState> state)
        {
            UpdateUndoMenuItem();
            UpdateRedoMenuItem();
        });
}

void EditorMain::HandleProjectClosing(const Handle<EditorProject>& project)
{
    HYP_SCOPE;

    HYP_LOG(Editor, Verbose, "HandleProjectClosing invoked with project: {}", project->GetName());

    // Remove action stack state change delegate
    m_onActionStackStateChangeDelegate.Reset();
}

UIEventHandlerResult EditorMain::SaveClicked(const MouseEvent& event)
{
    HYP_SCOPE;

    EditorSubsystem* editorSubsystem = m_world->GetSubsystem<EditorSubsystem>();
    if (editorSubsystem == nullptr)
    {
        HYP_LOG(Editor, Error, "EditorSubsystem not found");

        return UIEventHandlerResult::ERR;
    }

    const Handle<EditorProject>& currentProject = editorSubsystem->GetCurrentProject();
    if (!currentProject.IsValid())
    {
        HYP_LOG(Editor, Warning, "No project is currently opened");

        return UIEventHandlerResult::ERR;
    }

    currentProject->Save();

    return UIEventHandlerResult::OK;
}

UIEventHandlerResult EditorMain::UndoClicked(const MouseEvent& event)
{
    HYP_SCOPE;

    EditorSubsystem* editorSubsystem = m_world->GetSubsystem<EditorSubsystem>();
    if (editorSubsystem == nullptr)
    {
        HYP_LOG(Editor, Error, "EditorSubsystem not found");

        return UIEventHandlerResult::ERR;
    }

    const Handle<EditorProject>& currentProject = editorSubsystem->GetCurrentProject();
    if (!currentProject.IsValid())
    {
        HYP_LOG(Editor, Warning, "No project is currently opened");

        return UIEventHandlerResult::ERR;
    }

    if (!currentProject->GetActionStack()->CanUndo())
    {
        HYP_LOG(Editor, Warning, "No actions to undo");

        return UIEventHandlerResult::ERR;
    }

    currentProject->GetActionStack()->Undo();

    return UIEventHandlerResult::OK;
}

UIEventHandlerResult EditorMain::RedoClicked(const MouseEvent& event)
{
    HYP_SCOPE;

    EditorSubsystem* editorSubsystem = m_world->GetSubsystem<EditorSubsystem>();
    if (editorSubsystem == nullptr)
    {
        HYP_LOG(Editor, Error, "EditorSubsystem not found");

        return UIEventHandlerResult::ERR;
    }

    const Handle<EditorProject>& currentProject = editorSubsystem->GetCurrentProject();
    if (!currentProject.IsValid())
    {
        HYP_LOG(Editor, Warning, "No project is currently opened");

        return UIEventHandlerResult::ERR;
    }

    if (!currentProject->GetActionStack()->CanRedo())
    {
        HYP_LOG(Editor, Warning, "No actions to redo");

        return UIEventHandlerResult::ERR;
    }

    currentProject->GetActionStack()->Redo();

    return UIEventHandlerResult::OK;
}

UIEventHandlerResult EditorMain::UpdateUndoMenuItem()
{
    HYP_SCOPE;

    EditorSubsystem* editorSubsystem = m_world->GetSubsystem<EditorSubsystem>();
    if (editorSubsystem == nullptr)
    {
        HYP_LOG(Editor, Error, "EditorSubsystem not found");

        return UIEventHandlerResult::ERR;
    }

    const Handle<EditorProject>& currentProject = editorSubsystem->GetCurrentProject();
    if (!currentProject.IsValid())
    {
        HYP_LOG(Editor, Error, "No project loaded; cannot update undo menu item");

        return UIEventHandlerResult::ERR;
    }

    UISubsystem* uiSubsystem = m_world->GetSubsystem<UISubsystem>();
    if (uiSubsystem == nullptr)
    {
        HYP_LOG(Editor, Error, "UISubsystem not found");

        return UIEventHandlerResult::ERR;
    }

    Handle<UIObject> undoMenuItemObject = uiSubsystem->GetUIStage()->FindChildUIObject(CreateStringHashFromDynamicString("Undo_MenuItem"), true);
    if (!undoMenuItemObject.IsValid())
    {
        HYP_LOG(Editor, Error, "Undo menu item not found");

        return UIEventHandlerResult::ERR;
    }

    Handle<UIMenuItem> undoMenuItem = ObjCast<UIMenuItem>(undoMenuItemObject);
    if (!undoMenuItem.IsValid())
    {
        HYP_LOG(Editor, Error, "Undo menu item is not a UIMenuItem");

        return UIEventHandlerResult::ERR;
    }

    String undoText = "Undo";

    if (currentProject->GetActionStack()->CanUndo())
    {
        const Handle<EditorActionBase>& editorAction = currentProject->GetActionStack()->GetUndoAction();
        Assert(editorAction.IsValid());

        String actionUndoText = editorAction->GetName().ToString();

        if (actionUndoText.Any())
        {
            undoText = String("Undo ") + actionUndoText;
        }

        undoMenuItem->SetIsEnabled(true);
    }
    else
    {
        undoText = "Can't Undo";

        undoMenuItem->SetIsEnabled(false);
    }

    undoMenuItem->SetText(undoText);

    return UIEventHandlerResult::OK;
}

UIEventHandlerResult EditorMain::UpdateRedoMenuItem()
{
    HYP_SCOPE;

    EditorSubsystem* editorSubsystem = m_world->GetSubsystem<EditorSubsystem>();
    if (editorSubsystem == nullptr)
    {
        HYP_LOG(Editor, Error, "EditorSubsystem not found");

        return UIEventHandlerResult::ERR;
    }

    const Handle<EditorProject>& currentProject = editorSubsystem->GetCurrentProject();
    if (!currentProject.IsValid())
    {
        HYP_LOG(Editor, Error, "No project loaded; cannot update redo menu item");

        return UIEventHandlerResult::ERR;
    }

    UISubsystem* uiSubsystem = m_world->GetSubsystem<UISubsystem>();
    if (uiSubsystem == nullptr)
    {
        HYP_LOG(Editor, Error, "UISubsystem not found");

        return UIEventHandlerResult::ERR;
    }

    Handle<UIObject> redoMenuItemObject = uiSubsystem->GetUIStage()->FindChildUIObject(CreateStringHashFromDynamicString("Redo_MenuItem"), true);
    if (!redoMenuItemObject.IsValid())
    {
        HYP_LOG(Editor, Error, "Redo menu item not found");

        return UIEventHandlerResult::ERR;
    }

    Handle<UIMenuItem> redoMenuItem = ObjCast<UIMenuItem>(redoMenuItemObject);
    if (!redoMenuItem.IsValid())
    {
        HYP_LOG(Editor, Error, "Redo menu item is not a UIMenuItem");

        return UIEventHandlerResult::ERR;
    }

    String redoText = "Redo";

    if (currentProject->GetActionStack()->CanRedo())
    {
        const Handle<EditorActionBase>& editorAction = currentProject->GetActionStack()->GetRedoAction();
        Assert(editorAction.IsValid());

        String actionRedoText = editorAction->GetName().ToString();

        if (actionRedoText.Any())
        {
            redoText = String("Redo ") + actionRedoText;
        }

        redoMenuItem->SetIsEnabled(true);
    }
    else
    {
        redoText = "Can't Redo";

        redoMenuItem->SetIsEnabled(false);
    }

    redoMenuItem->SetText(redoText);

    return UIEventHandlerResult::OK;
}

UIEventHandlerResult EditorMain::RebuildLightmaps(const MouseEvent& event)
{
    HYP_SCOPE;

    EditorSubsystem* editorSubsystem = m_world->GetSubsystem<EditorSubsystem>();
    if (editorSubsystem == nullptr)
    {
        HYP_LOG(Editor, Error, "EditorSubsystem not found");

        return UIEventHandlerResult::ERR;
    }

    Handle<Scene> activeScene = editorSubsystem->GetActiveScene();
    if (!activeScene.IsValid())
    {
        HYP_LOG(Editor, Error, "No active scene; cannot add lightmap volume!");

        return UIEventHandlerResult::ERR;
    }

    /// \todo : Allow selection of scene(s) and volume(s) to generate for?

    Array<Handle<ObjectBase>> sources;

    // Generate EnvProbes and LightmapVolumes in the scene
    auto collectEntitiesOfType = [&]<class T>(TypeWrapper<T>)
    {
        for (auto [entity, _] : activeScene->GetEntityManager()->GetEntitySet<EntityType<T>>())
        {
            AssertDebug(entity->template IsA<T>());
            sources.PushBack(MakeStrongRef(entity));
        }
    };

    collectEntitiesOfType(TypeWrapper<EnvProbe> {});
    collectEntitiesOfType(TypeWrapper<LightmapVolume> {});

    if (sources.Empty())
    {
        HYP_LOG(Editor, Warning, "No EnvProbes or LightmapVolumes found in the active scene ({}); nothing to generate lightmaps for", activeScene->GetName());

        return UIEventHandlerResult::OK;
    }

    Handle<GenerateLightmapsEditorTask> generateLightmapsTask = MakeHandle<GenerateLightmapsEditorTask>(sources);
    InitObject(generateLightmapsTask);

    generateLightmapsTask->SetScene(activeScene);

    Handle<World> worldHandle = MakeStrongRef(m_world);

    generateLightmapsTask->SetWorld(worldHandle);

    g_editorState->AddTask(generateLightmapsTask);

    return UIEventHandlerResult::OK;
}

UIEventHandlerResult EditorMain::AddPointLight(const MouseEvent& event)
{
    HYP_SCOPE;

    HYP_LOG(Editor, Verbose, "Add Point Light clicked");

    EditorSubsystem* editorSubsystem = m_world->GetSubsystem<EditorSubsystem>();
    if (editorSubsystem == nullptr)
    {
        HYP_LOG(Editor, Error, "EditorSubsystem not found");

        return UIEventHandlerResult::ERR;
    }

    const Handle<EditorProject>& currentProject = editorSubsystem->GetCurrentProject();
    if (!currentProject.IsValid())
    {
        HYP_LOG(Editor, Error, "No project loaded; cannot add point light");

        return UIEventHandlerResult::ERR;
    }

    Handle<Scene> activeScene = editorSubsystem->GetActiveScene();
    if (!activeScene.IsValid())
    {
        HYP_LOG(Editor, Error, "No active scene found");

        return UIEventHandlerResult::ERR;
    }

    Handle<PointLight> light = activeScene->GetEntityManager()->AddEntity<PointLight>();
    light->SetColor(Color(1.0f, 1.0f, 1.0f, 1.0f));
    light->SetRadius(10.0f);
    light->SetIntensity(3.0f);
    // light->SetShadowMapFilter(SMF_VSM);

    light->SetName(activeScene->GetUniqueNodeName("PointLight"));

    // Calculate appropriate insertion point in front of camera
    const Vec3f insertionPoint = editorSubsystem->CalculateSceneInsertionPoint(5.0f, 0.5f);
    light->SetWorldTranslation(insertionPoint);

    WeakHandle<Node> previousFocusedNode = editorSubsystem->GetFocusedNode();

    Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
        NAME("AddPointLight"),
        Proc<EditorActionFunctions()>([light, previousFocusedNode, activeScene]() -> EditorActionFunctions
            {
                return EditorActionFunctions {
                    .execute = Proc<void(EditorSubsystem*, EditorProject*)>([light, activeScene](EditorSubsystem* editorSubsystem, EditorProject* project)
                        {
                            activeScene->GetRoot()->AddChild(light);
                            editorSubsystem->SetFocusedNode(light, true);
                        }),
                    .revert = Proc<void(EditorSubsystem*, EditorProject*)>([light, previousFocusedNode](EditorSubsystem* editorSubsystem, EditorProject* project)
                        {
                            light->Remove();

                            if (editorSubsystem->GetFocusedNode() == light)
                            {
                                editorSubsystem->SetFocusedNode(nullptr, true);

                                Handle<Node> focusedNode = previousFocusedNode.Lock();
                                if (focusedNode.IsValid())
                                {
                                    editorSubsystem->SetFocusedNode(focusedNode, true);
                                }
                            }
                        })
                };
            }));

    InitObject(action);

    currentProject->GetActionStack()->Push(action);

    return UIEventHandlerResult::OK;
}

UIEventHandlerResult EditorMain::AddSpotLight(const MouseEvent& event)
{
    HYP_SCOPE;

    HYP_LOG(Editor, Verbose, "Add Spot Light clicked");

    EditorSubsystem* editorSubsystem = m_world->GetSubsystem<EditorSubsystem>();
    if (editorSubsystem == nullptr)
    {
        HYP_LOG(Editor, Error, "EditorSubsystem not found");

        return UIEventHandlerResult::ERR;
    }

    const Handle<EditorProject>& currentProject = editorSubsystem->GetCurrentProject();
    if (!currentProject.IsValid())
    {
        HYP_LOG(Editor, Error, "No project loaded; cannot add spot light");

        return UIEventHandlerResult::ERR;
    }

    Handle<Scene> activeScene = editorSubsystem->GetActiveScene();
    if (!activeScene.IsValid())
    {
        HYP_LOG(Editor, Error, "No active scene found");

        return UIEventHandlerResult::ERR;
    }

    Handle<SpotLight> light = activeScene->GetEntityManager()->AddEntity<SpotLight>();
    light->SetColor(Color(1.0f, 1.0f, 1.0f, 1.0f));
    light->SetRadius(15.0f);
    light->SetIntensity(4.0f);

    constexpr Vec2f DefaultSpotAngles = Vec2f(MathUtil::DegToRad(15.0f), MathUtil::DegToRad(30.0f));
    light->SetSpotAngles(DefaultSpotAngles);

    light->SetName(activeScene->GetUniqueNodeName("SpotLight"));

    // Calculate appropriate insertion point in front of camera
    const Vec3f insertionPoint = editorSubsystem->CalculateSceneInsertionPoint(5.0f, 0.5f);
    light->SetWorldTranslation(insertionPoint);

    WeakHandle<Node> previousFocusedNode = editorSubsystem->GetFocusedNode();

    Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
        NAME("AddSpotLight"),
        Proc<EditorActionFunctions()>([light, previousFocusedNode, activeScene]() -> EditorActionFunctions
            {
                return EditorActionFunctions {
                    .execute = Proc<void(EditorSubsystem*, EditorProject*)>([light, activeScene](EditorSubsystem* editorSubsystem, EditorProject* project)
                        {
                            activeScene->GetRoot()->AddChild(light);
                            editorSubsystem->SetFocusedNode(light, true);
                        }),
                    .revert = Proc<void(EditorSubsystem*, EditorProject*)>([light, previousFocusedNode](EditorSubsystem* editorSubsystem, EditorProject* project)
                        {
                            light->Remove();

                            if (editorSubsystem->GetFocusedNode() == light)
                            {
                                editorSubsystem->SetFocusedNode(nullptr, true);

                                Handle<Node> focusedNode = previousFocusedNode.Lock();
                                if (focusedNode.IsValid())
                                {
                                    editorSubsystem->SetFocusedNode(focusedNode, true);
                                }
                            }
                        })
                };
            }));

    InitObject(action);

    currentProject->GetActionStack()->Push(action);

    return UIEventHandlerResult::OK;
}

UIEventHandlerResult EditorMain::AddAreaRectLight(const MouseEvent& event)
{
    HYP_SCOPE;

    EditorSubsystem* editorSubsystem = m_world->GetSubsystem<EditorSubsystem>();
    if (editorSubsystem == nullptr)
    {
        HYP_LOG(Editor, Error, "EditorSubsystem not found");

        return UIEventHandlerResult::ERR;
    }

    const Handle<EditorProject>& currentProject = editorSubsystem->GetCurrentProject();
    if (!currentProject.IsValid())
    {
        HYP_LOG(Editor, Error, "No project loaded; cannot add AreaRect light");

        return UIEventHandlerResult::ERR;
    }

    Handle<Scene> activeScene = editorSubsystem->GetActiveScene();
    if (!activeScene.IsValid())
    {
        HYP_LOG(Editor, Error, "No active scene found");

        return UIEventHandlerResult::ERR;
    }

    Handle<AreaRectLight> light = activeScene->GetEntityManager()->AddEntity<AreaRectLight>();
    light->SetPosition(Vec3f(0.0f, 5.0f, 0.0f));
    light->SetColor(Color(1.0f, 1.0f, 1.0f, 1.0f));
    light->SetRadius(30.0f);
    light->SetIntensity(5.0f);
    light->SetAreaSize(Vec2f(2.0f, 2.0f));

    light->SetName(activeScene->GetUniqueNodeName("AreaRectLight"));

    // Calculate appropriate insertion point in front of camera
    const Vec3f insertionPoint = editorSubsystem->CalculateSceneInsertionPoint(5.0f, 0.5f);
    light->SetWorldTranslation(insertionPoint);

    WeakHandle<Node> previousFocusedNode = editorSubsystem->GetFocusedNode();

    Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
        NAME("AddAreaRectLight"),
        Proc<EditorActionFunctions()>([light, previousFocusedNode, activeScene]() -> EditorActionFunctions
            {
                return EditorActionFunctions {
                    .execute = Proc<void(EditorSubsystem*, EditorProject*)>([light, activeScene](EditorSubsystem* editorSubsystem, EditorProject* project)
                        {
                            activeScene->GetRoot()->AddChild(light);
                            editorSubsystem->SetFocusedNode(light, true);
                        }),
                    .revert = Proc<void(EditorSubsystem*, EditorProject*)>([light, previousFocusedNode](EditorSubsystem* editorSubsystem, EditorProject* project)
                        {
                            light->Remove();

                            if (editorSubsystem->GetFocusedNode() == light)
                            {
                                editorSubsystem->SetFocusedNode(nullptr, true);

                                Handle<Node> focusedNode = previousFocusedNode.Lock();
                                if (focusedNode.IsValid())
                                {
                                    editorSubsystem->SetFocusedNode(focusedNode, true);
                                }
                            }
                        })
                };
            }));

    InitObject(action);

    currentProject->GetActionStack()->Push(action);

    return UIEventHandlerResult::OK;
}

UIEventHandlerResult EditorMain::AddDirectionalLight(const MouseEvent& event)
{
    HYP_SCOPE;

    HYP_LOG(Editor, Verbose, "Add Directional Light clicked");

    EditorSubsystem* editorSubsystem = m_world->GetSubsystem<EditorSubsystem>();
    if (editorSubsystem == nullptr)
    {
        HYP_LOG(Editor, Error, "EditorSubsystem not found");

        return UIEventHandlerResult::ERR;
    }

    const Handle<EditorProject>& currentProject = editorSubsystem->GetCurrentProject();
    if (!currentProject.IsValid())
    {
        HYP_LOG(Editor, Error, "No project loaded; cannot add directional light");

        return UIEventHandlerResult::ERR;
    }

    Handle<Scene> activeScene = editorSubsystem->GetActiveScene();
    if (!activeScene.IsValid())
    {
        HYP_LOG(Editor, Error, "No active scene found");

        return UIEventHandlerResult::ERR;
    }

    if (activeScene->GetEntityManager()->GetEntitySet<EntityType<DirectionalLight>>().Size() > 0)
    {
        bool shouldContinue = false;

        SystemMessageBox confirmMsgBox { MessageBoxType::WARNING };
        confirmMsgBox.Title("Add Directional Light");
        confirmMsgBox.Text(
            "A directional light already exists in the scene. Adding another may lead to unexpected lighting results and performance issues!\n"
            "Are you sure you want to add another directional light?");
        confirmMsgBox.Button(
            "Cancel",
            Proc<void()>([]()
                {
                    HYP_LOG(Editor, Verbose, "Add Directional Light cancelled by user");
                }));

        confirmMsgBox.Button(
            "Add Light",
            Proc<void()>([&shouldContinue]()
                {
                    HYP_LOG(Editor, Verbose, "User confirmed adding another Directional Light");
                    shouldContinue = true;
                }));

        confirmMsgBox.Show();

        if (!shouldContinue)
        {
            return UIEventHandlerResult::OK;
        }
    }

    Handle<DirectionalLight> light = activeScene->GetEntityManager()->AddEntity<DirectionalLight>();
    light->SetColor(Color(1.0f, 1.0f, 1.0f, 1.0f));
    light->SetIntensity(1.0f);

    light->SetName(activeScene->GetUniqueNodeName("DirectionalLight"));

    // Calculate appropriate insertion point in front of camera
    // Use a larger distance for directional lights since they affect the entire scene
    const Vec3f insertionPoint = editorSubsystem->CalculateSceneInsertionPoint(10.0f, 0.5f);
    light->SetWorldTranslation(insertionPoint);

    WeakHandle<Node> previousFocusedNode = editorSubsystem->GetFocusedNode();

    Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
        NAME("AddDirectionalLight"),
        Proc<EditorActionFunctions()>([light, previousFocusedNode, activeScene]() -> EditorActionFunctions
            {
                return EditorActionFunctions {
                    .execute = Proc<void(EditorSubsystem*, EditorProject*)>([light, activeScene](EditorSubsystem* editorSubsystem, EditorProject* project)
                        {
                            activeScene->GetRoot()->AddChild(light);
                            editorSubsystem->SetFocusedNode(light, true);
                        }),
                    .revert = Proc<void(EditorSubsystem*, EditorProject*)>([light, previousFocusedNode](EditorSubsystem* editorSubsystem, EditorProject* project)
                        {
                            light->Remove();

                            if (editorSubsystem->GetFocusedNode() == light)
                            {
                                editorSubsystem->SetFocusedNode(nullptr, true);

                                Handle<Node> focusedNode = previousFocusedNode.Lock();
                                if (focusedNode.IsValid())
                                {
                                    editorSubsystem->SetFocusedNode(focusedNode, true);
                                }
                            }
                        })
                };
            }));

    InitObject(action);

    currentProject->GetActionStack()->Push(action);

    return UIEventHandlerResult::OK;
}

UIEventHandlerResult EditorMain::AddReflectionProbe(const MouseEvent& event)
{
    HYP_SCOPE;

    Handle<TestNativeUI> testNativeUi = MakeHandle<TestNativeUI>();
    testNativeUi->OnAccepted.Bind([this, weakThis = MakeWeakRef(this)](AddReflectionProbeResult result)
                                {
                                    auto impl = [this, weakThis, result]()
                                    {
                                        Handle<EditorMain> strongThis = weakThis.Lock();
                                        if (!strongThis)
                                        {
                                            HYP_LOG(Editor, Error, "EditorMain reference expired!");

                                            return;
                                        }

                                        HYP_LOG(Editor, Verbose, "Add Reflection Probe clicked");

                                        EditorSubsystem* editorSubsystem = m_world->GetSubsystem<EditorSubsystem>();
                                        if (editorSubsystem == nullptr)
                                        {
                                            HYP_LOG(Editor, Error, "EditorSubsystem not found");

                                            return;
                                        }

                                        const Handle<EditorProject>& currentProject = editorSubsystem->GetCurrentProject();
                                        if (!currentProject.IsValid())
                                        {
                                            HYP_LOG(Editor, Error, "No project loaded; cannot add reflection probe");

                                            return;
                                        }

                                        Handle<Scene> activeScene = editorSubsystem->GetActiveScene();
                                        if (!activeScene.IsValid())
                                        {
                                            HYP_LOG(Editor, Error, "No active scene found");

                                            return;
                                        }

                                        BoundingBox aabb = BoundingBox(result.worldTranslation - result.probeVolumeDimensions * 0.5f, result.worldTranslation + result.probeVolumeDimensions * 0.5f);

                                        Handle<ReflectionProbe> reflectionProbe = activeScene->GetEntityManager()->AddEntity<ReflectionProbe>(aabb, Vec2u(result.textureDimension));
                                        reflectionProbe->SetIsBaked(result.bakeLighting);
                                        reflectionProbe->SetLocalBounds(aabb);

                                        reflectionProbe->SetName(activeScene->GetUniqueNodeName("ReflectionProbe"));

                                        // Calculate appropriate insertion point in front of camera
                                        const Vec3f insertionPoint = editorSubsystem->CalculateSceneInsertionPoint(5.0f, 0.5f);
                                        reflectionProbe->SetWorldTranslation(insertionPoint);

                                        WeakHandle<Node> previousFocusedNode = editorSubsystem->GetFocusedNode();

                                        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
                                            NAME("AddReflectionProbe"),
                                            Proc<EditorActionFunctions()>([reflectionProbe, previousFocusedNode, activeScene]() -> EditorActionFunctions
                                                {
                                                    return EditorActionFunctions {
                                                        .execute = Proc<void(EditorSubsystem*, EditorProject*)>([reflectionProbe, activeScene](EditorSubsystem* editorSubsystem, EditorProject* project)
                                                            {
                                                                activeScene->GetRoot()->AddChild(reflectionProbe);
                                                                editorSubsystem->SetFocusedNode(reflectionProbe, true);
                                                            }),
                                                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>([reflectionProbe, previousFocusedNode](EditorSubsystem* editorSubsystem, EditorProject* project)
                                                            {
                                                                reflectionProbe->Remove();

                                                                if (editorSubsystem->GetFocusedNode() == reflectionProbe)
                                                                {
                                                                    editorSubsystem->SetFocusedNode(nullptr, true);

                                                                    Handle<Node> focusedNode = previousFocusedNode.Lock();
                                                                    if (focusedNode.IsValid())
                                                                    {
                                                                        editorSubsystem->SetFocusedNode(focusedNode, true);
                                                                    }
                                                                }
                                                            })
                                                    };
                                                }));

                                        InitObject(action);

                                        currentProject->GetActionStack()->Push(action);

                                        // kickoff new lightmap generation task

                                        // kickoff lightmap generation for the new volume
                                        Handle<GenerateLightmapsEditorTask> generateLightmapsTask = MakeHandle<GenerateLightmapsEditorTask>(Array<Handle<ObjectBase>> { reflectionProbe });
                                        InitObject(generateLightmapsTask);

                                        generateLightmapsTask->SetScene(activeScene);
                                        generateLightmapsTask->SetWorld(MakeStrongRef(activeScene->GetWorld()));

                                        g_editorState->AddTask(generateLightmapsTask);
                                    };

                                    GetThreadById(g_simThread)->GetScheduler().Enqueue(std::move(impl), TaskEnqueueFlags::FIRE_AND_FORGET);
                                })
        .Detach();

    testNativeUi->Show();

    return UIEventHandlerResult::OK;
}

UIEventHandlerResult EditorMain::AddLightmapVolume(const MouseEvent& event)
{
    HYP_SCOPE;

    EditorSubsystem* editorSubsystem = m_world->GetSubsystem<EditorSubsystem>();
    if (editorSubsystem == nullptr)
    {
        HYP_LOG(Editor, Error, "EditorSubsystem not found");

        return UIEventHandlerResult::ERR;
    }

    const Handle<EditorProject>& currentProject = editorSubsystem->GetCurrentProject();
    if (!currentProject.IsValid())
    {
        HYP_LOG(Editor, Error, "No project loaded; cannot add lightmap volume!");

        return UIEventHandlerResult::ERR;
    }

    Handle<Scene> activeScene = editorSubsystem->GetActiveScene();
    if (!activeScene.IsValid())
    {
        HYP_LOG(Editor, Error, "No active scene; cannot add lightmap volume!");

        return UIEventHandlerResult::ERR;
    }

    /// \todo : Allow building a bounding box in editor before starting the task.
    BoundingBox lightmapVolumeAabb(Vec3f(-60.0f, -5.0f, -60.0f), Vec3f(60.0f, 40.0f, 60.0f));

    Handle<LightmapVolume> lightmapVolume = MakeHandle<LightmapVolume>(lightmapVolumeAabb);
    lightmapVolume->SetName(Name::Unique("LightmapVolume"));
    InitObject(lightmapVolume);

    WeakHandle<Node> previousFocusedNode = editorSubsystem->GetFocusedNode();

    Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
        NAME("AddLightmapVolume"),
        Proc<EditorActionFunctions()>([lightmapVolume, previousFocusedNode, activeScene]() -> EditorActionFunctions
            {
                return EditorActionFunctions {
                    .execute = Proc<void(EditorSubsystem*, EditorProject*)>([lightmapVolume, activeScene](EditorSubsystem* editorSubsystem, EditorProject* project)
                        {
                            activeScene->GetRoot()->AddChild(lightmapVolume);

                            editorSubsystem->SetFocusedNode(lightmapVolume, true);
                        }),
                    .revert = Proc<void(EditorSubsystem*, EditorProject*)>([lightmapVolume, previousFocusedNode](EditorSubsystem* editorSubsystem, EditorProject* project)
                        {
                            lightmapVolume->Remove();

                            if (editorSubsystem->GetFocusedNode() == lightmapVolume)
                            {
                                editorSubsystem->SetFocusedNode(nullptr, true);

                                Handle<Node> focusedNode = previousFocusedNode.Lock();
                                if (focusedNode.IsValid())
                                {
                                    editorSubsystem->SetFocusedNode(focusedNode, true);
                                }
                            }
                        })
                };
            }));

    InitObject(action);

    currentProject->GetActionStack()->Push(action);

    // kickoff lightmap generation for the new volume
    Handle<GenerateLightmapsEditorTask> generateLightmapsTask = MakeHandle<GenerateLightmapsEditorTask>(lightmapVolume);
    InitObject(generateLightmapsTask);

    generateLightmapsTask->SetScene(activeScene);

    Handle<World> worldHandle = MakeStrongRef(m_world);
    generateLightmapsTask->SetWorld(worldHandle);

    g_editorState->AddTask(generateLightmapsTask);

    return UIEventHandlerResult::OK;
}

UIEventHandlerResult EditorMain::AddParticleVolume(const MouseEvent& event)
{
    HYP_SCOPE;

    HYP_LOG(Editor, Verbose, "Add Particle Volume clicked");

    EditorSubsystem* editorSubsystem = m_world->GetSubsystem<EditorSubsystem>();

    if (editorSubsystem == nullptr)
    {
        HYP_LOG(Editor, Error, "EditorSubsystem not found");

        return UIEventHandlerResult::ERR;
    }

    const Handle<EditorProject>& currentProject = editorSubsystem->GetCurrentProject();
    if (!currentProject.IsValid())
    {
        HYP_LOG(Editor, Error, "No project loaded; cannot add particle volume");

        return UIEventHandlerResult::ERR;
    }

    Handle<Scene> activeScene = editorSubsystem->GetActiveScene();
    if (!activeScene.IsValid())
    {
        HYP_LOG(Editor, Error, "No active scene found");

        return UIEventHandlerResult::ERR;
    }

    ParticleVolumeParams params {};
    params.texture = g_assetManager->Load<Texture>("Textures/spark.png").GetValue().ExtractAs<Handle<Texture>>();
    params.origin = Vec3f(0.0f, 10.0f, 0.0f); // temp
    params.maxParticles = 2048;
    params.hasPhysics = true;
    params.lifespan = 10.0f;

    Handle<ParticleVolume> particleVolume = MakeHandle<ParticleVolume>(BoundingBox(Vec3f(-20.0f, 0.0f, -20.0f), Vec3f(20.0f, 20.0f, 20.0f)), params);
    InitObject(particleVolume);

    const Vec3f insertionPoint = editorSubsystem->CalculateSceneInsertionPoint(5.0f, 0.5f);
    particleVolume->SetWorldTranslation(insertionPoint);

    WeakHandle<Node> previousFocusedNode = editorSubsystem->GetFocusedNode();

    Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
        NAME("AddParticleVolume"),
        Proc<EditorActionFunctions()>([particleVolume, previousFocusedNode, activeScene]() -> EditorActionFunctions
            {
                return EditorActionFunctions {
                    .execute = Proc<void(EditorSubsystem*, EditorProject*)>([particleVolume, activeScene](EditorSubsystem* editorSubsystem, EditorProject* project)
                        {
                            activeScene->GetRoot()->AddChild(particleVolume);
                            editorSubsystem->SetFocusedNode(particleVolume, true);
                        }),
                    .revert = Proc<void(EditorSubsystem*, EditorProject*)>([particleVolume, previousFocusedNode](EditorSubsystem* editorSubsystem, EditorProject* project)
                        {
                            particleVolume->Remove();

                            if (editorSubsystem->GetFocusedNode() == particleVolume)
                            {
                                editorSubsystem->SetFocusedNode(nullptr, true);

                                Handle<Node> focusedNode = previousFocusedNode.Lock();
                                if (focusedNode.IsValid())
                                {
                                    editorSubsystem->SetFocusedNode(focusedNode, true);
                                }
                            }
                        })
                };
            }));

    InitObject(action);

    currentProject->GetActionStack()->Push(action);

    return UIEventHandlerResult::OK;
}

UIEventHandlerResult EditorMain::AddNode(const MouseEvent& event)
{
    HYP_SCOPE;

    HYP_LOG(Editor, Verbose, "Add Node clicked");

    EditorSubsystem* editorSubsystem = m_world->GetSubsystem<EditorSubsystem>();
    if (editorSubsystem == nullptr)
    {
        HYP_LOG(Editor, Error, "EditorSubsystem not found");

        return UIEventHandlerResult::ERR;
    }

    const Handle<EditorProject>& currentProject = editorSubsystem->GetCurrentProject();
    if (!currentProject.IsValid())
    {
        HYP_LOG(Editor, Error, "No project loaded; cannot add node");

        return UIEventHandlerResult::ERR;
    }

    Handle<Scene> activeScene = editorSubsystem->GetActiveScene();
    if (!activeScene.IsValid())
    {
        HYP_LOG(Editor, Error, "No active scene found");

        return UIEventHandlerResult::ERR;
    }

    WeakHandle<Node> currentFocusedNode = editorSubsystem->GetFocusedNode();

    Handle<Node> node = MakeHandle<Node>(NAME("New Node"));
    InitObject(node);

    // Calculate appropriate insertion point in front of camera
    const Vec3f insertionPoint = editorSubsystem->CalculateSceneInsertionPoint(5.0f, 0.5f);
    node->SetWorldTranslation(insertionPoint);

    Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
        NAME("AddNewNode"),
        Proc<EditorActionFunctions()>([node, currentFocusedNode, activeScene]() -> EditorActionFunctions
            {
                return EditorActionFunctions {
                    .execute = Proc<void(EditorSubsystem*, EditorProject*)>([node, activeScene](EditorSubsystem* editorSubsystem, EditorProject* project)
                        {
                            activeScene->GetRoot()->AddChild(node);
                            editorSubsystem->SetFocusedNode(node, true);
                        }),
                    .revert = Proc<void(EditorSubsystem*, EditorProject*)>([node, currentFocusedNode](EditorSubsystem* editorSubsystem, EditorProject* project)
                        {
                            node->Remove();

                            if (editorSubsystem->GetFocusedNode() == node)
                            {
                                editorSubsystem->SetFocusedNode(nullptr, true);

                                Handle<Node> focusedNode = currentFocusedNode.Lock();
                                if (focusedNode.IsValid())
                                {
                                    editorSubsystem->SetFocusedNode(focusedNode, true);
                                }
                            }
                        })
                };
            }));

    InitObject(action);

    currentProject->GetActionStack()->Push(action);

    return UIEventHandlerResult::OK;
}

UIEventHandlerResult EditorMain::AddEntity(const MouseEvent& event)
{
    HYP_SCOPE;

    HYP_LOG(Editor, Verbose, "Add Entity clicked");

    EditorSubsystem* editorSubsystem = m_world->GetSubsystem<EditorSubsystem>();
    if (editorSubsystem == nullptr)
    {
        HYP_LOG(Editor, Error, "EditorSubsystem not found");

        return UIEventHandlerResult::ERR;
    }

    const Handle<EditorProject>& currentProject = editorSubsystem->GetCurrentProject();
    if (!currentProject.IsValid())
    {
        HYP_LOG(Editor, Error, "No project loaded; cannot add entity");

        return UIEventHandlerResult::ERR;
    }

    Handle<Scene> activeScene = editorSubsystem->GetActiveScene();
    if (!activeScene.IsValid())
    {
        HYP_LOG(Editor, Error, "No active scene found");

        return UIEventHandlerResult::ERR;
    }

    WeakHandle<Node> currentFocusedNode = editorSubsystem->GetFocusedNode();

    Handle<Entity> entity = MakeHandle<Entity>(NAME("New Entity"));
    InitObject(entity);

    // Calculate appropriate insertion point in front of camera
    const Vec3f insertionPoint = editorSubsystem->CalculateSceneInsertionPoint(5.0f, 0.5f);
    entity->SetWorldTranslation(insertionPoint);

    Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
        NAME("AddNewEntity"),
        Proc<EditorActionFunctions()>([entity, currentFocusedNode, activeScene]() -> EditorActionFunctions
            {
                return EditorActionFunctions {
                    .execute = Proc<void(EditorSubsystem*, EditorProject*)>([entity, activeScene](EditorSubsystem* editorSubsystem, EditorProject* project)
                        {
                            activeScene->GetRoot()->AddChild(entity);
                            editorSubsystem->SetFocusedNode(entity, true);
                        }),
                    .revert = Proc<void(EditorSubsystem*, EditorProject*)>([entity, currentFocusedNode](EditorSubsystem* editorSubsystem, EditorProject* project)
                        {
                            entity->Remove();

                            if (editorSubsystem->GetFocusedNode() == entity)
                            {
                                editorSubsystem->SetFocusedNode(nullptr, true);

                                Handle<Node> focusedNode = currentFocusedNode.Lock();
                                if (focusedNode.IsValid())
                                {
                                    editorSubsystem->SetFocusedNode(focusedNode, true);
                                }
                            }
                        })
                };
            }));

    InitObject(action);

    currentProject->GetActionStack()->Push(action);

    return UIEventHandlerResult::OK;
}

} // namespace Hyperion
