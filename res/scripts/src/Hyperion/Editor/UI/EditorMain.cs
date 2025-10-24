
using System;
using System.IO;
using System.Linq;
using System.Text;
using Hyperion;
using Hyperion.Editor.UI.Overlays;

namespace Hyperion.Editor.UI;

public class CustomSystem : ScriptableSystem
{
    public CustomSystem()
    {
        Logger.Log(LogType.Info, "CustomSystem constructor called");
    }

    protected override ComponentInfo[] GetComponentInfos()
    {
        return new ComponentInfo[]
        {
            // new ComponentInfo(HypClass.GetClass<LightComponent>().TypeId, ComponentRWFlags.Read, true)
        };
    }

    public override bool AllowUpdate()
    {
        return false;
    }

    public override void OnEntityAdded(Entity entity)
    {
        Logger.Log(LogType.Info, "CustomSystem OnEntityAdded called for entity: " + entity.Id);
    }

    public override void Init()
    {
        base.Init();
        Logger.Log(LogType.Info, "CustomSystem Init called");
    }

    public override void Process(float delta)
    {
    }
}

public class TestEditorTask : LongRunningEditorTask
{
    public TestEditorTask()
    {
    }

    public override void Cancel()
    {
        Logger.Log(LogType.Info, "Cancel task");
    }

    public override bool IsCompleted()
    {
        return false;
    }

    public override void Process()
    {
        Logger.Log(LogType.Info, "Process task! testing");
    }
}


/// <summary>
///  Most of this has been replaced with a C++ implementation (see editor/script/EditorMain.hpp)
/// </summary>
public class EditorMain : UIEventHandler
{
    FirstPersonCameraController? firstPersonCameraController;

    Scene? testScene;

    DelegateHandler? onProjectOpenedDelegate;
    DelegateHandler? onProjectClosingDelegate;
    DelegateHandler? onActionStackStateChangeDelegate;

    public override void Init(Entity entity)
    {
        base.Init(entity);

        Logger.Log(LogType.Info, "EditorMain Init()");

        EditorSubsystem? editorSubsystem = World.GetSubsystem<EditorSubsystem>();
        if (editorSubsystem == null)
        {
            return;
        }

        onProjectOpenedDelegate = editorSubsystem.GetOnProjectOpenedDelegate().Bind((EditorProject proj) =>
        {
            HandleProjectOpened(proj);
        });

        onProjectClosingDelegate = editorSubsystem.GetOnProjectClosingDelegate().Bind((EditorProject proj) =>
        {
            HandleProjectClosing(proj);
        });

        if (editorSubsystem.GetCurrentProject() != null)
        {
            HandleProjectOpened(editorSubsystem.GetCurrentProject());
        }

        editorSubsystem.AddDebugOverlay(new FpsCounter(World));
        editorSubsystem.AddDebugOverlay(new StatOverlay());
    }

    ~EditorMain()
    {
        Logger.Log(LogType.Info, "EditorMain finalizer");
    }

    private void HandleProjectOpened(EditorProject project)
    {
        Logger.Log(LogType.Info, "HandleProjectOpened invoked with project: " + project.GetName().ToString());

        // // test add custom system class...
        // if (project.GetScene() != null)
        // {
        //     project.GetScene().GetEntityManager().AddSystem(new CustomSystem());
        // }

        if (onActionStackStateChangeDelegate != null)
        {
            onActionStackStateChangeDelegate.Remove();
        }

        onActionStackStateChangeDelegate = project.GetActionStack().GetOnStateChangeDelegate().Bind((EditorActionStackState state) =>
        {
            var undoMenuItem = UIObject.GetStage().Find<UIMenuItem>(new Name("Undo_MenuItem", weak: true));

            if (undoMenuItem != null)
            {
                UpdateUndoMenuItem();
                return;
            }

            var redoMenuItem = UIObject.GetStage().Find<UIMenuItem>(new Name("Redo_MenuItem", weak: true));

            if (redoMenuItem != null)
            {
                UpdateRedoMenuItem();
                return;
            }
        });
    }

    private void HandleProjectClosing(EditorProject project)
    {
        onActionStackStateChangeDelegate?.Remove();
        onActionStackStateChangeDelegate = null;

        Logger.Log(LogType.Info, "HandleProjectClosing invoked with project: " + project.GetName().ToString());
    }

    public UIEventHandlerResult UpdateUndoMenuItem()
    {
        try
        {
            Logger.Log(LogType.Info, "UpdateUndoMenuItem");

            UIMenuItem? undoMenuItem = UIObject.GetStage().Find<UIMenuItem>(new Name("Undo_MenuItem", weak: true));

            if (undoMenuItem == null)
            {
                Logger.Log(LogType.Error, "Undo menu item not found");
                return UIEventHandlerResult.Error;
            }

            var editorSubsystem = World.GetSubsystem<EditorSubsystem>();

            if (editorSubsystem == null)
            {
                Logger.Log(LogType.Error, "EditorSubsystem not found");
                return UIEventHandlerResult.Error;
            }

            EditorProject currentProject = editorSubsystem.GetCurrentProject();

            if (currentProject == null)
            {
                Logger.Log(LogType.Error, "No project loaded; cannot update undo menu item");
                return UIEventHandlerResult.Error;
            }

            string undoText = "Undo";

            if (currentProject.GetActionStack().CanUndo())
            {
                var editorAction = currentProject.GetActionStack().GetUndoAction();
                Assert.Throw(editorAction != null, "Undo action should not be null");

                string actionUndoText = editorAction.GetName().ToString();

                if (actionUndoText.Length > 0)
                {
                    undoText = string.Format("Undo {0}", actionUndoText);
                }

                undoMenuItem.SetIsEnabled(true);
            }
            else
            {
                undoText = "Can't Undo";

                undoMenuItem.SetIsEnabled(false);
            }

            undoMenuItem.SetText(undoText);
        }
        catch (Exception ex)
        {
            Logger.Log(LogType.Error, "Error updating undo menu item: " + ex.Message);
            return UIEventHandlerResult.Error;
        }

        return UIEventHandlerResult.Ok;
    }

    public UIEventHandlerResult UpdateRedoMenuItem()
    {
        try
        {
            UIMenuItem? redoMenuItem = UIObject.GetStage().Find<UIMenuItem>(new Name("Redo_MenuItem", weak: true));

            if (redoMenuItem == null)
            {
                Logger.Log(LogType.Error, "Redo menu item not found");
                return UIEventHandlerResult.Error;
            }

            var editorSubsystem = World.GetSubsystem<EditorSubsystem>();

            if (editorSubsystem == null)
            {
                Logger.Log(LogType.Error, "EditorSubsystem not found");
                return UIEventHandlerResult.Error;
            }

            EditorProject currentProject = editorSubsystem.GetCurrentProject();

            if (currentProject == null)
            {
                Logger.Log(LogType.Error, "No project loaded; cannot update redo menu item");
                return UIEventHandlerResult.Error;
            }

            string redoText = "Redo";

            if (currentProject.GetActionStack().CanRedo())
            {
                var editorAction = currentProject.GetActionStack().GetRedoAction();
                Assert.Throw(editorAction != null, "Redo action should not be null");

                string actionRedoText = editorAction.GetName().ToString();

                if (actionRedoText.Length > 0)
                {
                    redoText = string.Format("Redo {0}", actionRedoText);
                }

                redoMenuItem.SetIsEnabled(true);
            }
            else
            {
                redoText = "Can't Redo";

                redoMenuItem.SetIsEnabled(false);
            }

            redoMenuItem.SetText(redoText);
        }
        catch (Exception ex)
        {
            Logger.Log(LogType.Error, "Error updating redo menu item: " + ex.Message);
            return UIEventHandlerResult.Error;
        }

        return UIEventHandlerResult.Ok;
    }

    public override void Destroy()
    {
        Logger.Log(LogType.Info, "EditorMain Destroy()");

        onProjectOpenedDelegate?.Remove();
        onProjectOpenedDelegate = null;

        onProjectClosingDelegate?.Remove();
        onProjectClosingDelegate = null;

        onActionStackStateChangeDelegate?.Remove();
        onActionStackStateChangeDelegate = null;

        if (World != null)
        {
            EditorSubsystem? editorSubsystem = World.GetSubsystem<EditorSubsystem>();
            editorSubsystem?.RemoveDebugOverlay(new Name("FpsCounter", weak: true));
            editorSubsystem?.RemoveDebugOverlay(new Name("StatOverlay", weak: true));
        }
    }

    public override void OnPlayStart()
    {
        // Logger.Log(LogType.Info, "OnPlayStart");

        // var editorSubsystem = World.GetSubsystem<EditorSubsystem>();
        // editorSubsystem.GetScene().RemoveFromWorld();

        // // firstPersonCameraController = new FirstPersonCameraController();
        // // editorSubsystem.GetScene().GetPrimaryCamera().AddCameraController(firstPersonCameraController);

        // // firstPersonCameraController.SetMode(FirstPersonCameraControllerMode.MouseLocked);
        // ((Scene)testScene).GetRoot().AddChild(editorSubsystem.GetScene().GetRoot());
        // ((Scene)testScene).AddToWorld(World);
    }

    public override void OnPlayStop()
    {
        // Logger.Log(LogType.Info, "OnPlayStop");

        // var editorSubsystem = World.GetSubsystem<EditorSubsystem>();

        // ((Scene)testScene).RemoveFromWorld();
        // editorSubsystem.GetScene().SetRoot(((Scene)testScene).GetRoot().GetChild(0));

        // editorSubsystem.GetScene().AddToWorld(World);

        // // if (!editorSubsystem.GetScene().GetPrimaryCamera().RemoveCameraController(firstPersonCameraController))
        // // {
        // //     Logger.Log(LogType.Error, "Failed to remove camera controller!");

        // //     return;
        // // }

        // firstPersonCameraController = null;

        // Logger.Log(LogType.Info, "Camera controller removed");
    }


    public UIEventHandlerResult OpenProjectClicked()
    {
        Logger.Log(LogType.Info, "Open Project clicked");

        var editorSubsystem = World.GetSubsystem<EditorSubsystem>();

        if (editorSubsystem == null)
        {
            Logger.Log(LogType.Error, "EditorSubsystem not found");

            return UIEventHandlerResult.Error;
        }

        editorSubsystem.ShowOpenProjectDialog();

        return UIEventHandlerResult.Ok;
    }

    public UIEventHandlerResult SaveClicked()
    {
        Logger.Log(LogType.Info, "Save clicked");

        var editorSubsystem = World.GetSubsystem<EditorSubsystem>();

        if (editorSubsystem == null)
        {
            Logger.Log(LogType.Error, "EditorSubsystem not found");

            return UIEventHandlerResult.Error;
        }

        EditorProject currentProject = editorSubsystem.GetCurrentProject();

        if (currentProject == null)
        {
            Logger.Log(LogType.Error, "No project loaded; cannot save");

            return UIEventHandlerResult.Error;
        }

        Result saveResult = currentProject.Save();

        if (!saveResult.IsValid)
        {
            Logger.Log(LogType.Error, "Failed to save project");

            return UIEventHandlerResult.Error;
        }

        return UIEventHandlerResult.Ok;
    }

    public UIEventHandlerResult SimulateClicked()
    {
        // Test: Force GC
        GC.Collect();

        World world = this.World;

        if (world.GetGameState().Mode == GameStateMode.Simulating)
        {
            Logger.Log(LogType.Info, "Stop simulation");

            world.StopSimulating();
            return UIEventHandlerResult.Ok;
        }

        Logger.Log(LogType.Info, "Start simulation");

        world.StartSimulating();

        var editorSubsystem = World.GetSubsystem<EditorSubsystem>();

        if (editorSubsystem == null)
        {
            Logger.Log(LogType.Error, "EditorSubsystem not found");

            return UIEventHandlerResult.Error;
        }

        return UIEventHandlerResult.Ok;
    }

    public UIEventHandlerResult GenerateLightmapsClicked()
    {
        Logger.Log(LogType.Info, "Generate Lightmaps clicked");

        var editorSubsystem = World.GetSubsystem<EditorSubsystem>();

        if (editorSubsystem == null)
        {
            Logger.Log(LogType.Error, "EditorSubsystem not found");

            return UIEventHandlerResult.Error;
        }

        Scene scene = editorSubsystem.GetActiveScene();

        if (scene == null)
        {
            Logger.Log(LogType.Error, "No active scene; cannot generate lightmaps");

            return UIEventHandlerResult.Error;
        }

        World world = this.World;
        Assert.Throw(world != null);

        // @TODO: Allow building a bounding box in editor before starting the task.
        BoundingBox lightmapVolumeAabb = new BoundingBox(new Vec3f(-60.0f, -5.0f, -60.0f), new Vec3f(60.0f, 40.0f, 60.0f));

        GenerateLightmapsEditorTask generateLightmapsTask = new GenerateLightmapsEditorTask();
        generateLightmapsTask.SetScene(scene);
        generateLightmapsTask.SetWorld(world);
        generateLightmapsTask.SetAABB(lightmapVolumeAabb);

        editorSubsystem.AddTask(generateLightmapsTask);

        return UIEventHandlerResult.Ok;
    }

    public UIEventHandlerResult AddPointLight()
    {
        Logger.Log(LogType.Info, "Add Point Light clicked");

        var editorSubsystem = World.GetSubsystem<EditorSubsystem>();

        if (editorSubsystem == null)
        {
            Logger.Log(LogType.Error, "EditorSubsystem not found");

            return UIEventHandlerResult.Error;
        }

        EditorProject currentProject = editorSubsystem.GetCurrentProject();

        if (currentProject == null)
        {
            Logger.Log(LogType.Error, "No project loaded; cannot add point light");

            return UIEventHandlerResult.Error;
        }

        Scene activeScene = editorSubsystem.GetActiveScene();

        if (activeScene == null)
        {
            Logger.Log(LogType.Error, "No active scene found");

            return UIEventHandlerResult.Error;
        }

        PointLight light = activeScene.GetEntityManager().AddEntity<PointLight>();
        light.SetColor(new Color(1.0f, 1.0f, 1.0f, 1.0f));
        light.SetRadius(10.0f);
        light.SetIntensity(3.0f);
        // light.SetLightFlags(((light.GetLightFlags() & ~LightFlags.Shadow) & ~LightFlags.ShadowFilterMask));

        light.SetName(activeScene.GetUniqueNodeName("PointLight"));
        light.SetWorldTranslation(new Vec3f(0.0f, 3.0f, 5.0f));

        WeakReference<Node> previousFocusedNode = new WeakReference<Node>(editorSubsystem.GetFocusedNode());

        currentProject.GetActionStack().Push(new EditorAction(
            new Name("AddPointLight"),
            (EditorSubsystem editorSubsystem, EditorProject project) =>
            {
                activeScene.GetRoot().AddChild(light);
                editorSubsystem.SetFocusedNode(light, true);
            },
            (EditorSubsystem editorSubsystem, EditorProject project) =>
            {
                light.Remove();

                if (editorSubsystem.GetFocusedNode() == light)
                {
                    editorSubsystem.SetFocusedNode(null, true);

                    if (previousFocusedNode.TryGetTarget(out Node? focusedNode) && focusedNode != null)
                    {
                        editorSubsystem.SetFocusedNode(focusedNode, true);
                    }
                }
            }
        ));

        return UIEventHandlerResult.Ok;
    }

    public UIEventHandlerResult AddDirectionalLight()
    {
        // var editorSubsystem = World.GetSubsystem<EditorSubsystem>();

        // if (editorSubsystem == null)
        // {
        //     Logger.Log(LogType.Error, "EditorSubsystem not found");

        //     return UIEventHandlerResult.Error;
        // }

        // EditorProject currentProject = editorSubsystem.GetCurrentProject();

        // if (currentProject == null)
        // {
        //     Logger.Log(LogType.Error, "No project loaded; cannot save");

        //     return UIEventHandlerResult.Error;
        // }

        // var light = new Light();
        // light.SetLightType(LightType.Directional);
        // light.SetPosition(new Vec3f(-0.5f, 0.5f, 0.0f).Normalize());
        // light.SetColor(new Color(1.0f, 1.0f, 1.0f, 1.0f));
        // light.SetIntensity(5.0f);

        // var lightEntity = currentProject.GetScene().GetEntityManager().AddEntity();

        // currentProject.GetScene().GetEntityManager().AddComponent<LightComponent>(lightEntity, new LightComponent {
        //     Light = light
        // });

        // var lightNode = new Node();
        // lightNode.SetName(currentProject.GetScene().GetUniqueNodeName("DirectionalLight"));
        // lightNode.SetEntity(lightEntity);
        // lightNode.SetWorldTranslation(new Vec3f(-0.5f, 0.5f, 0.0f).Normalize());

        // currentProject.GetActionStack().Push(new EditorAction(
        //     new Name("AddDirectionalLight"),
        //    (EditorSubsystem editorSubsystem, EditorProject project) =>
        //     {
        //         currentProject.GetScene().GetRoot().AddChild(lightNode);
        //         editorSubsystem.SetFocusedNode(lightNode, true);
        //     },
        //    (EditorSubsystem editorSubsystem, EditorProject project) =>
        //     {
        //         lightNode.Remove();

        //         if (editorSubsystem.GetFocusedNode() == lightNode)
        //             editorSubsystem.SetFocusedNode(null, true);
        //     }
        // ));

        return UIEventHandlerResult.Ok;
    }

    public UIEventHandlerResult AddSpotLight()
    {
        // var editorSubsystem = World.GetSubsystem<EditorSubsystem>();

        // if (editorSubsystem == null)
        // {
        //     Logger.Log(LogType.Error, "EditorSubsystem not found");

        //     return UIEventHandlerResult.Error;
        // }

        // EditorProject currentProject = editorSubsystem.GetCurrentProject();

        // if (currentProject == null)
        // {
        //     Logger.Log(LogType.Error, "No project loaded; cannot save");

        //     return UIEventHandlerResult.Error;
        // }

        // var light = new Light();
        // light.SetLightType(LightType.Spot);
        // light.SetPosition(new Vec3f(0.0f, 0.0f, 0.0f));
        // light.SetColor(new Color(1.0f, 1.0f, 1.0f, 1.0f));
        // light.SetRadius(10.0f);
        // light.SetIntensity(5.0f);
        // light.SetSpotAngles(new Vec2f(30.0f * MathF.PI / 180.0f, 60.0f * MathF.PI / 180.0f));

        // var lightEntity = currentProject.GetScene().GetEntityManager().AddEntity();

        // currentProject.GetScene().GetEntityManager().AddComponent<LightComponent>(lightEntity, new LightComponent {
        //     Light = light
        // });

        // var lightNode = new Node();
        // lightNode.SetName(currentProject.GetScene().GetUniqueNodeName("SpotLight"));
        // lightNode.SetEntity(lightEntity);
        // lightNode.SetWorldTranslation(new Vec3f(0.0f, 0.0f, 0.0f));

        // currentProject.GetActionStack().Push(new EditorAction(
        //     new Name("AddSpotLight"),
        //     (EditorSubsystem editorSubsystem, EditorProject project) =>
        //     {
        //         currentProject.GetScene().GetRoot().AddChild(lightNode);
        //         editorSubsystem.SetFocusedNode(lightNode, true);
        //     },
        //     (EditorSubsystem editorSubsystem, EditorProject project) =>
        //     {
        //         lightNode.Remove();

        //         if (editorSubsystem.GetFocusedNode() == lightNode)
        //             editorSubsystem.SetFocusedNode(null, true);
        //     }
        // ));

        return UIEventHandlerResult.Ok;
    }

    public UIEventHandlerResult AddAreaRectLight()
    {

        Logger.Log(LogType.Info, "Add AreaRect Light clicked");

        var editorSubsystem = World.GetSubsystem<EditorSubsystem>();

        if (editorSubsystem == null)
        {
            Logger.Log(LogType.Error, "EditorSubsystem not found");

            return UIEventHandlerResult.Error;
        }

        EditorProject currentProject = editorSubsystem.GetCurrentProject();

        if (currentProject == null)
        {
            Logger.Log(LogType.Error, "No project loaded; cannot add AreaRect light");

            return UIEventHandlerResult.Error;
        }

        Scene activeScene = editorSubsystem.GetActiveScene();

        if (activeScene == null)
        {
            Logger.Log(LogType.Error, "No active scene found");

            return UIEventHandlerResult.Error;
        }

        AreaRectLight light = activeScene.GetEntityManager().AddEntity<AreaRectLight>();
        light.SetPosition(new Vec3f(0.0f, 5.0f, 0.0f));
        light.SetColor(new Color(1.0f, 1.0f, 1.0f, 1.0f));
        light.SetRadius(30.0f);
        light.SetIntensity(5.0f);
        light.SetAreaSize(new Vec2f(2.0f, 2.0f));

        light.SetName(activeScene.GetUniqueNodeName("AreaRectLight"));
        light.SetWorldTranslation(new Vec3f(0.0f, 5.0f, 0.0f));

        WeakReference<Node> previousFocusedNode = new WeakReference<Node>(editorSubsystem.GetFocusedNode());

        currentProject.GetActionStack().Push(new EditorAction(
            new Name("AddAreaRectLight"),
            (EditorSubsystem editorSubsystem, EditorProject project) =>
            {
                activeScene.GetRoot().AddChild(light);
                editorSubsystem.SetFocusedNode(light, true);
            },
            (EditorSubsystem editorSubsystem, EditorProject project) =>
            {
                light.Remove();

                if (editorSubsystem.GetFocusedNode() == light)
                {
                    editorSubsystem.SetFocusedNode(null, true);

                    if (previousFocusedNode.TryGetTarget(out Node? focusedNode) && focusedNode != null)
                    {
                        editorSubsystem.SetFocusedNode(focusedNode, true);
                    }
                }
            }
        ));

        return UIEventHandlerResult.Ok;
    }

    public UIEventHandlerResult AddReflectionProbe()
    {
        var editorSubsystem = World.GetSubsystem<EditorSubsystem>();

        if (editorSubsystem == null)
        {
            Logger.Log(LogType.Error, "EditorSubsystem not found");

            return UIEventHandlerResult.Error;
        }

        EditorProject currentProject = editorSubsystem.GetCurrentProject();

        if (currentProject == null)
        {
            Logger.Log(LogType.Error, "No project loaded; cannot save");

            return UIEventHandlerResult.Error;
        }

        Scene activeScene = editorSubsystem.GetActiveScene();

        if (activeScene == null)
        {
            Logger.Log(LogType.Error, "No active scene found");

            return UIEventHandlerResult.Error;
        }

        var reflectionProbe = activeScene.GetEntityManager().AddEntity<ReflectionProbe>();
        activeScene.GetEntityManager().AddComponent<BoundingBoxComponent>(reflectionProbe, new BoundingBoxComponent
        {
            LocalAABB = new BoundingBox(new Vec3f(-15.0f, 0.0f, -15.0f), new Vec3f(15.0f, 15.0f, 15.0f)),
            WorldAABB = new BoundingBox(new Vec3f(-15.0f, 0.0f, -15.0f), new Vec3f(15.0f, 15.0f, 15.0f))
        });

        reflectionProbe.SetName(activeScene.GetUniqueNodeName("ReflectionProbe"));
        reflectionProbe.SetWorldTranslation(new Vec3f(0.0f, 5.0f, 0.0f));

        WeakReference<Node> previousFocusedNode = new WeakReference<Node>(editorSubsystem.GetFocusedNode());

        currentProject.GetActionStack().Push(new EditorAction(
            new Name("AddReflectionProbe"),
            (EditorSubsystem editorSubsystem, EditorProject project) =>
            {
                activeScene.GetRoot().AddChild(reflectionProbe);
                editorSubsystem.SetFocusedNode(reflectionProbe, true);
            },
            (EditorSubsystem editorSubsystem, EditorProject project) =>
            {
                reflectionProbe.Remove();

                if (editorSubsystem.GetFocusedNode() == reflectionProbe)
                {
                    editorSubsystem.SetFocusedNode(null, true);

                    if (previousFocusedNode.TryGetTarget(out Node? focusedNode) && focusedNode != null)
                    {
                        editorSubsystem.SetFocusedNode(focusedNode, true);
                    }
                }
            }
        ));

        return UIEventHandlerResult.Ok;
    }

    public UIEventHandlerResult UndoClicked()
    {
        Logger.Log(LogType.Info, "Undo clicked");

        var editorSubsystem = World.GetSubsystem<EditorSubsystem>();

        if (editorSubsystem == null)
        {
            Logger.Log(LogType.Error, "EditorSubsystem not found");

            return UIEventHandlerResult.Error;
        }

        EditorProject currentProject = editorSubsystem.GetCurrentProject();

        if (currentProject == null)
        {
            Logger.Log(LogType.Error, "No project loaded; cannot save");

            return UIEventHandlerResult.Error;
        }

        if (!currentProject.GetActionStack().CanUndo())
        {
            Logger.Log(LogType.Info, "Cannot undo, stack is empty");

            return UIEventHandlerResult.Ok;
        }

        currentProject.GetActionStack().Undo();

        return UIEventHandlerResult.Ok;
    }

    public UIEventHandlerResult RedoClicked()
    {
        Logger.Log(LogType.Info, "Redo clicked");

        var editorSubsystem = World.GetSubsystem<EditorSubsystem>();

        if (editorSubsystem == null)
        {
            Logger.Log(LogType.Error, "EditorSubsystem not found");

            return UIEventHandlerResult.Error;
        }

        EditorProject currentProject = editorSubsystem.GetCurrentProject();

        if (currentProject == null)
        {
            Logger.Log(LogType.Error, "No project loaded; cannot save");

            return UIEventHandlerResult.Error;
        }

        if (!currentProject.GetActionStack().CanRedo())
        {
            Logger.Log(LogType.Info, "Cannot redo, stack is empty");

            return UIEventHandlerResult.Ok;
        }

        currentProject.GetActionStack().Redo();

        return UIEventHandlerResult.Ok;
    }

    public UIEventHandlerResult AddNode()
    {
        var editorSubsystem = World.GetSubsystem<EditorSubsystem>();

        if (editorSubsystem == null)
        {
            Logger.Log(LogType.Error, "EditorSubsystem not found");

            return UIEventHandlerResult.Ok;
        }

        EditorProject currentProject = editorSubsystem.GetCurrentProject();

        if (currentProject == null)
        {
            Logger.Log(LogType.Error, "No project loaded; cannot save");

            return UIEventHandlerResult.Ok;
        }

        Scene activeScene = editorSubsystem.GetActiveScene();

        if (activeScene == null)
        {
            Logger.Log(LogType.Error, "No active scene found");

            return UIEventHandlerResult.Error;
        }

        WeakReference<Node> currentFocusedNode = new WeakReference<Node>(editorSubsystem.GetFocusedNode());

        var node = new Node();
        node.SetName(new Name("New Node"));

        currentProject.GetActionStack().Push(new EditorAction(
            new Name("AddNewNode"),
            (EditorSubsystem editorSubsystem, EditorProject project) =>
            {
                activeScene.GetRoot().AddChild(node);

                editorSubsystem.SetFocusedNode(node, true);
            },
            (EditorSubsystem editorSubsystem, EditorProject project) =>
            {
                node.Remove();

                if (editorSubsystem.GetFocusedNode() == node)
                {
                    // If the focused node was the node we just removed, clear the focus
                    editorSubsystem.SetFocusedNode(null, true);

                    // If we had a previous focused node, restore it
                    if (currentFocusedNode != null && currentFocusedNode.TryGetTarget(out var previousFocusedNode))
                    {
                        editorSubsystem.SetFocusedNode(previousFocusedNode, true);
                    }
                }
            }
        ));

        // @TODO Focus on node in scene view

        return UIEventHandlerResult.Ok;
    }

    public UIEventHandlerResult AddEntity()
    {
        var editorSubsystem = World.GetSubsystem<EditorSubsystem>();

        if (editorSubsystem == null)
        {
            Logger.Log(LogType.Error, "EditorSubsystem not found");

            return UIEventHandlerResult.Ok;
        }

        EditorProject currentProject = editorSubsystem.GetCurrentProject();

        if (currentProject == null)
        {
            Logger.Log(LogType.Error, "No project loaded; cannot save");

            return UIEventHandlerResult.Ok;
        }

        Scene activeScene = editorSubsystem.GetActiveScene();

        if (activeScene == null)
        {
            Logger.Log(LogType.Error, "No active scene found");

            return UIEventHandlerResult.Error;
        }

        WeakReference<Node> currentFocusedNode = new WeakReference<Node>(editorSubsystem.GetFocusedNode());

        var entity = new Entity();
        entity.SetName(new Name("New Entity"));

        currentProject.GetActionStack().Push(new EditorAction(
            new Name("AddNewEntity"),
            (EditorSubsystem editorSubsystem, EditorProject project) =>
            {
                activeScene.GetRoot().AddChild(entity);

                editorSubsystem.SetFocusedNode(entity, true);
            },
            (EditorSubsystem editorSubsystem, EditorProject project) =>
            {
                entity.Remove();

                if (editorSubsystem.GetFocusedNode() == entity)
                {
                    // If the focused node was the entity we just removed, clear the focus
                    editorSubsystem.SetFocusedNode(null, true);

                    // If we had a previous focused node, restore it
                    if (currentFocusedNode != null && currentFocusedNode.TryGetTarget(out var previousFocusedNode))
                    {
                        editorSubsystem.SetFocusedNode(previousFocusedNode, true);
                    }
                }
            }
        ));

        // @TODO Focus on entity in scene view

        return UIEventHandlerResult.Ok;
    }
}