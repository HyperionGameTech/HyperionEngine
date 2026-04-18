#include <editor/EditorCommand.hpp>
#include <editor/EditorSubsystem.hpp>
#include <editor/EditorProject.hpp>
#include <editor/EditorState.hpp>

#include <scene/Scene.hpp>
#include <scene/World.hpp>
#include <scene/EntityManager.hpp>
#include <scene/Light.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/LightmapVolume.hpp>
#include <scene/InstancedMeshProxy.hpp>

#include <scene/ParticleVolume.hpp>
#include <scene/FogVolume.hpp>

#include <scene/components/BoundingBoxComponent.hpp>

#include <Core/reflection/ClassUtils.hpp>

#include <Core/logging/Logger.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetBatch.hpp>
#include <asset/AssetRegistry.hpp>

#include <rendering/Texture.hpp>
#include <rendering/Mesh.hpp>

#include <scene/animation/Skeleton.hpp>

#include <engine/EngineGlobals.hpp>

#include <system/OpenFileDialog.hpp>
#include <system/SaveFileDialog.hpp>
#include <system/SelectFolderDialog.hpp>

#include <ui/UISubsystem.hpp>
#include <ui/overlays/Overlay.hpp>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);
HYP_DECLARE_LOG_CHANNEL(Console);

namespace CoreApi {
extern FilePath GetExecutablePath();
} // namespace CoreApi

extern const FilePath& GetProjectsDirectory();
extern const FilePath& GetDataDirectory();

#define DEFINE_EDITOR_COMMAND(name)                                        \
    HYP_API const Class* g_clsEditorCommand##name = nullptr;               \
                                                                           \
    HYP_BEGIN_CLASS(EditorCommand##name, -1, 0, NAME("EditorCommandBase")) \
    HYP_END_CLASS                                                          \
                                                                           \
    HYP_EXPORT TClassStaticInit<EditorCommand##name> g_classInit##EditorCommand##name {};

#pragma region Undo

class HYP_API EditorCommandUndo final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandUndo);

public:
    virtual ~EditorCommandUndo() override = default;

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        EditorProject* project = subsystem->GetCurrentProject();
        if (project != nullptr)
        {
            project->GetActionStack()->Undo();
        }
    }
};

DEFINE_EDITOR_COMMAND(Undo);

#pragma endregion Undo

#pragma region Redo

class HYP_API EditorCommandRedo final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandRedo);

public:
    virtual ~EditorCommandRedo() override = default;

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        EditorProject* project = subsystem->GetCurrentProject();
        if (project != nullptr)
        {
            project->GetActionStack()->Redo();
        }
    }
};

DEFINE_EDITOR_COMMAND(Redo);

#pragma endregion Redo

#pragma region NewProject

class HYP_API EditorCommandNewProject final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandNewProject);

public:
    virtual ~EditorCommandNewProject() override = default;

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        subsystem->NewProject();
    }
};

DEFINE_EDITOR_COMMAND(NewProject);

#pragma endregion NewProject

#pragma region OpenProject

class HYP_API EditorCommandOpenProject final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandOpenProject);

public:
    virtual ~EditorCommandOpenProject() override = default;

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        HYP_SCOPE;

        const FilePath dir = GetProjectsDirectory();
        dir.MkDir();

        ShowOpenFileDialog(
            "Select the project to open",
            dir,
            { "hypproject" },
            /* allowMultiple */ false, /* allowDirectories */ true,
            [weakSubsystem = MakeWeakRef(subsystem)](TResult<Array<FilePath>>&& result) mutable
            {
                if (result.HasError())
                {
                    HYP_LOG(Editor, Error, "Failed to select project file: {}", result.GetError().GetMessage());
                    return;
                }

                if (result->Empty())
                {
                    HYP_LOG(Editor, Warning, "No project file selected.");
                    return;
                }

                GetThreadById(g_simThread)->GetScheduler().Enqueue([weakSubsystem = std::move(weakSubsystem), projectFilepath = std::move(result.GetValue()[0])]() mutable
                    {
                        Handle<EditorSubsystem> subsystem = weakSubsystem.Lock();
                        if (!subsystem)
                        {
                            HYP_LOG(Editor, Error, "Failed to lock EditorSubsystem from weak reference in ShowOpenProjectDialog");
                            return;
                        }

                        subsystem->CloseProject();

                        TResult<Handle<EditorProject>> loadProjectResult = EditorProject::Load(projectFilepath);

                        if (loadProjectResult.HasError())
                        {
                            HYP_LOG(Editor, Error, "Failed to load project: {}", loadProjectResult.GetError().GetMessage());
                            return;
                        }

                        Handle<EditorProject> project = loadProjectResult.GetValue();

                        if (!project.IsValid())
                        {
                            HYP_LOG(Editor, Error, "Loaded project is invalid.");
                            return;
                        }

                        subsystem->OpenProject(project);
                    },
                    TaskEnqueueFlags::FIRE_AND_FORGET);
            });
    }
};

DEFINE_EDITOR_COMMAND(OpenProject);

#pragma endregion OpenProject

#pragma region SaveProject

class EditorCommandSaveProjectAs;

class HYP_API EditorCommandSaveProject final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandSaveProject);

public:
    virtual ~EditorCommandSaveProject() override = default;

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        EditorProject* project = subsystem->GetCurrentProject();
        if (project != nullptr)
        {
            if (!project->IsSaved())
            {
                Handle<EditorCommandSaveProjectAs> saveAs = MakeHandle<EditorCommandSaveProjectAs>();
                reinterpret_cast<EditorCommandBase&>(*saveAs).Execute(subsystem);

                return;
            }

            Result result = project->Save();
            if (!result)
            {
                HYP_LOG(Editor, Error, "Failed to save project: {}", result.GetError().GetMessage());
            }
        }
    }
};

DEFINE_EDITOR_COMMAND(SaveProject);

#pragma endregion SaveProject

#pragma region SaveProjectAs

class HYP_API EditorCommandSaveProjectAs final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandSaveProjectAs);

public:
    virtual ~EditorCommandSaveProjectAs() override = default;

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        EditorProject* project = subsystem->GetCurrentProject();
        if (project != nullptr)
        {
            FilePath dir;

            if (project->IsSaved())
            {
                dir = project->GetFilePath().BasePath();
            }
            else
            {
                dir = GetProjectsDirectory() / *project->GetName();
            }

            dir.MkDir();

            ShowSelectFolderDialog(
                "Select project folder",
                dir,
                [weakSubsystem = MakeWeakRef(subsystem)](TResult<FilePath>&& result) mutable
                {
                    if (result.HasError())
                    {
                        HYP_LOG(Editor, Error, "Failed to select project directory: {}", result.GetError().GetMessage());
                        return;
                    }

                    FilePath selectedPath = result.GetValue();
                    if (selectedPath.Empty())
                    {
                        HYP_LOG(Editor, Warning, "No save path selected.");
                        return;
                    }

                    GetThreadById(g_simThread)->GetScheduler().Enqueue([weakSubsystem = std::move(weakSubsystem), selectedPath = std::move(selectedPath)]() mutable
                        {
                            Handle<EditorSubsystem> subsystem = weakSubsystem.Lock();
                            if (!subsystem)
                            {
                                HYP_LOG(Editor, Error, "Failed to lock EditorSubsystem from weak reference in ShowSaveProjectDialog");
                                return;
                            }

                            EditorProject* project = subsystem->GetCurrentProject();
                            if (!project)
                            {
                                HYP_LOG(Editor, Error, "No current project in EditorSubsystem; cannot save project as.");
                                return;
                            }

                            Result saveResult = project->SaveAs(selectedPath);
                            if (!saveResult)
                            {
                                HYP_LOG(Editor, Error, "Failed to save project as '{}': {}", selectedPath, saveResult.GetError().GetMessage());
                            }
                        },
                        TaskEnqueueFlags::FIRE_AND_FORGET);
                });
        }
    }
};

DEFINE_EDITOR_COMMAND(SaveProjectAs);

#pragma endregion SaveProjectAs

#pragma region AddLightmapVolume

class HYP_API EditorCommandAddLightmapVolume final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandAddLightmapVolume);

public:
    virtual ~EditorCommandAddLightmapVolume() override = default;

    virtual String GetText() const override
    {
        return "Add Lightmap Volume";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();
        if (!currentProject.IsValid())
        {
            HYP_LOG(Editor, Error, "No project loaded; cannot add lightmap volume!");

            return;
        }

        Handle<Scene> activeScene = subsystem->GetActiveScene();
        if (!activeScene.IsValid())
        {
            HYP_LOG(Editor, Error, "No active scene; cannot add lightmap volume!");

            return;
        }

        BoundingBox lightmapVolumeAabb(Vec3f(-60.0f, -5.0f, -60.0f), Vec3f(60.0f, 40.0f, 60.0f));

        Handle<LightmapVolume> lightmapVolume = MakeHandle<LightmapVolume>(lightmapVolumeAabb);
        lightmapVolume->SetName(Name::Unique("LightmapVolume"));
        InitObject(lightmapVolume);

        WeakHandle<Node> previousFocusedNode = subsystem->GetFocusedNode();

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            GetText(),
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

        currentProject->GetActionStack()->PushAction(action);

#if 0
        // kickoff lightmap generation for the new volume
        Handle<GenerateLightmapsEditorTask> generateLightmapsTask = MakeHandle<GenerateLightmapsEditorTask>(lightmapVolume);
        InitObject(generateLightmapsTask);

        generateLightmapsTask->SetScene(activeScene);

        Handle<World> worldHandle = MakeStrongRef(subsystem->GetWorld());
        generateLightmapsTask->SetWorld(worldHandle);

        g_editorState->AddTask(generateLightmapsTask);
#endif
    }
};

DEFINE_EDITOR_COMMAND(AddLightmapVolume);

#pragma endregion AddLightmapVolume

#pragma region AddReflectionProbe

class HYP_API EditorCommandAddReflectionProbe final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandAddReflectionProbe);

public:
    virtual ~EditorCommandAddReflectionProbe() override = default;

    virtual String GetText() const override
    {
        return "Add Reflection Probe";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();
        if (!currentProject.IsValid())
        {
            HYP_LOG(Editor, Error, "No project loaded; cannot add reflection probe!");

            return;
        }

        Handle<Scene> activeScene = subsystem->GetActiveScene();
        if (!activeScene.IsValid())
        {
            HYP_LOG(Editor, Error, "No active scene; cannot add reflection probe!");

            return;
        }

        Handle<ReflectionProbe> reflectionProbe = MakeHandle<ReflectionProbe>(BoundingBox(Vec3f(-10.0f), Vec3f(10.0f)), Vec2u(128));
        reflectionProbe->SetIsBaked(true);
        InitObject(reflectionProbe);

        WeakHandle<Node> previousFocusedNode = subsystem->GetFocusedNode();

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            GetText(),
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

        currentProject->GetActionStack()->PushAction(action);

        if (!reflectionProbe->IsRealtime())
        {
            // kickoff task to generate reflection cubemap
            Handle<GenerateLightmapsEditorTask> generateLightmapsTask = MakeHandle<GenerateLightmapsEditorTask>(reflectionProbe);
            InitObject(generateLightmapsTask);

            generateLightmapsTask->SetScene(activeScene);

            Handle<World> worldHandle = MakeStrongRef(subsystem->GetWorld());
            generateLightmapsTask->SetWorld(worldHandle);

            g_editorState->AddTask(generateLightmapsTask);
        }
    }
};

DEFINE_EDITOR_COMMAND(AddReflectionProbe);

#pragma endregion AddReflectionProbe

#pragma region AddParticleVolume

class HYP_API EditorCommandAddParticleVolume final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandAddParticleVolume);

public:
    virtual ~EditorCommandAddParticleVolume() override = default;

    virtual String GetText() const override
    {
        return  "Add Particle Volume";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {

        const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();
        if (!currentProject.IsValid())
        {
            HYP_LOG(Editor, Error, "No project loaded; cannot add particle volume");

            return;
        }

        Handle<Scene> activeScene = subsystem->GetActiveScene();
        if (!activeScene.IsValid())
        {
            HYP_LOG(Editor, Error, "No active scene found");

            return;
        }

        ParticleVolumeParams params {};
        params.texture = g_assetManager->Load<Texture>("Textures/spark.png").GetValue().ExtractAs<Handle<Texture>>();
        params.origin = Vec3f(0.0f, 10.0f, 0.0f); // temp
        params.maxParticles = 2048;
        params.hasPhysics = true;
        params.lifespan = 3.5f;
        params.startSize = 0.05f;
        params.randomness = 0.8f;

        Handle<ParticleVolume> particleVolume = MakeHandle<ParticleVolume>(BoundingBox(Vec3f(-20.0f, 0.0f, -20.0f), Vec3f(20.0f, 20.0f, 20.0f)), params);
        InitObject(particleVolume);

        const Vec3f insertionPoint = subsystem->CalculateSceneInsertionPoint(5.0f, 0.5f);
        particleVolume->SetWorldTranslation(insertionPoint);

        WeakHandle<Node> previousFocusedNode = subsystem->GetFocusedNode();

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            GetText(),
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

        currentProject->GetActionStack()->PushAction(action);
    }
};

DEFINE_EDITOR_COMMAND(AddParticleVolume);

#pragma endregion AddParticleVolume

#pragma region AddFogVolume

class HYP_API EditorCommandAddFogVolume final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandAddFogVolume);

public:
    virtual ~EditorCommandAddFogVolume() override = default;

    virtual String GetText() const override
    {
        return "Add Fog Volume";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();
        if (!currentProject.IsValid())
        {
            HYP_LOG(Editor, Error, "No project loaded; cannot add fog volume");

            return;
        }

        Handle<Scene> activeScene = subsystem->GetActiveScene();
        if (!activeScene.IsValid())
        {
            HYP_LOG(Editor, Error, "No active scene found");

            return;
        }

        Handle<FogVolume> fogVolume = MakeHandle<FogVolume>(BoundingBox(Vec3f(-20.0f, 0.0f, -20.0f), Vec3f(20.0f, 30.0f, 20.0f)));
        InitObject(fogVolume);

        WeakHandle<Node> previousFocusedNode = subsystem->GetFocusedNode();

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            GetText(),
            Proc<EditorActionFunctions()>([fogVolume, previousFocusedNode, activeScene]() -> EditorActionFunctions
                {
                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem*, EditorProject*)>([fogVolume, activeScene](EditorSubsystem* editorSubsystem, EditorProject* project)
                            {
                                activeScene->GetRoot()->AddChild(fogVolume);
                                editorSubsystem->SetFocusedNode(fogVolume, true);
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>([fogVolume, previousFocusedNode](EditorSubsystem* editorSubsystem, EditorProject* project)
                            {
                                fogVolume->Remove();

                                if (editorSubsystem->GetFocusedNode() == fogVolume)
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

        currentProject->GetActionStack()->PushAction(action);

        // start baking fog volume

        Handle<GenerateLightmapsEditorTask> generateLightmapsTask = MakeHandle<GenerateLightmapsEditorTask>(
            Array<Handle<ObjectBase>> { fogVolume });

        InitObject(generateLightmapsTask);

        generateLightmapsTask->SetScene(activeScene);

        Handle<World> worldHandle = MakeStrongRef(subsystem->GetWorld());
        generateLightmapsTask->SetWorld(worldHandle);

        g_editorState->AddTask(generateLightmapsTask);
    }
};

DEFINE_EDITOR_COMMAND(AddFogVolume);

#pragma endregion AddFogVolume

template <class T>
static constexpr bool ShouldAddNodeAsChild()
{
    // InstancedMeshProxy should always be a child of the actively selected entity.
    return std::is_same_v<T, InstancedMeshProxy>;
}

template <class EditorCommandType, class T>
static void AddNodeOfTypeImpl(EditorSubsystem* subsystem, Name defaultNodeName)
{
    const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();
    if (!currentProject.IsValid())
    {
        HYP_LOG(Editor, Error, "No project loaded; cannot add entity");

        return;
    }

    Handle<Scene> activeScene = subsystem->GetActiveScene();
    if (!activeScene.IsValid())
    {
        HYP_LOG(Editor, Error, "No active scene found");

        return;
    }

    WeakHandle<Node> currentFocusedNode = subsystem->GetFocusedNode();

    Handle<T> n = MakeHandle<T>();
    n->SetName(defaultNodeName);
    InitObject(n);

    // Calculate appropriate insertion point in front of camera
    const Vec3f insertionPoint = subsystem->CalculateSceneInsertionPoint(5.0f, 0.5f);
    n->SetWorldTranslation(insertionPoint);

    Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
        HYP_FORMAT("Add {}", defaultNodeName),
        Proc<EditorActionFunctions()>([n, currentFocusedNode, activeScene]() -> EditorActionFunctions
            {
                return EditorActionFunctions {
                    .execute = Proc<void(EditorSubsystem*, EditorProject*)>([n, currentFocusedNode, activeScene](EditorSubsystem* editorSubsystem, EditorProject* project)
                        {
                            if constexpr (ShouldAddNodeAsChild<T>())
                            {
                                Handle<Node> parentNode = currentFocusedNode.Lock();

                                if (!parentNode.IsValid())
                                {
                                    parentNode = activeScene->GetRoot();
                                }

                                parentNode->AddChild(n);
                            }
                            else
                            {
                                activeScene->GetRoot()->AddChild(n);
                            }

                            editorSubsystem->SetFocusedNode(n, true);
                        }),
                    .revert = Proc<void(EditorSubsystem*, EditorProject*)>([n, currentFocusedNode](EditorSubsystem* editorSubsystem, EditorProject* project)
                        {
                            n->Remove();

                            if (editorSubsystem->GetFocusedNode() == n)
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

    currentProject->GetActionStack()->PushAction(action);
}

template <class Derived>
class HYP_API EditorCommandAddNodeBase : public EditorCommandBase
{
public:
    virtual ~EditorCommandAddNodeBase() override = default;

    virtual String GetText() const override
    {
        return HYP_FORMAT("Add {}", Derived::s_defaultNodeName);
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AddNodeOfTypeImpl<Derived, typename Derived::NodeType>(subsystem, Derived::s_defaultNodeName);
    }
};

#pragma region EditorCommandAddEntity

class HYP_API EditorCommandAddEntity final : public EditorCommandAddNodeBase<EditorCommandAddEntity>
{
    HYP_OBJECT_BODY(EditorCommandAddEntity);

public:
    using NodeType = Entity;
    static inline const Name s_defaultNodeName = NAME("New Entity");
};

DEFINE_EDITOR_COMMAND(AddEntity);

#pragma endregion EditorCommandAddEntity

#pragma region EditorCommandAddEmptyNode

class HYP_API EditorCommandAddEmptyNode final : public EditorCommandAddNodeBase<EditorCommandAddEmptyNode>
{
    HYP_OBJECT_BODY(EditorCommandAddEmptyNode);

public:
    using NodeType = Node;
    static inline const Name s_defaultNodeName = NAME("New Node");
};

DEFINE_EDITOR_COMMAND(AddEmptyNode);

#pragma endregion EditorCommandAddEmptyNode

#pragma region EditorCommandAddInstance

class HYP_API EditorCommandAddInstance final : public EditorCommandAddNodeBase<EditorCommandAddInstance>
{
    HYP_OBJECT_BODY(EditorCommandAddInstance);

public:
    using NodeType = InstancedMeshProxy;
    static inline const Name s_defaultNodeName = NAME("New Instance");
};

DEFINE_EDITOR_COMMAND(AddInstance);

#pragma region EditorCommandAddCamera

class HYP_API EditorCommandAddCamera final : public EditorCommandAddNodeBase<EditorCommandAddCamera>
{
    HYP_OBJECT_BODY(EditorCommandAddCamera);

public:
    using NodeType = Camera;
    static inline const Name s_defaultNodeName = NAME("New Camera");
};

DEFINE_EDITOR_COMMAND(AddCamera);

#pragma endregion EditorCommandAddCamera

#pragma region EditorCommandAddPointLight

class HYP_API EditorCommandAddPointLight final : public EditorCommandAddNodeBase<EditorCommandAddPointLight>
{
    HYP_OBJECT_BODY(EditorCommandAddPointLight);

public:
    using NodeType = PointLight;
    static inline const Name s_defaultNodeName = NAME("New Point Light");
};

DEFINE_EDITOR_COMMAND(AddPointLight);

#pragma endregion EditorCommandAddPointLight

#pragma region EditorCommandAddDirectionalLight

class HYP_API EditorCommandAddDirectionalLight final : public EditorCommandAddNodeBase<EditorCommandAddDirectionalLight>
{
    HYP_OBJECT_BODY(EditorCommandAddDirectionalLight);

public:
    using NodeType = DirectionalLight;
    static inline const Name s_defaultNodeName = NAME("New Directional Light");
};

DEFINE_EDITOR_COMMAND(AddDirectionalLight);

#pragma endregion EditorCommandAddDirectionalLight

#pragma region EditorCommandAddSpotLight

class HYP_API EditorCommandAddSpotLight final : public EditorCommandAddNodeBase<EditorCommandAddSpotLight>
{
    HYP_OBJECT_BODY(EditorCommandAddSpotLight);

public:
    using NodeType = SpotLight;
    static inline const Name s_defaultNodeName = NAME("New Spot Light");
};

DEFINE_EDITOR_COMMAND(AddSpotLight);

#pragma endregion EditorCommandAddSpotLight

#pragma region EditorCommandAddAreaRectLight

class HYP_API EditorCommandAddAreaRectLight final : public EditorCommandAddNodeBase<EditorCommandAddAreaRectLight>
{
    HYP_OBJECT_BODY(EditorCommandAddAreaRectLight);

public:
    using NodeType = AreaRectLight;
    static inline const Name s_defaultNodeName = NAME("New Rectangular Area Light");
};

DEFINE_EDITOR_COMMAND(AddAreaRectLight);

#pragma endregion EditorCommandAddAreaRectLight

#pragma region EditorCommandImportContent

class HYP_API EditorCommandImportContent final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandImportContent);

public:
    virtual ~EditorCommandImportContent() override = default;

    virtual String GetText() const override
    {
        return m_text.Length() ? m_text : EditorCommandBase::GetText();
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        ShowOpenFileDialog(
            "Select the file(s) to import into the project",
            GetDataDirectory(),
            { "obj", "fbx", "jpg", "jpeg", "png", "tga", "bmp", "ogre.xml" },
            /* allowMultiple */ true, /* allowDirectories */ false,
            [this](TResult<Array<FilePath>>&& result)
            {
                if (result.HasError())
                {
                    HYP_LOG(Editor, Error, "Failed to select files to import: {}", result.GetError().GetMessage());

                    return;
                }

                if (result.GetValue().Size() > 1)
                {
                    m_text = HYP_FORMAT("Import {} files", result.GetValue().Size());
                }
                else if (result.GetValue().Size() == 1)
                {
                    m_text = HYP_FORMAT("Import '{}'", result.GetValue()[0].Basename());
                }
                else
                {
                    HYP_LOG(Editor, Warning, "No files selected for import.");

                    return;
                }

                EditorTaskScope* editorTaskScope = new EditorTaskScope(
                    TickableEditorTask::StaticClass(),
                    []() { /* no tick function */ },
                    "Importing content...",
                    "Content is being imported in the background.",
                    /* isForegroundTask */ true);

                // Create identifier based on the common folder of the assets
                String identifier = "Unknown";

                if (result.GetValue().Any())
                {
                    identifier = result.GetValue()[0].BasePath().Basename();
                }

                // Queue up an asset batch
                AssetBatch* batch = AssetManager::GetInstance()->CreateBatch(identifier);

                for (const FilePath& file : result.GetValue())
                {
                    batch->Add(file.Basename(), FilePath::Relative(file, CoreApi::GetExecutablePath()));
                }

                batch->OnComplete
                    .Bind([editorTaskScope](AssetMap& results)
                        {
                            HYP_LOG(Editor, Verbose, "{} assets loaded.", results.Size());

                            for (auto& it : results)
                            {
                                String& key = it.first;
                                LoadedAsset& loadedAsset = it.second;

                                editorTaskScope->GetEditorTask()->SetDescription("Processing " + key);
                                
                                Handle<AssetObject> assetObject = loadedAsset.ExtractAs<AssetObject>();
                                if (!assetObject.IsValid())
                                {
                                    continue;
                                }

                                ANSIStringView importSubPath;

                                if (assetObject->IsA(Texture::StaticClass()))
                                {
                                    importSubPath = "Media/Textures";
                                }
                                else if (assetObject->IsA(Node::StaticClass()))
                                {
                                    importSubPath = "Media/Models";
                                }
                                else if (assetObject->IsA(Skeleton::StaticClass()))
                                {
                                    importSubPath = "Media/Skeletons";
                                }
                                else
                                {
                                    importSubPath = "Media/Misc";
                                }

                                Result registerAssetResult = AssetManager::GetInstance()->GetAssetRegistry()->RegisterAsset(
                                    HYP_FORMAT("$Import/{}", importSubPath),
                                    assetObject,
                                    AddAssetConflictMode::GenerateNewName);

                                if (registerAssetResult.HasError())
                                {
                                    HYP_LOG(Editor, Error, "Failed to import asset {}: {}", assetObject->GetName(), registerAssetResult.GetError().GetMessage());
                                }
                            }

                            delete editorTaskScope;
                        })
                    .Detach();

                batch->LoadAsync();

                // Note: The batch will be destroyed automatically by AssetManager when complete
            });
    }

private:
    String m_text;
};

DEFINE_EDITOR_COMMAND(ImportContent);

#pragma endregion EditorCommandImportContent

#pragma region EditorCommandReparentNode

class HYP_API EditorCommandReparentNode final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandReparentNode);

public:
    EditorCommandReparentNode() = default;

    virtual ~EditorCommandReparentNode() override = default;

    virtual String GetText() const override
    {
        return m_text.Length() ? m_text : EditorCommandBase::GetText();
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        Handle<Node> node = subsystem->GetActiveScene()->FindNodeByName(StringHash(GetArgument(0)));
        Handle<Node> newParent = subsystem->GetActiveScene()->FindNodeByName(StringHash(GetArgument(1)));

        if (!node.IsValid() || !newParent.IsValid())
        {
            HYP_LOG(Editor, Error, "EditorCommandReparentNode: invalid node or new parent");
            return;
        }

        m_text = HYP_FORMAT("Attach '{}' to '{}'", node->GetName(), newParent->GetName());

        // Prevent cycles: reject if newParent is the dragged node itself or any of its descendants.
        if (newParent->IsOrHasParent(node))
        {
            HYP_LOG(Editor, Warning, "EditorCommandReparentNode: cannot reparent a node to its own descendant");
            return;
        }

        Node* previousParent = node->GetParent();
        if (!previousParent)
        {
            HYP_LOG(Editor, Error, "EditorCommandReparentNode: node has no parent, cannot reparent");
            return;
        }

        if (previousParent == newParent)
            return;

        const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();
        if (!currentProject.IsValid())
        {
            HYP_LOG(Editor, Error, "EditorCommandReparentNode: no project loaded");
            return;
        }

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            GetText(),
            Proc<EditorActionFunctions()>([node, newParent, previousParent = MakeStrongRef(previousParent)]() -> EditorActionFunctions
                {
                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem*, EditorProject*)>([node, newParent](EditorSubsystem*, EditorProject*)
                            {
                                node->Remove();
                                newParent->AddChild(node);
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>([node, previousParent](EditorSubsystem*, EditorProject*)
                            {
                                node->Remove();
                                previousParent->AddChild(node);
                            })
                    };
                }));

        InitObject(action);

        currentProject->GetActionStack()->PushAction(action);
    }

private:
    String m_text;
};

DEFINE_EDITOR_COMMAND(ReparentNode);

#pragma endregion EditorCommandReparentNode

#pragma region ShowTexture

class HYP_API EditorCommandShowTexture final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandShowTexture);

public:
    virtual ~EditorCommandShowTexture() override = default;

    struct WatchTextureState
    {
        Handle<TextureOverlay> overlay;

        DelegateHandler onAssetRemoved;
        DelegateHandler onAssetAdded;
        AssetPath path;

        explicit operator bool () const
        {
            return path.IsValid();
        }
    };

    static WatchTextureState s_watchTextureState;
    static Mutex s_watchTextureStateMtx;

    virtual String GetText() const override
    {
        return "Show Texture";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        UISubsystem* uiSubsystem = subsystem->GetWorld()->GetSubsystem<UISubsystem>();
        if (!uiSubsystem)
        {
            HYP_LOG(Editor, Error, "ShowTexture: No UISubsystem available");
            return;
        }

        { // remove existing overlay and stop watching if active
            Mutex::Guard guard(s_watchTextureStateMtx);
            if (s_watchTextureState)
            {
                if (IsOnThread(g_simThread))
                {
                    if (s_watchTextureState.overlay.IsValid())
                    {
                        uiSubsystem->RemoveDebugOverlay(s_watchTextureState.overlay);
                    }
                }
                else
                {
                    GetThreadById(g_simThread)->GetScheduler().Enqueue([uiSubsystem = MakeStrongRef(uiSubsystem), overlay = s_watchTextureState.overlay]()
                        {
                            if (overlay.IsValid())
                            {
                                uiSubsystem->RemoveDebugOverlay(overlay);
                            }
                        },
                        TaskEnqueueFlags::FIRE_AND_FORGET);
                }

                s_watchTextureState = {};
            }
        }

        // just hide the overlay
        if (NumArguments() == 0 || GetArgument(0).Empty())
        {
            return;
        }

        AssetPath path = AssetPath(GetArgument(0));

        static const auto SetupWatch = [](UISubsystem* uiSubsystem, const AssetPath& path)
        {
            Handle<AssetObject> assetObject = g_assetManager->GetAssetRegistry()->GetAssetFromPath(path.ToString());
            Handle<Texture> texture = ObjCast<Texture>(assetObject);

            if (!texture.IsValid())
            {
                HYP_LOG(Console, Error, "ShowTexture: texture '{}' not found", path.ToString());
                return;
            }

            Handle<AssetPackage> package = assetObject->GetPackage();
            if (!package.IsValid())
            {
                HYP_LOG(Console, Error, "ShowTexture: texture '{}' has no package", path.ToString());
                return;
            }

            Handle<TextureOverlay> overlay = MakeHandle<TextureOverlay>(texture);
            InitObject(overlay);

            Mutex::Guard guard(s_watchTextureStateMtx);
            s_watchTextureState.overlay = overlay;
            s_watchTextureState.path = path;

            s_watchTextureState.onAssetRemoved = package->OnAssetObjectRemoved.Bind(
                [uiSubsystem = MakeStrongRef(uiSubsystem), overlay = s_watchTextureState.overlay](Name assetName, bool isDirect, AssetPackage*)
                {
                    Mutex::Guard guard(s_watchTextureStateMtx);
                    if (!isDirect || assetName != s_watchTextureState.path.GetName())
                        return;

                    if (overlay.IsValid())
                    {
                        // immediately set texture to null so we dont hold an unregistered texture.
                        overlay->SetTexture(Handle<Texture>::Null());
                    }
                });

            s_watchTextureState.onAssetAdded = package->OnAssetObjectAdded.Bind(
                [uiSubsystem = MakeStrongRef(uiSubsystem)](const AssetDesc& addedAsset, bool isDirect, AssetPackage* parentPackage)
                {
                    Mutex::Guard guard(s_watchTextureStateMtx);
                    if (!isDirect || addedAsset.name != s_watchTextureState.path.GetName())
                        return;

                    Handle<AssetObject> assetObject = parentPackage->GetAssetObject(addedAsset.name);
                    if (!assetObject.IsValid())
                        return;

                    Handle<Texture> texture = ObjCast<Texture>(assetObject);
                    if (!texture.IsValid())
                        return;

                    Handle<TextureOverlay> newOverlay = MakeHandle<TextureOverlay>(texture);
                    InitObject(newOverlay);

                    GetThreadById(g_simThread)->GetScheduler().Enqueue([uiSubsystem, newOverlay, oldOverlay = s_watchTextureState.overlay]() mutable
                        {
                            if (oldOverlay.IsValid())
                            {
                                uiSubsystem->RemoveDebugOverlay(oldOverlay);
                                oldOverlay.Reset();
                            }

                            uiSubsystem->AddDebugOverlay(newOverlay);
                        }, TaskEnqueueFlags::FIRE_AND_FORGET);

                    s_watchTextureState.overlay = newOverlay;
                });
            
            GetThreadById(g_simThread)->GetScheduler().Enqueue([uiSubsystem, overlay]()
                {
                    uiSubsystem->AddDebugOverlay(overlay);
                }, TaskEnqueueFlags::FIRE_AND_FORGET);
        };

        SetupWatch(uiSubsystem, path);
    }
};

// static definition
EditorCommandShowTexture::WatchTextureState EditorCommandShowTexture::s_watchTextureState;
Mutex EditorCommandShowTexture::s_watchTextureStateMtx;

DEFINE_EDITOR_COMMAND(ShowTexture);

#pragma endregion ShowTexture

#undef DEFINE_EDITOR_COMMAND

} // namespace Hyperion