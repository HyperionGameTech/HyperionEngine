#include <Editor/EditorCommand.hpp>
#include <Editor/EditorSubsystem.hpp>
#include <Editor/EditorProject.hpp>
#include <Editor/EditorState.hpp>
#include <Editor/EditorViewport.hpp>

#include <Scene/Scene.hpp>
#include <Scene/World.hpp>
#include <Scene/ProbeVolume.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/Light.hpp>
#include <Scene/EnvProbe.hpp>
#include <Scene/LightmapVolume.hpp>
#include <Scene/InstancedMeshProxy.hpp>
#include <Scene/ParticleVolume.hpp>
#include <Scene/FogVolume.hpp>
#include <Scene/Sprite.hpp>
#include <Scene/TextSprite.hpp>

#include <Scene/Camera/Camera.hpp>

#include <Scene/Components/BoundingBoxComponent.hpp>
#include <Scene/Components/LightmapElementComponent.hpp>
#include <Scene/Components/MeshComponent.hpp>

#include <Scripting/Asset/ScriptAsset.hpp>

#include <Core/Reflection/ClassUtils.hpp>

#include <Core/Utilities/StringUtil.hpp>

#include <Core/Logging/Logger.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetBatch.hpp>
#include <Asset/AssetRegistry.hpp>

#include <Rendering/Texture.hpp>
#include <Rendering/Mesh.hpp>

#include <Rendering/Util/MeshBuilder.hpp>

#include <Scene/Animation/Skeleton.hpp>

#include <Framework/EngineGlobals.hpp>

#include <System/OpenFileDialog.hpp>
#include <System/SaveFileDialog.hpp>
#include <System/SelectFolderDialog.hpp>

#include <UI/UISubsystem.hpp>
#include <UI/Overlays/Overlay.hpp>

namespace Hyperion {

EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);
ENGINE_API HYP_DECLARE_LOG_CHANNEL(Console);

extern Handle<EditorState> g_editorState;

namespace CoreApi {
CORE_API extern FilePath GetExecutablePath();
} // namespace CoreApi

ENGINE_API extern const FilePath& GetProjectsDirectory();
ENGINE_API extern const FilePath& GetDataDirectory();

#define DEFINE_EDITOR_COMMAND(name)                                        \
    const Class* g_clsEditorCommand##name = nullptr;                       \
                                                                           \
    const Class* EditorCommand##name ::StaticClass()                       \
    {                                                                      \
        return g_clsEditorCommand##name;                                   \
    }                                                                      \
                                                                           \
    HYP_BEGIN_CLASS(EditorCommand##name, -1, 0, NAME("EditorCommandBase")) \
    HYP_END_CLASS                                                          \
                                                                           \
    static TClassStaticInit<EditorCommand##name> g_classInit##EditorCommand##name {};

#pragma region Undo

class EditorCommandUndo final : public EditorCommandBase
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

class EditorCommandRedo final : public EditorCommandBase
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

class EditorCommandNewProject final : public EditorCommandBase
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

class EditorCommandOpenProject final : public EditorCommandBase
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

class EditorCommandSaveProject final : public EditorCommandBase
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

class EditorCommandSaveProjectAs final : public EditorCommandBase
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

            String projectName = *project->GetName();

            ShowSelectFolderDialog(
                "Select project folder",
                dir,
                [weakSubsystem = MakeWeakRef(subsystem), projectName = std::move(projectName)](TResult<FilePath>&& result) mutable
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

                    if (selectedPath.EndsWith(projectName))
                    {
                        // IF the path we receive ends with the project name (ie. Projects/Project1) we want to chop off that part,
                        // otherwise we'd end up saving at Projects/Project1/Project1.
                        selectedPath = selectedPath.BasePath();
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

class EditorCommandAddLightmapVolume final : public EditorCommandBase
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

class EditorCommandAddReflectionProbe final : public EditorCommandBase
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

        Handle<ReflectionProbe> reflectionProbe = MakeHandle<ReflectionProbe>(BoundingBox(Vec3f(-10.0f), Vec3f(10.0f)), Vec2u(128, 128));
        reflectionProbe->SetIsBaked(true);
        InitObject(reflectionProbe);

        WeakHandle<Node> previousFocusedNode = subsystem->GetFocusedNode();

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            GetText(),
            Proc<EditorActionFunctions()>(
                [reflectionProbe, previousFocusedNode, activeScene]() -> EditorActionFunctions
                {
                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [reflectionProbe, activeScene](EditorSubsystem* editorSubsystem, EditorProject* project)
                            {
                                activeScene->GetRoot()->AddChild(reflectionProbe);

                                editorSubsystem->SetFocusedNode(reflectionProbe, true);
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [reflectionProbe, previousFocusedNode](EditorSubsystem* editorSubsystem, EditorProject* project)
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

        if (reflectionProbe->IsBaked())
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

#pragma region AddIrradianceProbe

class EditorCommandAddIrradianceProbe final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandAddIrradianceProbe);

public:
    virtual ~EditorCommandAddIrradianceProbe() override = default;

    virtual String GetText() const override
    {
        return "Add Irradiance Probe";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();
        if (!currentProject.IsValid())
        {
            HYP_LOG(Editor, Error, "No project loaded; cannot add irradiance probe!");

            return;
        }

        Handle<Scene> activeScene = subsystem->GetActiveScene();
        if (!activeScene.IsValid())
        {
            HYP_LOG(Editor, Error, "No active scene; cannot add irradiance probe!");

            return;
        }

        Handle<IrradianceProbe> irradianceProbe = MakeHandle<IrradianceProbe>(BoundingBox(Vec3f(-10.0f), Vec3f(10.0f)), Vec2u(64, 64));
        InitObject(irradianceProbe);

        WeakHandle<Node> previousFocusedNode = subsystem->GetFocusedNode();

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            GetText(),
            Proc<EditorActionFunctions()>(
                [irradianceProbe, previousFocusedNode, activeScene]() -> EditorActionFunctions
                {
                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [irradianceProbe, activeScene](EditorSubsystem* editorSubsystem, EditorProject* project)
                            {
                                activeScene->GetRoot()->AddChild(irradianceProbe);

                                editorSubsystem->SetFocusedNode(irradianceProbe, true);
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [irradianceProbe, previousFocusedNode](EditorSubsystem* editorSubsystem, EditorProject* project)
                            {
                                irradianceProbe->Remove();

                                if (editorSubsystem->GetFocusedNode() == irradianceProbe)
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

DEFINE_EDITOR_COMMAND(AddIrradianceProbe);

#pragma endregion AddIrradianceProbe

#pragma region AddProbeVolume

class EditorCommandAddProbeVolume final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandAddProbeVolume);

public:
    virtual ~EditorCommandAddProbeVolume() override = default;

    virtual String GetText() const override
    {
        return "Add Probe Volume";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();
        if (!currentProject.IsValid())
        {
            HYP_LOG(Editor, Error, "No project loaded; cannot add irradiance probe volume!");

            return;
        }

        const BoundingBox probeGridBounds = BoundingBox(Vec3f(-50.0f, -15.0f, -50.0f), Vec3f(50.0f, 30.0f, 50.0f));

        // Ensure surrounding entities have a LightmapElementComponent.
        static const auto initOverlappingEntities = [](World* world, const BoundingBox& probeGridBounds)
        {
            for (Scene* scene : world->GetScenes())
            {
                TSet<Entity*> entitiesToAddComponent;

                for (auto&& [entity, meshComponent] : scene->GetEntityManager()->GetEntitySet<MeshComponent>().GetScopedView(DataAccessFlags::ACCESS_RW))
                {
                    const BoundingBox entityWorldBounds = entity->GetWorldBounds();

                    if (entityWorldBounds.Overlaps(probeGridBounds))
                    {
                        entitiesToAddComponent.Add(entity);
                    }
                }

                for (Entity* entity : entitiesToAddComponent)
                {
                    if (!entity->HasComponent<LightmapElementComponent>())
                    {
                        LightmapElementComponent component;
                        entity->AddComponent<LightmapElementComponent>(component);
                    }

                    entity->AddTag<EntityTag::UpdateSphericalHarmonicsData>();
                }
            }
        };

        Handle<ProbeVolume> volume = MakeHandle<ProbeVolume>(probeGridBounds);
        volume->SetGridSize(Vec3u(4, 4, 4));
        volume->CreateProbes();
        InitObject(volume);

        WeakHandle<Node> previousFocusedNode = subsystem->GetFocusedNode();

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            GetText(),
            Proc<EditorActionFunctions()>(
                [volume, previousFocusedNode]() -> EditorActionFunctions
                {
                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [volume](EditorSubsystem* editorSubsystem, EditorProject* project)
                            {
                                Handle<Scene> activeScene = editorSubsystem->GetActiveScene();
                                if (!activeScene.IsValid())
                                {
                                    HYP_LOG(Editor, Error, "No active scene; cannot add irradiance probe volume!");

                                    return;
                                }

                                activeScene->GetRoot()->AddChild(volume);

                                editorSubsystem->SetFocusedNode(volume, true);

                                initOverlappingEntities(activeScene->GetWorld(), volume->GetWorldBounds());
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [volume, previousFocusedNode](EditorSubsystem* editorSubsystem, EditorProject* project)
                            {
                                volume->Remove();

                                if (editorSubsystem->GetFocusedNode() == volume)
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

DEFINE_EDITOR_COMMAND(AddProbeVolume);

#pragma endregion AddProbeVolume

#pragma region AddParticleVolume

class EditorCommandAddParticleVolume final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandAddParticleVolume);

public:
    virtual ~EditorCommandAddParticleVolume() override = default;

    virtual String GetText() const override
    {
        return "Add Particle Volume";
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

        Handle<ParticleVolume> particleVolume = MakeHandle<ParticleVolume>(BoundingBox(Vec3f(-20.0f, 0.0f, -20.0f), Vec3f(20.0f, 20.0f, 20.0f)));

        particleVolume->texture = g_assetManager->Load<Texture>("Textures/spark.png").GetValue().ExtractAs<Handle<Texture>>();
        particleVolume->mesh = MeshBuilder::Quad();
        particleVolume->origin = Vec3f(0.0f, 10.0f, 0.0f); // temp
        particleVolume->maxParticles = 2048;
        particleVolume->hasPhysics = true;
        particleVolume->lifespan = 3.5f;
        particleVolume->startSize = 0.05f;
        particleVolume->randomness = 0.8f;

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

class EditorCommandAddFogVolume final : public EditorCommandBase
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
        Proc<EditorActionFunctions()>(
            [n, currentFocusedNode, activeScene]() -> EditorActionFunctions
            {
                return EditorActionFunctions {
                    .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                        [n, currentFocusedNode, activeScene](EditorSubsystem* editorSubsystem, EditorProject* project)
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

                            editorSubsystem->ClearSelection();
                            editorSubsystem->AddToSelection(n);
                            editorSubsystem->SetFocusedNode(n, true);
                        }),
                    .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                        [n, currentFocusedNode](EditorSubsystem* editorSubsystem, EditorProject* project)
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
class EditorCommandAddNodeBase : public EditorCommandBase
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

class EditorCommandAddEntity final : public EditorCommandAddNodeBase<EditorCommandAddEntity>
{
    HYP_OBJECT_BODY(EditorCommandAddEntity);

public:
    using NodeType = Entity;
    static inline const Name s_defaultNodeName = NAME("New Entity");
};

DEFINE_EDITOR_COMMAND(AddEntity);

#pragma endregion EditorCommandAddEntity

#pragma region EditorCommandAddEmptyNode

class EditorCommandAddEmptyNode final : public EditorCommandAddNodeBase<EditorCommandAddEmptyNode>
{
    HYP_OBJECT_BODY(EditorCommandAddEmptyNode);

public:
    using NodeType = Node;
    static inline const Name s_defaultNodeName = NAME("New Node");
};

DEFINE_EDITOR_COMMAND(AddEmptyNode);

#pragma endregion EditorCommandAddEmptyNode

#pragma region EditorCommandAddInstance

class EditorCommandAddInstance final : public EditorCommandAddNodeBase<EditorCommandAddInstance>
{
    HYP_OBJECT_BODY(EditorCommandAddInstance);

public:
    using NodeType = InstancedMeshProxy;
    static inline const Name s_defaultNodeName = NAME("New Instance");
};

DEFINE_EDITOR_COMMAND(AddInstance);

#pragma region EditorCommandAddCamera

class EditorCommandAddCamera final : public EditorCommandAddNodeBase<EditorCommandAddCamera>
{
    HYP_OBJECT_BODY(EditorCommandAddCamera);

public:
    using NodeType = Camera;
    static inline const Name s_defaultNodeName = NAME("New Camera");
};

DEFINE_EDITOR_COMMAND(AddCamera);

#pragma endregion EditorCommandAddCamera

#pragma region EditorCommandAddSprite

class EditorCommandAddSprite final : public EditorCommandAddNodeBase<EditorCommandAddSprite>
{
    HYP_OBJECT_BODY(EditorCommandAddSprite);

public:
    using NodeType = Sprite;
    static inline const Name s_defaultNodeName = NAME("New Sprite");
};

DEFINE_EDITOR_COMMAND(AddSprite);

#pragma endregion EditorCommandAddSprite

#pragma region EditorCommandAddTextSprite

class EditorCommandAddTextSprite final : public EditorCommandAddNodeBase<EditorCommandAddTextSprite>
{
    HYP_OBJECT_BODY(EditorCommandAddTextSprite);

public:
    using NodeType = TextSprite;
    static inline const Name s_defaultNodeName = NAME("New Text Sprite");
};

DEFINE_EDITOR_COMMAND(AddTextSprite);

#pragma endregion EditorCommandAddTextSprite

#pragma region EditorCommandAddPointLight

class EDITOR_API EditorCommandAddPointLight final : public EditorCommandAddNodeBase<EditorCommandAddPointLight>
{
    HYP_OBJECT_BODY(EditorCommandAddPointLight);

public:
    using NodeType = PointLight;
    static inline const Name s_defaultNodeName = NAME("New Point Light");
};

DEFINE_EDITOR_COMMAND(AddPointLight);

#pragma endregion EditorCommandAddPointLight

#pragma region EditorCommandAddDirectionalLight

class EditorCommandAddDirectionalLight final : public EditorCommandAddNodeBase<EditorCommandAddDirectionalLight>
{
    HYP_OBJECT_BODY(EditorCommandAddDirectionalLight);

public:
    using NodeType = DirectionalLight;
    static inline const Name s_defaultNodeName = NAME("New Directional Light");
};

DEFINE_EDITOR_COMMAND(AddDirectionalLight);

#pragma endregion EditorCommandAddDirectionalLight

#pragma region EditorCommandAddSpotLight

class EditorCommandAddSpotLight final : public EditorCommandAddNodeBase<EditorCommandAddSpotLight>
{
    HYP_OBJECT_BODY(EditorCommandAddSpotLight);

public:
    using NodeType = SpotLight;
    static inline const Name s_defaultNodeName = NAME("New Spot Light");
};

DEFINE_EDITOR_COMMAND(AddSpotLight);

#pragma endregion EditorCommandAddSpotLight

#pragma region EditorCommandAddAreaRectLight

class EditorCommandAddAreaRectLight final : public EditorCommandAddNodeBase<EditorCommandAddAreaRectLight>
{
    HYP_OBJECT_BODY(EditorCommandAddAreaRectLight);

public:
    using NodeType = AreaRectLight;
    static inline const Name s_defaultNodeName = NAME("New Rectangular Area Light");
};

DEFINE_EDITOR_COMMAND(AddAreaRectLight);

#pragma endregion EditorCommandAddAreaRectLight

#pragma region EditorCommandImportContent

class EditorCommandImportContent final : public EditorCommandBase
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
                    []()
                    { /* no tick function */ },
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

                                  GetCurrentAssetRegistry()->PutAssetUnique(assetObject);
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

class EditorCommandReparentNode final : public EditorCommandBase
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

#pragma region DeleteNode

class EditorCommandDeleteNode final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandDeleteNode);

public:
    virtual ~EditorCommandDeleteNode() override = default;

    virtual String GetText() const override
    {
        return "Delete Nodes";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();
        if (!currentProject.IsValid())
        {
            HYP_LOG(Editor, Error, "EditorCommandDeleteNode: no project loaded");
            return;
        }

        Array<Handle<Node>> nodesToDelete;

        if (NumArguments() >= 1 && !GetArgument(0).Empty())
        {
            // Named node deletion (e.g., from context menu)
            Handle<Node> node = subsystem->GetActiveScene()->FindNodeByName(StringHash(GetArgument(0)));

            if (!node.IsValid())
            {
                HYP_LOG(Editor, Warning, "EditorCommandDeleteNode: could not find node '{}'", GetArgument(0));
                return;
            }

            nodesToDelete.PushBack(node);
        }
        else
        {
            // Delete all selected nodes; fall back to focused node if nothing selected
            nodesToDelete = subsystem->GetSelectedNodes();

            if (nodesToDelete.Empty())
            {
                Handle<Node> focusedNode = subsystem->GetFocusedNode();

                if (focusedNode.IsValid())
                {
                    nodesToDelete.PushBack(focusedNode);
                }
            }
        }

        // Filter: skip nodes whose parent is also in the deletion set (they'll be removed implicitly)
        Array<Handle<Node>> topLevelNodes;
        for (const Handle<Node>& node : nodesToDelete)
        {
            if (!node.IsValid())
            {
                continue;
            }

            bool hasAncestorInSet = false;
            for (Node* p = node->GetParent(); p; p = p->GetParent())
            {
                for (const Handle<Node>& other : nodesToDelete)
                {
                    if (other.Get() == p)
                    {
                        hasAncestorInSet = true;
                        break;
                    }
                }
                if (hasAncestorInSet)
                {
                    break;
                }
            }

            if (!hasAncestorInSet)
            {
                topLevelNodes.PushBack(node);
            }
        }

        if (topLevelNodes.Empty())
        {
            HYP_LOG(Editor, Warning, "EditorCommandDeleteNode: no nodes to delete (all are children of other selected nodes)");
            return;
        }

        // Build undo data: each node paired with its parent
        Array<Pair<Handle<Node>, WeakHandle<Node>>> nodesWithParents;
        for (const Handle<Node>& node : topLevelNodes)
        {
            Node* parentRaw = node->GetParent();
            if (!parentRaw)
            {
                HYP_LOG(Editor, Warning, "EditorCommandDeleteNode: node has no parent (cannot delete root)");
                continue;
            }

            nodesWithParents.PushBack({ node, MakeWeakRef(parentRaw) });
        }

        if (nodesWithParents.Empty())
        {
            return;
        }

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            nodesWithParents.Size() == 1
                ? HYP_FORMAT("Delete {}", nodesWithParents[0].first->GetName())
                : HYP_FORMAT("Delete {} nodes", nodesWithParents.Size()),
            Proc<EditorActionFunctions()>([nodesWithParents]() -> EditorActionFunctions
                                          {
                                              return EditorActionFunctions {
                                                  .execute = Proc<void(EditorSubsystem*, EditorProject*)>([nodesWithParents](EditorSubsystem* editorSubsystem, EditorProject* project)
                                                                                                          {
                                                                                                              for (const auto& pair : nodesWithParents)
                                                                                                              {
                                                                                                                  pair.first->Remove();
                                                                                                              }

                                                                                                              // Clear focus if it was one of the deleted nodes
                                                                                                              if (Handle<Node> focusedNode = editorSubsystem->GetFocusedNode(); !focusedNode.IsValid())
                                                                                                              {
                                                                                                                  // Focus was auto-cleared; nothing to restore
                                                                                                              }
                                                                                                          }),
                                                  .revert = Proc<void(EditorSubsystem*, EditorProject*)>([nodesWithParents](EditorSubsystem* editorSubsystem, EditorProject* project)
                                                                                                         {
                                                                                                             // Re-attach in reverse order so original order is preserved
                                                                                                             for (int i = nodesWithParents.Size() - 1; i >= 0; --i)
                                                                                                             {
                                                                                                                 const auto& pair = nodesWithParents[i];

                                                                                                                 Handle<Node> parent = pair.second.Lock();
                                                                                                                 if (!parent.IsValid())
                                                                                                                 {
                                                                                                                     continue;
                                                                                                                 }

                                                                                                                 parent->AddChild(pair.first);
                                                                                                             }

                                                                                                             // Focus the first restored node
                                                                                                             if (nodesWithParents.Any())
                                                                                                             {
                                                                                                                 editorSubsystem->SetFocusedNode(nodesWithParents[0].first, true);
                                                                                                             }
                                                                                                         })
                                              };
                                          }));

        InitObject(action);

        currentProject->GetActionStack()->PushAction(action);
    }
};

DEFINE_EDITOR_COMMAND(DeleteNode);

#pragma endregion DeleteNode

#pragma region TeleportTo

class EditorCommandTeleportTo final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandTeleportTo);

public:
    virtual ~EditorCommandTeleportTo() override = default;

    virtual String GetText() const override
    {
        return "Teleport To";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        Handle<Node> node;

        if (NumArguments() >= 1 && !GetArgument(0).Empty())
        {
            node = subsystem->GetActiveScene()->FindNodeByName(StringHash(GetArgument(0)));

            if (!node.IsValid())
            {
                HYP_LOG(Editor, Warning, "EditorCommandTeleportTo: could not find node '{}'", GetArgument(0));
                return;
            }
        }
        else
        {
            node = subsystem->GetFocusedNode();
        }

        if (!node.IsValid())
        {
            HYP_LOG(Editor, Warning, "EditorCommandTeleportTo: no node specified or focused");
            return;
        }

        EditorViewport* activeViewport = subsystem->GetActiveViewport();
        if (!activeViewport)
        {
            HYP_LOG(Editor, Warning, "EditorCommandTeleportTo: no active viewport");
            return;
        }

        const Handle<Camera>& camera = activeViewport->GetCamera();
        if (!camera.IsValid())
        {
            HYP_LOG(Editor, Warning, "EditorCommandTeleportTo: no camera in active viewport");
            return;
        }

        camera->SetWorldTranslation(node->GetWorldTranslation());
    }
};

DEFINE_EDITOR_COMMAND(TeleportTo);

#pragma endregion TeleportTo

#pragma region Copy

class EditorCommandCopy final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandCopy);

public:
    virtual ~EditorCommandCopy() override = default;

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        Array<Handle<Node>> nodes;

        if (NumArguments() >= 1 && !GetArgument(0).Empty())
        {
            Handle<Node> node = subsystem->GetActiveScene()->FindNodeByName(StringHash(GetArgument(0)));

            if (node.IsValid())
            {
                nodes.PushBack(node);
            }
            else
            {
                HYP_LOG(Editor, Warning, "could not find node '{}'", GetArgument(0));
                return;
            }
        }
        else
        {
            // Copy all selected nodes; fall back to the focused node if nothing is selected
            nodes = subsystem->GetSelectedNodes();

            if (nodes.Empty())
            {
                Handle<Node> focusedNode = subsystem->GetFocusedNode();

                if (focusedNode.IsValid())
                {
                    nodes.PushBack(focusedNode);
                }
            }
        }

        if (nodes.Empty())
        {
            HYP_LOG(Editor, Warning, "No nodes to copy");
            return;
        }

        g_editorState->SetClipboardNodes(nodes);

        HYP_LOG(Editor, Verbose, "Copied {} node(s) to clipboard", nodes.Size());
    }
};

DEFINE_EDITOR_COMMAND(Copy);

#pragma endregion Copy

#pragma region Paste

class EditorCommandPaste final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandPaste);

public:
    virtual ~EditorCommandPaste() override = default;

    virtual String GetText() const override
    {
        return "Paste Nodes";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        EditorProject* currentProject = subsystem->GetCurrentProject();

        if (!currentProject)
        {
            HYP_LOG(Editor, Warning, "No current project");

            return;
        }

        Array<Handle<Node>> clipboardNodes = g_editorState->GetClipboardNodes();

        if (clipboardNodes.Empty())
        {
            HYP_LOG(Editor, Warning, "No nodes in clipboard");

            return;
        }

        Handle<Scene> activeScene = subsystem->GetActiveScene();

        if (!activeScene.IsValid())
        {
            HYP_LOG(Editor, Error, "No active scene");

            return;
        }

        WeakHandle<Node> previousFocusedNode = subsystem->GetFocusedNode();
        const Handle<Node>& sceneRoot = activeScene->GetRoot();

        // Clone each clipboard node, pairing with its individual parent
        Array<Pair<Handle<Node>, WeakHandle<Node>>> newNodesWithParents;
        for (const Handle<Node>& clipboardNode : clipboardNodes)
        {
            if (!clipboardNode.IsValid())
            {
                continue;
            }

            Handle<Node> newNode = clipboardNode->Clone();

            if (!newNode.IsValid())
            {
                HYP_LOG(Editor, Error, "Failed to clone clipboard node '{}'", clipboardNode->GetName());

                continue;
            }

            String newName = clipboardNode->GetName().ToString() + "Copy";
            newNode->SetName(CreateNameFromDynamicString(newName));

            // Attach to the original node's parent, falling back to the scene root
            WeakHandle<Node> parentNode = MakeWeakRef(clipboardNode->GetParent());
            if (!parentNode.IsValid())
            {
                parentNode = MakeWeakRef(sceneRoot);
            }

            newNodesWithParents.PushBack({ newNode, parentNode });
        }

        if (newNodesWithParents.Empty())
        {
            HYP_LOG(Editor, Error, "Failed to create any pasted nodes");

            return;
        }

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            (newNodesWithParents.Size() == 1)
                ? HYP_FORMAT("Paste {}", newNodesWithParents[0].first->GetName())
                : HYP_FORMAT("Paste {} nodes", newNodesWithParents.Size()),
            Proc<EditorActionFunctions()>([newNodesWithParents, previousFocusedNode, activeScene]() -> EditorActionFunctions
                                          {
                                              return EditorActionFunctions {
                                                  .execute = Proc<void(EditorSubsystem*, EditorProject*)>([newNodesWithParents, activeScene](EditorSubsystem* editorSubsystem, EditorProject* project)
                                                                                                          {
                                                                                                              const Handle<Node>& sceneRoot = activeScene->GetRoot();

                                                                                                              Array<Handle<Node>> addedNodes;

                                                                                                              for (const auto& pair : newNodesWithParents)
                                                                                                              {
                                                                                                                  const Handle<Node>& newNode = pair.first;
                                                                                                                  WeakHandle<Node> parentWeak = pair.second;

                                                                                                                  Handle<Node> parentStrong = parentWeak.Lock();
                                                                                                                  if (!parentStrong.IsValid())
                                                                                                                  {
                                                                                                                      parentStrong = MakeStrongRef(sceneRoot);
                                                                                                                  }

                                                                                                                  if (!parentStrong.IsValid())
                                                                                                                  {
                                                                                                                      HYP_LOG(Editor, Error, "Cannot paste node; no parent to attach to");

                                                                                                                      continue;
                                                                                                                  }

                                                                                                                  parentStrong->AddChild(newNode);
                                                                                                                  addedNodes.PushBack(newNode);
                                                                                                              }

                                                                                                              // Focus the first pasted node and select all pasted nodes
                                                                                                              if (addedNodes.Any())
                                                                                                              {
                                                                                                                  editorSubsystem->ClearSelection();

                                                                                                                  for (const Handle<Node>& node : addedNodes)
                                                                                                                  {
                                                                                                                      editorSubsystem->AddToSelection(node);
                                                                                                                  }

                                                                                                                  editorSubsystem->SetFocusedNode(addedNodes[0], true);
                                                                                                              }
                                                                                                          }),
                                                  .revert = Proc<void(EditorSubsystem*, EditorProject*)>([newNodesWithParents, previousFocusedNode](EditorSubsystem* editorSubsystem, EditorProject* project)
                                                                                                         {
                                                                                                             for (const auto& pair : newNodesWithParents)
                                                                                                             {
                                                                                                                 pair.first->Remove();
                                                                                                             }

                                                                                                             // Restore previous focused node if it was cleared
                                                                                                             if (editorSubsystem->GetFocusedNode() == Handle<Node>::Null())
                                                                                                             {
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

        HYP_LOG(Editor, Verbose, "Pasted {} node(s) to scene", newNodesWithParents.Size());
    }
};

DEFINE_EDITOR_COMMAND(Paste);

#pragma endregion Paste

#pragma region SelectAll

class EditorCommandSelectAll final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandSelectAll);

public:
    virtual ~EditorCommandSelectAll() override = default;

    virtual String GetText() const override
    {
        return "Select All";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        const Handle<Scene> activeScene = subsystem->GetActiveScene();
        if (!activeScene.IsValid())
        {
            HYP_LOG(Editor, Warning, "EditorCommandSelectAll: no active scene");
            return;
        }

        const Handle<Node>& root = activeScene->GetRoot();
        if (!root.IsValid())
        {
            HYP_LOG(Editor, Warning, "EditorCommandSelectAll: scene has no root node");
            return;
        }

        Array<Node*> descendants = root->GetDescendantsArray();

        subsystem->ClearSelection();
        subsystem->AddToSelection(root);

        for (Node* descendant : descendants)
        {
            subsystem->AddToSelection(MakeStrongRef(descendant));
        }
    }
};

DEFINE_EDITOR_COMMAND(SelectAll);

#pragma endregion SelectAll

#pragma region NewScript

class EditorCommandNewScript final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandNewScript);

public:
    virtual ~EditorCommandNewScript() override = default;

    virtual String GetText() const override
    {
        return "New Script";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();
        if (!currentProject.IsValid())
        {
            HYP_LOG(Editor, Error, "No project loaded; cannot create script asset!");

            return;
        }

        Handle<ScriptAsset> scriptAsset = MakeHandle<ScriptAsset>(Name::Unique("NewScript"), ScriptDesc());
        InitObject(scriptAsset);

        // scriptAsset->SetSourceCode(HYP_FORMAT("// {}\n\nexport func Update(DeltaTime : float)\nend\n", scriptAsset->GetName()));

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            GetText(),
            Proc<EditorActionFunctions()>(
                [scriptAsset]() -> EditorActionFunctions
                {
                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [scriptAsset](EditorSubsystem*, EditorProject*)
                            {
                                GetCurrentAssetRegistry()->PutAssetUnique(scriptAsset);
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [scriptAsset](EditorSubsystem*, EditorProject*)
                            {
                                GetCurrentAssetRegistry()->RemoveAsset(scriptAsset);
                            })
                    };
                }));

        InitObject(action);

        currentProject->GetActionStack()->PushAction(action);
    }
};

DEFINE_EDITOR_COMMAND(NewScript);

#pragma endregion NewScript

#pragma region AddAsset

class EditorCommandAddAsset final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandAddAsset);

public:
    virtual ~EditorCommandAddAsset() override = default;

    virtual String GetText() const override
    {
        return "Add Asset";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        if (NumArguments() < 2)
        {
            HYP_LOG(Editor, Warning, "EditorCommandAddAsset requires bucket index and asset name");
            return;
        }

        uint32 bucketIndex = 0;
        if (!StringUtil::Parse(GetArgument(0).Data(), &bucketIndex) || bucketIndex == AssetBuckets::None.GetIndex())
        {
            HYP_LOG(Editor, Warning, "EditorCommandAddAsset: invalid bucket index '{}'", GetArgument(0));
            return;
        }

        const String& assetName = GetArgument(1);

        const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();
        if (!currentProject.IsValid())
        {
            HYP_LOG(Editor, Error, "EditorCommandAddAsset: no project loaded");
            return;
        }

        Handle<Scene> activeScene = subsystem->GetActiveScene();
        if (!activeScene.IsValid())
        {
            HYP_LOG(Editor, Error, "EditorCommandAddAsset: no active scene");
            return;
        }

        Handle<AssetObject> asset = GetCurrentAssetRegistry()->GetAsset(*AssetBuckets::AllBuckets[bucketIndex], CreateNameFromDynamicString(assetName));
        if (!asset.IsValid())
        {
            HYP_LOG(Editor, Warning, "EditorCommandAddAsset: asset '{}' in bucket {} is not valid", assetName, GetAssetBucketName(bucketIndex));
            return;
        }

        Handle<Node> assetNode = DynamicCast<Node>(asset);
        if (!assetNode.IsValid())
        {
            HYP_LOG(Editor, Warning, "EditorCommandAddAsset: asset '{}' in bucket {} is not valid", assetName, GetAssetBucketName(bucketIndex));
            return;
        }

        Handle<Node> clonedNode = assetNode->Clone();
        if (!clonedNode.IsValid())
        {
            HYP_LOG(Editor, Error, "EditorCommandAddAsset: failed to clone asset '{}'", assetName);
            return;
        }

        Handle<Node> parentNode;
        if (Handle<Node> focusedNode = subsystem->GetFocusedNode(); focusedNode.IsValid())
        {
            parentNode = focusedNode;
        }
        else
        {
            parentNode = MakeStrongRef(activeScene->GetRoot());
        }

        if (!parentNode.IsValid())
        {
            HYP_LOG(Editor, Error, "EditorCommandAddAsset: no parent node to attach to");
            return;
        }

        Vec3f insertionPoint = subsystem->CalculateSceneInsertionPoint();

        // If viewport coordinates are provided, try raycasting from the camera
        if (NumArguments() >= 4)
        {
            float nx = 0.5f, ny = 0.5f;
            StringUtil::Parse(GetArgument(2), &nx);
            StringUtil::Parse(GetArgument(3), &ny);

            if (EditorViewport* activeViewport = subsystem->GetActiveViewport())
            {
                if (Camera* camera = activeViewport->GetCamera())
                {
                    const Vec4f worldPos = camera->TransformScreenToWorld(Vec2f(nx, ny));
                    const Vec3f rayDir = worldPos.GetXYZ().Normalize();
                    const Ray ray { camera->GetWorldTranslation(), rayDir };

                    if (activeScene->GetSceneFlags() & SceneFlags::HAS_OCTREE)
                    {
                        RayTestResults results;
                        if (activeScene->GetOctree().TestRay(ray, results, RayTestFlags::TestBVH))
                        {
                            insertionPoint = results.Front().hitpoint;
                        }
                    }
                }
            }
        }

        clonedNode->SetWorldTranslation(insertionPoint);

        WeakHandle<Node> previousFocusedNode = subsystem->GetFocusedNode();

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            HYP_FORMAT("Add {}", assetName),
            Proc<EditorActionFunctions()>([clonedNode, parentNode, previousFocusedNode]() -> EditorActionFunctions
                                          {
                                              return EditorActionFunctions {
                                                  .execute = Proc<void(EditorSubsystem*, EditorProject*)>([clonedNode, parentNode](EditorSubsystem* editorSubsystem, EditorProject*)
                                                                                                          {
                                                                                                              parentNode->AddChild(clonedNode);
                                                                                                              editorSubsystem->SetFocusedNode(clonedNode, true);
                                                                                                          }),
                                                  .revert = Proc<void(EditorSubsystem*, EditorProject*)>([clonedNode, previousFocusedNode](EditorSubsystem* editorSubsystem, EditorProject*)
                                                                                                         {
                                                                                                             clonedNode->Remove();

                                                                                                             if (Handle<Node> focusedNode = previousFocusedNode.Lock(); focusedNode.IsValid())
                                                                                                             {
                                                                                                                 editorSubsystem->SetFocusedNode(focusedNode, true);
                                                                                                             }
                                                                                                         })
                                              };
                                          }));

        InitObject(action);
        currentProject->GetActionStack()->PushAction(action);
    }
};

DEFINE_EDITOR_COMMAND(AddAsset);

#pragma endregion AddAsset

#undef DEFINE_EDITOR_COMMAND

} // namespace Hyperion
