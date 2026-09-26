#include <Editor/Commands/EditorCommandsCommon.hpp>

namespace Hyperion {

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
        lightmapVolume->SetName(activeScene->GetUniqueNodeNameT<LightmapVolume>());

        WeakHandle<Node> previousFocusedNode = subsystem->GetFocusedNode();

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            GetText(),
            Proc<EditorActionFunctions()>(
                [lightmapVolume, previousFocusedNode, activeScene]() -> EditorActionFunctions
                {
                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [lightmapVolume, activeScene](EditorSubsystem* editorSubsystem, EditorProject* project)
                            {
                                activeScene->GetRoot()->AddChild(lightmapVolume);

                                project->GetActiveBakeLayer().Add<Baking::BakeLayerCategory::LightReceiver>(*lightmapVolume);
                                project->GetActiveBakeLayer().Add<Baking::BakeLayerCategory::Lightmap>(*lightmapVolume);

                                editorSubsystem->SetFocusedNode(lightmapVolume, true);
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [lightmapVolume, previousFocusedNode](EditorSubsystem* editorSubsystem, EditorProject* project)
                            {
                                project->GetActiveBakeLayer().Remove<Baking::BakeLayerCategory::LightReceiver>(*lightmapVolume);
                                project->GetActiveBakeLayer().Remove<Baking::BakeLayerCategory::Lightmap>(*lightmapVolume);

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

        // No kickoff since lightmap volume tasks are heavy and we don't want to put that on the user
    }
};

DEFINE_EDITOR_COMMAND(AddLightmapVolume);

#pragma endregion AddLightmapVolume

#pragma region BuildReflectionProbes

class EditorCommandBuildReflectionProbes final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandBuildReflectionProbes);

public:
    virtual ~EditorCommandBuildReflectionProbes() override = default;

    virtual String GetText() const override
    {
        return "Build Reflection Probes";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        Handle<Scene> activeScene = subsystem->GetActiveScene();
        if (!activeScene.IsValid())
        {
            HYP_LOG(Editor, Error, "No active scene; cannot build reflections!");

            return;
        }

        Handle<EditorProject> project = subsystem->GetCurrentProject();
        if (!project.IsValid())
        {
            HYP_LOG(Editor, Error, "No active project");

            return;
        }

        Array<Handle<ObjectBase>> reflectionProbes;

        if (Handle<Node> root = activeScene->GetRoot(); root.IsValid())
        {
            for (Node* node : root->GetDescendants())
            {
                if (node->IsA<ReflectionProbe>())
                {
                    ReflectionProbe* reflectionProbe = StaticCast<ReflectionProbe>(node);
                    if (reflectionProbe->IsRealtime())
                    {
                        continue;
                    }

                    reflectionProbes.PushBack(MakeStrongRef(node));
                }
            }
        }

        if (reflectionProbes.Empty())
        {
            HYP_LOG(Editor, Warning, "No Reflection Probes in the active scene. Cannot bake.");

            SystemMessageBox(MessageBoxType::WARNING)
                .Title("Cannot Bake Reflections")
                .Text("No Reflection Probes in the scene to bake lighting for. Add a Reflection Probe and try again.")
                .Button("Close", []() { })
                .Show();

            return;
        }

        Handle<GenerateLightmapsEditorTask> editorTask = MakeHandle<GenerateLightmapsEditorTask>(reflectionProbes);
        editorTask->SetIsForegroundTask(true);
        InitObject(editorTask);

        editorTask->SetScene(activeScene);

        Handle<World> worldHandle = subsystem->GetProjectWorld();
        editorTask->SetWorld(worldHandle);

        g_editorState->AddTask(editorTask);
    }
};

DEFINE_EDITOR_COMMAND(BuildReflectionProbes);

#pragma endregion BuildReflectionProbes


#pragma region BuildIrradianceProbes

class EditorCommandBuildIrradianceProbes final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandBuildIrradianceProbes);

public:
    virtual ~EditorCommandBuildIrradianceProbes() override = default;

    virtual String GetText() const override
    {
        return "Build Irradiance Probes";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        Handle<Scene> activeScene = subsystem->GetActiveScene();
        if (!activeScene.IsValid())
        {
            HYP_LOG(Editor, Error, "No active scene; cannot build Irradiance Probes!");

            return;
        }

        Handle<EditorProject> project = subsystem->GetCurrentProject();
        if (!project.IsValid())
        {
            HYP_LOG(Editor, Error, "No active project");

            return;
        }

        Array<Handle<ObjectBase>> irradianceProbes;

        if (Handle<Node> root = activeScene->GetRoot(); root.IsValid())
        {
            for (Node* node : root->GetDescendants())
            {
                if (node->IsA<IrradianceProbe>())
                {
                    IrradianceProbe* irradianceProbe = StaticCast<IrradianceProbe>(node);
                    if (irradianceProbe->IsRealtime())
                    {
                        continue;
                    }

                    irradianceProbes.PushBack(MakeStrongRef(node));
                }
            }
        }

        if (irradianceProbes.Empty())
        {
            HYP_LOG(Editor, Warning, "No Irradiance Probes in the active scene. Cannot bake.");

            SystemMessageBox(MessageBoxType::WARNING)
                .Title("Cannot Bake Reflections")
                .Text("No Irradiance Probes in the scene to bake lighting for. Add an Irradiance Probe and try again.")
                .Button("Close", []() { })
                .Show();

            return;
        }

        Handle<GenerateLightmapsEditorTask> editorTask = MakeHandle<GenerateLightmapsEditorTask>(irradianceProbes);
        editorTask->SetIsForegroundTask(true);
        InitObject(editorTask);

        editorTask->SetScene(activeScene);

        Handle<World> worldHandle = subsystem->GetProjectWorld();
        editorTask->SetWorld(worldHandle);

        g_editorState->AddTask(editorTask);
    }
};

DEFINE_EDITOR_COMMAND(BuildIrradianceProbes);

#pragma endregion BuildIrradianceProbes


#pragma region BuildLightmaps

class EditorCommandBuildLightmaps final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandBuildLightmaps);

public:
    virtual ~EditorCommandBuildLightmaps() override = default;

    virtual String GetText() const override
    {
        return "Build Lightmap Volumes";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        Handle<Scene> activeScene = subsystem->GetActiveScene();
        if (!activeScene.IsValid())
        {
            HYP_LOG(Editor, Error, "No active scene; cannot bake lightmaps!");

            return;
        }

        Handle<EditorProject> project = subsystem->GetCurrentProject();
        if (!project.IsValid())
        {
            HYP_LOG(Editor, Error, "No active project");

            return;
        }

        Array<Handle<ObjectBase>> sources;

        if (Handle<Node> root = activeScene->GetRoot(); root.IsValid())
        {
            for (Node* node : root->GetDescendants())
            {
                if (node->IsA<LightmapVolume>())
                {
                    sources.PushBack(MakeStrongRef(node));
                }
            }
        }

        if (sources.Empty())
        {
            HYP_LOG(Editor, Warning, "No Lightmap Volumes in the active scene. Cannot bake.");

            SystemMessageBox(MessageBoxType::WARNING)
                .Title("Cannot Bake Lighting")
                .Text("No Lightmap Volumes in the scene to bake lighting for. Add one and try again.")
                .Button("Close", []() { })
                .Show();

            return;
        }

        Handle<World> worldHandle = subsystem->GetProjectWorld();

        Handle<GenerateLightmapsEditorTask> editorTask = MakeHandle<GenerateLightmapsEditorTask>(sources);
        InitObject(editorTask);
        editorTask->SetIsForegroundTask(true);
        editorTask->SetScene(activeScene);
        editorTask->SetWorld(worldHandle);

        g_editorState->AddTask(editorTask);
    }
};

DEFINE_EDITOR_COMMAND(BuildLightmaps);

#pragma endregion BuildLightmaps

#pragma region BuildStaticShadows

class EditorCommandBuildStaticShadows final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandBuildStaticShadows);

public:
    virtual ~EditorCommandBuildStaticShadows() override = default;

    virtual String GetText() const override
    {
        return "Build Static Shadows";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        Handle<Scene> activeScene = subsystem->GetActiveScene();
        if (!activeScene.IsValid())
        {
            HYP_LOG(Editor, Error, "No active scene; cannot bake static shadows!");

            return;
        }

        Handle<EditorProject> project = subsystem->GetCurrentProject();
        if (!project.IsValid())
        {
            HYP_LOG(Editor, Error, "No active project");

            return;
        }

        Array<Handle<ObjectBase>> lights;

        if (Handle<Node> root = activeScene->GetRoot(); root.IsValid())
        {
            for (Node* node : root->GetDescendants())
            {
                if (!node->IsA<Light>())
                {
                    continue;
                }

                if (!node->IsStatic())
                {
                    continue;
                }

                Light* light = StaticCast<Light>(node);

                if (!light->CanBakeStaticShadows())
                {
                    continue;
                }

                lights.PushBack(MakeStrongRef(node));
            }
        }

        if (lights.Empty())
        {
            HYP_LOG(Editor, Warning, "No static Lights in the active scene. Cannot bake.");

            SystemMessageBox(MessageBoxType::WARNING)
                .Title("Cannot Bake Static Shadows")
                .Text("No Lights marked as Static in the scene to bake shadow maps for. Mark a Light as Static (Directional Lights do not support baked shadows) and try again.")
                .Button("Close", []() { })
                .Show();

            return;
        }

        Handle<GenerateLightmapsEditorTask> editorTask = MakeHandle<GenerateLightmapsEditorTask>(lights);
        editorTask->SetIsForegroundTask(true);
        InitObject(editorTask);

        editorTask->SetScene(activeScene);

        Handle<World> worldHandle = subsystem->GetProjectWorld();
        editorTask->SetWorld(worldHandle);

        g_editorState->AddTask(editorTask);
    }
};

DEFINE_EDITOR_COMMAND(BuildStaticShadows);

#pragma endregion BuildStaticShadows

#pragma region BuildFogVolumes

class EditorCommandBuildFogVolumes final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandBuildFogVolumes);

public:
    virtual ~EditorCommandBuildFogVolumes() override = default;

    virtual String GetText() const override
    {
        return "Build Fog Volumes";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        Handle<Scene> activeScene = subsystem->GetActiveScene();
        if (!activeScene.IsValid())
        {
            HYP_LOG(Editor, Error, "No active scene; cannot bake fog volumes!");

            return;
        }

        Handle<EditorProject> project = subsystem->GetCurrentProject();
        if (!project.IsValid())
        {
            HYP_LOG(Editor, Error, "No active project");

            return;
        }

        Array<Handle<ObjectBase>> fogVolumes;

        if (Handle<Node> root = activeScene->GetRoot(); root.IsValid())
        {
            for (Node* node : root->GetDescendants())
            {
                if (node->IsA<FogVolume>())
                {
                    fogVolumes.PushBack(MakeStrongRef(node));
                }
            }
        }

        if (fogVolumes.Empty())
        {
            HYP_LOG(Editor, Warning, "No Fog Volumes in the active scene. Cannot bake.");

            SystemMessageBox(MessageBoxType::WARNING)
                .Title("Cannot Bake Fog Volumes")
                .Text("No Fog Volumes in the scene to bake. Add a Fog Volume and try again.")
                .Button("Close", []() { })
                .Show();

            return;
        }

        Handle<GenerateLightmapsEditorTask> editorTask = MakeHandle<GenerateLightmapsEditorTask>(fogVolumes);
        editorTask->SetIsForegroundTask(true);
        InitObject(editorTask);

        editorTask->SetScene(activeScene);

        Handle<World> worldHandle = subsystem->GetProjectWorld();
        editorTask->SetWorld(worldHandle);

        g_editorState->AddTask(editorTask);
    }
};

DEFINE_EDITOR_COMMAND(BuildFogVolumes);

#pragma endregion BuildFogVolumes

#pragma region BuildBentNormals

class EditorCommandBuildBentNormals final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandBuildBentNormals);

public:
    virtual ~EditorCommandBuildBentNormals() override = default;

    virtual String GetText() const override
    {
        return "Build Bent Normals";
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
            HYP_LOG(Editor, Error, "No active scene; cannot build bent normals!");

            return;
        }

        Array<Handle<LightmapVolume>> lightmapVolumes;

        if (Handle<Node> root = activeScene->GetRoot(); root.IsValid())
        {
            for (Node* node : root->GetDescendants())
            {
                if (node->IsA<LightmapVolume>())
                {
                    lightmapVolumes.PushBack(MakeStrongRef(StaticCast<LightmapVolume>(node)));
                }
            }
        }

        if (lightmapVolumes.Empty())
        {
            HYP_LOG(Editor, Warning, "No LightmapVolumes in the active scene. Cannot bake bent normals.");

            SystemMessageBox(MessageBoxType::WARNING)
                .Title("Cannot Bake Bent Normals")
                .Text("Bent Normals cannot be computed as there are no LightmapVolumes in the scene. Add a LightmapVolume and try again.")
                .Button("Close", []() { })
                .Show();

            return;
        }

        Handle<GenerateBentNormalsEditorTask> generateBentNormalsTask = MakeHandle<GenerateBentNormalsEditorTask>(lightmapVolumes);
        InitObject(generateBentNormalsTask);

        generateBentNormalsTask->SetIsForegroundTask(true);
        generateBentNormalsTask->SetScene(activeScene);

        Handle<World> worldHandle = subsystem->GetProjectWorld();
        generateBentNormalsTask->SetWorld(worldHandle);

        g_editorState->AddTask(generateBentNormalsTask);
    }
};

DEFINE_EDITOR_COMMAND(BuildBentNormals);

#pragma endregion BuildBentNormals

#pragma region RebuildMeshBVHs

class EditorCommandRebuildMeshBVHs final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandRebuildMeshBVHs);

public:
    virtual ~EditorCommandRebuildMeshBVHs() override = default;

    virtual String GetText() const override
    {
        return "Rebuild Mesh BVHs";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        const Handle<EditorProject>& currentProject = subsystem->GetCurrentProject();
        if (!currentProject.IsValid())
        {
            HYP_LOG(Editor, Error, "No project loaded; cannot cook game content");

            return;
        }

        Handle<Scene> activeScene = subsystem->GetActiveScene();
        if (!activeScene.IsValid())
        {
            HYP_LOG(Editor, Error, "No active scene; cannot build bent normals!");

            return;
        }

        EditorTaskScope* editorTaskScope = new EditorTaskScope(
            TickableEditorTask::StaticClass(),
            []()
            { /* no tick function */ },
            "Rebuilding Mesh BVHs",
            "Collecting Meshes",
            /* isForegroundTask */ true);

        Array<Handle<Mesh>> meshes;

        for (auto [entity, meshComponent] : activeScene->GetEntityManager()->GetEntitySet<MeshComponent>().GetScopedView(DataAccessFlags::ACCESS_READ))
        {
            if (meshComponent.mesh.IsValid())
            {
                meshes.PushBack(meshComponent.mesh);
            }
        }

        TaskSystem::GetInstance().Enqueue(
            [editorTaskScope, project = currentProject, meshes = std::move(meshes)]()
            {
                for (size_t i = 0; i < meshes.Size(); i++)
                {
                    Mesh* mesh = meshes[i];

                    editorTaskScope->GetEditorTask()->SetDescription("Building BVH data: " + mesh->GetName().ToString());

                    auto readScope = mesh->GetReadScope();

                    BVHNode bvhNode;
                    mesh->BuildBVH(bvhNode);

                    readScope.Reset();

                    auto writeScope = mesh->GetWriteScope();
                    mesh->SetBVH(std::move(bvhNode));
                }

                // Clear EPC to pick up new BVHs.
                GetThreadById(g_simThread)->GetScheduler().Enqueue(
                    []()
                    {
                        g_editorState->GetPickCache().Clear();
                    }, TaskEnqueueFlags::FIRE_AND_FORGET);

                delete editorTaskScope;
            },
            TaskThreadPoolName::THREAD_POOL_BACKGROUND,
            TaskEnqueueFlags::FIRE_AND_FORGET);
    }
};

DEFINE_EDITOR_COMMAND(RebuildMeshBVHs);

#pragma endregion RebuildMeshBVHs


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

        const Vec3f insertionPoint = subsystem->CalculateSceneInsertionPoint(5.0f, 0.5f);

        Handle<ReflectionProbe> reflectionProbe = MakeHandle<ReflectionProbe>(BoundingBox(Vec3f(-10.0f), Vec3f(10.0f)), ReflectionProbe::DefaultDimensions);
        reflectionProbe->SetName(activeScene->GetUniqueNodeNameT<ReflectionProbe>());
        reflectionProbe->SetIsBaked(true);
        reflectionProbe->SetWorldTranslation(insertionPoint);

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

                                project->GetActiveBakeLayer().Add<Baking::BakeLayerCategory::LightReceiver>(*reflectionProbe);

                                editorSubsystem->SetFocusedNode(reflectionProbe, true);
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [reflectionProbe, previousFocusedNode](EditorSubsystem* editorSubsystem, EditorProject* project)
                            {
                                project->GetActiveBakeLayer().Remove<Baking::BakeLayerCategory::LightReceiver>(*reflectionProbe);

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
            Handle<GenerateLightmapsEditorTask> editorTask = MakeHandle<GenerateLightmapsEditorTask>(reflectionProbe);
            editorTask->SetIsForegroundTask(true);
            InitObject(editorTask);

            editorTask->SetScene(activeScene);

            Handle<World> worldHandle = subsystem->GetProjectWorld();
            editorTask->SetWorld(worldHandle);

            g_editorState->AddTask(editorTask);
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

        const Vec3f insertionPoint = subsystem->CalculateSceneInsertionPoint(5.0f, 0.5f);

        Handle<IrradianceProbe> irradianceProbe = MakeHandle<IrradianceProbe>(BoundingBox(Vec3f(-10.0f), Vec3f(10.0f)), IrradianceProbe::DefaultDimensions);
        irradianceProbe->SetName(activeScene->GetUniqueNodeNameT<IrradianceProbe>());
        irradianceProbe->SetWorldTranslation(insertionPoint);

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

                                project->GetActiveBakeLayer().Add<Baking::BakeLayerCategory::LightReceiver>(*irradianceProbe);

                                editorSubsystem->SetFocusedNode(irradianceProbe, true);
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [irradianceProbe, previousFocusedNode](EditorSubsystem* editorSubsystem, EditorProject* project)
                            {
                                project->GetActiveBakeLayer().Remove<Baking::BakeLayerCategory::LightReceiver>(*irradianceProbe);

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
        particleVolume->SetName(activeScene->GetUniqueNodeNameT<ParticleVolume>());

        //particleVolume->texture = g_assetManager->Load<Texture>("Textures/spark.png").GetValue().ExtractAs<Handle<Texture>>();
        particleVolume->mesh = MeshBuilder::Quad();
        particleVolume->origin = Vec3f(0.0f, 10.0f, 0.0f); // temp
        particleVolume->maxParticles = 2048;
        particleVolume->enableCollision = true;
        particleVolume->lifespan = 3.5f;
        particleVolume->startSize = 0.05f;
        particleVolume->randomness = 0.8f;

        InitObject(particleVolume);

        const Vec3f insertionPoint = subsystem->CalculateSceneInsertionPoint(5.0f, 0.5f);
        particleVolume->SetWorldTranslation(insertionPoint);

        WeakHandle<Node> previousFocusedNode = subsystem->GetFocusedNode();

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            GetText(),
            Proc<EditorActionFunctions()>(
                [particleVolume, previousFocusedNode, activeScene]() -> EditorActionFunctions
                {
                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [particleVolume, activeScene](EditorSubsystem* editorSubsystem, EditorProject* project)
                            {
                                activeScene->GetRoot()->AddChild(particleVolume);
                                editorSubsystem->SetFocusedNode(particleVolume, true);
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [particleVolume, previousFocusedNode](EditorSubsystem* editorSubsystem, EditorProject* project)
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
        fogVolume->SetName(activeScene->GetUniqueNodeNameT<FogVolume>());
        InitObject(fogVolume);

        WeakHandle<Node> previousFocusedNode = subsystem->GetFocusedNode();

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            GetText(),
            Proc<EditorActionFunctions()>(
                [fogVolume, previousFocusedNode, activeScene]() -> EditorActionFunctions
                {
                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [fogVolume, activeScene](EditorSubsystem* editorSubsystem, EditorProject* project)
                            {
                                activeScene->GetRoot()->AddChild(fogVolume);
                                editorSubsystem->SetFocusedNode(fogVolume, true);

                                project->GetActiveBakeLayer().Add<Baking::BakeLayerCategory::LightReceiver>(*fogVolume);
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [fogVolume, previousFocusedNode](EditorSubsystem* editorSubsystem, EditorProject* project)
                            {
                                project->GetActiveBakeLayer().Remove<Baking::BakeLayerCategory::LightReceiver>(*fogVolume);

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

        Handle<GenerateLightmapsEditorTask> editorTask = MakeHandle<GenerateLightmapsEditorTask>(Array<Handle<ObjectBase>> { fogVolume });
        editorTask->SetIsForegroundTask(true);
        InitObject(editorTask);

        editorTask->SetScene(activeScene);

        Handle<World> worldHandle = subsystem->GetProjectWorld();
        editorTask->SetWorld(worldHandle);

        g_editorState->AddTask(editorTask);
    }
};

DEFINE_EDITOR_COMMAND(AddFogVolume);

#pragma endregion AddFogVolume

} // namespace Hyperion
