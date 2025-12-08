#include <editor/EditorCommand.hpp>
#include <editor/EditorSubsystem.hpp>
#include <editor/EditorProject.hpp>

#include <lightmapper/LightmapVolume.hpp>

#include <scene/Scene.hpp>
#include <scene/World.hpp>
#include <scene/EntityManager.hpp>
#include <scene/Light.hpp>
#include <scene/EnvProbe.hpp>

#include <scene/ParticleVolume.hpp>
#include <scene/FogVolume.hpp>

#include <scene/components/BoundingBoxComponent.hpp>

#include <core/reflection/ClassUtils.hpp>

#include <core/logging/Logger.hpp>

#include <asset/Assets.hpp>

#include <engine/EngineGlobals.hpp>

#include <system/OpenFileDialog.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

HYP_API extern const FilePath& GetResourceDirectory();

#define DEFINE_EDITOR_COMMAND(name)                                        \
    HYP_API const Class* g_clsEditorCommand##name = nullptr;               \
                                                                           \
    HYP_BEGIN_CLASS(EditorCommand##name, -1, 0, NAME("EditorCommandBase")) \
    HYP_END_CLASS                                                          \
                                                                           \
    HYP_REGISTER_STATIC_CLASS(EditorCommand##name)

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

        ShowOpenFileDialog(
            "Select the project to open",
            GetResourceDirectory(),
            { "hypproj" },
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

                GetThreadById(g_gameThread)->GetScheduler().Enqueue([weakSubsystem = std::move(weakSubsystem), projectFilepath = std::move(result.GetValue()[0])]() mutable
                    {
                        Handle<EditorSubsystem> subsystem = weakSubsystem.Lock();
                        if (!subsystem)
                        {
                            HYP_LOG(Editor, Error, "Failed to lock EditorSubsystem from weak reference in ShowOpenProjectDialog");
                            return;
                        }

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
            // @TODO
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

        // @TODO: Allow building a bounding box in editor before starting the task.
        BoundingBox lightmapVolumeAabb(Vec3f(-60.0f, -5.0f, -60.0f), Vec3f(60.0f, 40.0f, 60.0f));

        Handle<LightmapVolume> lightmapVolume = CreateObject<LightmapVolume>(lightmapVolumeAabb);
        lightmapVolume->SetName(Name::Unique("LightmapVolume"));
        InitObject(lightmapVolume);

        lightmapVolume->AddComponent<BoundingBoxComponent>(BoundingBoxComponent {});

        WeakHandle<Node> previousFocusedNode = subsystem->GetFocusedNode();

        Handle<FunctionalEditorAction> action = CreateObject<FunctionalEditorAction>(
            StaticClass()->GetName(),
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
        Handle<GenerateLightmapsEditorTask> generateLightmapsTask = CreateObject<GenerateLightmapsEditorTask>(lightmapVolume);
        InitObject(generateLightmapsTask);

        generateLightmapsTask->SetScene(activeScene);

        Handle<World> worldHandle = MakeStrongRef(subsystem->GetWorld());
        generateLightmapsTask->SetWorld(worldHandle);

        subsystem->AddTask(generateLightmapsTask);
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

        Handle<ReflectionProbe> reflectionProbe = CreateObject<ReflectionProbe>(BoundingBox(Vec3f(-10.0f), Vec3f(10.0f)), Vec2u(128));
        reflectionProbe->SetIsBaked(true);
        InitObject(reflectionProbe);

        WeakHandle<Node> previousFocusedNode = subsystem->GetFocusedNode();

        Handle<FunctionalEditorAction> action = CreateObject<FunctionalEditorAction>(
            StaticClass()->GetName(),
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

        // kickoff task to generate reflection cubemap
        Handle<GenerateLightmapsEditorTask> generateLightmapsTask = CreateObject<GenerateLightmapsEditorTask>(reflectionProbe);
        InitObject(generateLightmapsTask);

        generateLightmapsTask->SetScene(activeScene);

        Handle<World> worldHandle = MakeStrongRef(subsystem->GetWorld());
        generateLightmapsTask->SetWorld(worldHandle);

        subsystem->AddTask(generateLightmapsTask);
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
        params.texture = g_assetManager->Load<Texture>("textures/spark.png").GetValue().ExtractAs<Handle<Texture>>();
        params.origin = Vec3f(0.0f, 10.0f, 0.0f); // temp
        params.maxParticles = 2048;
        params.hasPhysics = true;
        params.lifespan = 10.0f;

        Handle<ParticleVolume> particleVolume = CreateObject<ParticleVolume>(BoundingBox(Vec3f(-20.0f, 0.0f, -20.0f), Vec3f(20.0f, 20.0f, 20.0f)), params);
        InitObject(particleVolume);

        const Vec3f insertionPoint = subsystem->CalculateSceneInsertionPoint(5.0f, 0.5f);
        particleVolume->SetWorldTranslation(insertionPoint);

        WeakHandle<Node> previousFocusedNode = subsystem->GetFocusedNode();

        Handle<FunctionalEditorAction> action = CreateObject<FunctionalEditorAction>(
            StaticClass()->GetName(),
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

        Handle<FogVolume> fogVolume = CreateObject<FogVolume>(BoundingBox(Vec3f(-20.0f, 0.0f, -20.0f), Vec3f(20.0f, 30.0f, 20.0f)));
        InitObject(fogVolume);

        WeakHandle<Node> previousFocusedNode = subsystem->GetFocusedNode();

        Handle<FunctionalEditorAction> action = CreateObject<FunctionalEditorAction>(
            StaticClass()->GetName(),
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

        currentProject->GetActionStack()->Push(action);

        // start baking fog volume

        Handle<GenerateLightmapsEditorTask> generateLightmapsTask = CreateObject<GenerateLightmapsEditorTask>(
            Array<Handle<ObjectBase>> { fogVolume });

        InitObject(generateLightmapsTask);

        generateLightmapsTask->SetScene(activeScene);

        Handle<World> worldHandle = MakeStrongRef(subsystem->GetWorld());
        generateLightmapsTask->SetWorld(worldHandle);

        subsystem->AddTask(generateLightmapsTask);
    }
};

DEFINE_EDITOR_COMMAND(AddFogVolume);

#pragma endregion AddFogVolume

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

    Handle<T> n = CreateObject<T>();
    n->SetName(defaultNodeName);
    InitObject(n);

    // Calculate appropriate insertion point in front of camera
    const Vec3f insertionPoint = subsystem->CalculateSceneInsertionPoint(5.0f, 0.5f);
    n->SetWorldTranslation(insertionPoint);

    Handle<FunctionalEditorAction> action = CreateObject<FunctionalEditorAction>(
        EditorCommandType::StaticClass()->GetName(),
        Proc<EditorActionFunctions()>([n, currentFocusedNode, activeScene]() -> EditorActionFunctions
            {
                return EditorActionFunctions {
                    .execute = Proc<void(EditorSubsystem*, EditorProject*)>([n, activeScene](EditorSubsystem* editorSubsystem, EditorProject* project)
                        {
                            activeScene->GetRoot()->AddChild(n);
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

    currentProject->GetActionStack()->Push(action);
}

template <class Derived>
class HYP_API EditorCommandAddNodeBase : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandAddNodeBase);

public:
    virtual ~EditorCommandAddNodeBase() override = default;

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

#undef DEFINE_EDITOR_COMMAND

} // namespace hyperion