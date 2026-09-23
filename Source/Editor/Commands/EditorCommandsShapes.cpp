#include <Editor/Commands/EditorCommandsCommon.hpp>

namespace Hyperion {

// Shapes

#pragma region AddPlane

class EditorCommandAddPlane final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandAddPlane);

public:
    virtual ~EditorCommandAddPlane() override = default;

    virtual String GetText() const override
    {
        return "Add Plane";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        Handle<EditorProject> currentProject = subsystem->GetCurrentProject();
        if (!currentProject.IsValid())
        {
            HYP_LOG(Editor, Error, "No project loaded; cannot add plane!");

            return;
        }

        const Vec3f insertionPoint = subsystem->CalculateSceneInsertionPoint(5.0f, 0.5f);

        Handle<Mesh> planeMesh = MeshBuilder::Quad();
        planeMesh->SetName(NAME("PlaneMesh"));

        MaterialAttributes attributes;
        attributes.shaderName = NAME("GeometryPass");

        Handle<Material> material = MakeHandle<Material>(NAME("PlaneMaterial"), attributes);

        Handle<Entity> entity = MakeHandle<Entity>();
        entity->SetName(NAME("PlaneEntity"));

        entity->SetWorldTranslation(insertionPoint);

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            GetText(),
            Proc<EditorActionFunctions()>(
                [planeMesh, entity, material]() -> EditorActionFunctions
                {
                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [&](EditorSubsystem* subsystem, EditorProject* project)
                            {
                                GetCurrentAssetRegistry()->PutAsset(planeMesh);

                                // Make an entity, assign MeshComponent w/ Mesh and a base Material

                                Handle<Scene> activeScene = subsystem->GetActiveScene();

                                if (activeScene.IsValid())
                                {
                                    activeScene->GetRoot()->AddChild(entity);

                                    GetCurrentAssetRegistry()->PutAsset(planeMesh);
                                    GetCurrentAssetRegistry()->PutAsset(material);

                                    // assign mesh component
                                    MeshComponent meshComponent;
                                    meshComponent.mesh = planeMesh;
                                    meshComponent.material = material;
                                    entity->AddComponent<MeshComponent>(meshComponent);

                                    entity->SetLocalBounds(planeMesh->GetAABB());
                                }
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [&](EditorSubsystem*, EditorProject* project)
                            {
                                GetCurrentAssetRegistry()->RemoveAsset(planeMesh);
                                GetCurrentAssetRegistry()->RemoveAsset(material);

                                entity->Remove();
                            })
                    };
                }));

        InitObject(action);

        currentProject->GetActionStack()->PushAction(action);
    }
};

DEFINE_EDITOR_COMMAND(AddPlane);

#pragma endregion AddPlane

#pragma region AddCube

class EditorCommandAddCube final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandAddCube);

public:
    virtual ~EditorCommandAddCube() override = default;

    virtual String GetText() const override
    {
        return "Add Cube";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        Handle<EditorProject> currentProject = subsystem->GetCurrentProject();
        if (!currentProject.IsValid())
        {
            HYP_LOG(Editor, Error, "No project loaded; cannot add cube!");

            return;
        }

        const Vec3f insertionPoint = subsystem->CalculateSceneInsertionPoint(5.0f, 0.5f);

        // Use mesh builder to create cube mesh

        Handle<Mesh> cubeMesh = MeshBuilder::Cube();
        cubeMesh->SetName(NAME("CubeMesh"));

        MaterialAttributes attributes;
        attributes.shaderName = NAME("GeometryPass");

        Handle<Material> material = MakeHandle<Material>(NAME("CubeMaterial"), attributes);

        Handle<Entity> entity = MakeHandle<Entity>();
        entity->SetName(NAME("CubeEntity"));

        entity->SetWorldTranslation(insertionPoint);

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            GetText(),
            Proc<EditorActionFunctions()>(
                [cubeMesh, entity, material]() -> EditorActionFunctions
                {
                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [&](EditorSubsystem* subsystem, EditorProject* project)
                            {
                                GetCurrentAssetRegistry()->PutAssetUnique(cubeMesh);
                                GetCurrentAssetRegistry()->PutAssetUnique(material);

                                // Make an entity, assign MeshComponent w/ Mesh and a base Material

                                Handle<Scene> activeScene = subsystem->GetActiveScene();

                                if (activeScene.IsValid())
                                {
                                    activeScene->GetRoot()->AddChild(entity);

                                    if (!entity->HasComponent<MeshComponent>())
                                    {
                                        // assign mesh component
                                        MeshComponent meshComponent;
                                        meshComponent.mesh = cubeMesh;
                                        meshComponent.material = material;
                                        entity->AddComponent<MeshComponent>(meshComponent);
                                    }
                                    else // has component
                                    {
                                        // This can happen if going undo->redo

                                        MeshComponent& meshComponent = entity->GetComponent<MeshComponent>();
                                        meshComponent.mesh = cubeMesh;
                                        meshComponent.material = material;
                                    }

                                    entity->SetLocalBounds(cubeMesh->GetAABB());
                                    entity->SetNeedsRenderProxyUpdate();
                                }
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [&](EditorSubsystem*, EditorProject* project)
                            {
                                GetCurrentAssetRegistry()->RemoveAsset(cubeMesh);
                                GetCurrentAssetRegistry()->RemoveAsset(material);

                                entity->Remove();
                            })
                    };
                }));

        InitObject(action);

        currentProject->GetActionStack()->PushAction(action);
    }
};

DEFINE_EDITOR_COMMAND(AddCube);

#pragma endregion AddCube

#pragma region AddCylinder

class EditorCommandAddCylinder final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandAddCylinder);

public:
    virtual ~EditorCommandAddCylinder() override = default;

    virtual String GetText() const override
    {
        return "Add Cylinder";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        Handle<EditorProject> currentProject = subsystem->GetCurrentProject();
        if (!currentProject.IsValid())
        {
            HYP_LOG(Editor, Error, "No project loaded; cannot add cylinder!");

            return;
        }

        const Vec3f insertionPoint = subsystem->CalculateSceneInsertionPoint(5.0f, 0.5f);

        // Use mesh builder to create cylinder mesh

        Handle<Mesh> cylinderMesh = MeshBuilder::Cylinder(0.5f, 1.0f, 32);
        cylinderMesh->SetName(NAME("CylinderMesh"));

        MaterialAttributes attributes;
        attributes.shaderName = NAME("GeometryPass");

        Handle<Material> material = MakeHandle<Material>(NAME("CylinderMaterial"), attributes);

        Handle<Entity> entity = MakeHandle<Entity>();
        entity->SetName(NAME("CylinderEntity"));

        entity->SetWorldTranslation(insertionPoint);

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            GetText(),
            Proc<EditorActionFunctions()>(
                [cylinderMesh, entity, material]() -> EditorActionFunctions
                {
                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [&](EditorSubsystem* subsystem, EditorProject* project)
                            {
                                GetCurrentAssetRegistry()->PutAssetUnique(cylinderMesh);
                                GetCurrentAssetRegistry()->PutAssetUnique(material);

                                // Make an entity, assign MeshComponent w/ Mesh and a base Material

                                Handle<Scene> activeScene = subsystem->GetActiveScene();

                                if (activeScene.IsValid())
                                {
                                    activeScene->GetRoot()->AddChild(entity);

                                    if (!entity->HasComponent<MeshComponent>())
                                    {
                                        // assign mesh component
                                        MeshComponent meshComponent;
                                        meshComponent.mesh = cylinderMesh;
                                        meshComponent.material = material;
                                        entity->AddComponent<MeshComponent>(meshComponent);
                                    }
                                    else // has component
                                    {
                                        // This can happen if going undo->redo

                                        MeshComponent& meshComponent = entity->GetComponent<MeshComponent>();
                                        meshComponent.mesh = cylinderMesh;
                                        meshComponent.material = material;
                                    }

                                    entity->SetLocalBounds(cylinderMesh->GetAABB());
                                    entity->SetNeedsRenderProxyUpdate();
                                }
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [&](EditorSubsystem*, EditorProject* project)
                            {
                                GetCurrentAssetRegistry()->RemoveAsset(cylinderMesh);
                                GetCurrentAssetRegistry()->RemoveAsset(material);

                                entity->Remove();
                            })
                    };
                }));

        InitObject(action);

        currentProject->GetActionStack()->PushAction(action);
    }
};

DEFINE_EDITOR_COMMAND(AddCylinder);

#pragma endregion AddCylinder

#pragma region AddWorldGridLayer

class EditorCommandAddWorldGridLayer final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandAddWorldGridLayer);

public:
    virtual ~EditorCommandAddWorldGridLayer() override = default;

    virtual String GetText() const override
    {
        return "Add World Grid Layer";
    }

    static Name CreateLayerName(const Class* cls)
    {
        ANSIString name = cls->GetName().LookupString();

        const ANSIStringView suffix = "WorldGridLayer";

        if (name.EndsWith(suffix))
        {
            name = name.Substr(0, name.Size() - suffix.Size());
        }

        if (name.Empty())
        {
            name = String(cls->GetName().LookupString());
        }

        return Name(name);
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        if (NumArguments() < 1)
        {
            HYP_LOG(Editor, Error, "EditorCommandAddWorldGridLayer: missing layer class name argument!");

            return;
        }

        const Class* layerClass = ClassRegistry::GetInstance().GetClass(Name(ANSIString(GetArgument(0))));

        if (!layerClass || !layerClass->IsDerivedFrom(WorldGridLayer::StaticClass()) || layerClass->IsAbstract())
        {
            HYP_LOG(Editor, Error, "EditorCommandAddWorldGridLayer: '{}' is not a valid WorldGridLayer class!", GetArgument(0));

            return;
        }

        Handle<EditorProject> currentProject = subsystem->GetCurrentProject();

        if (!currentProject.IsValid())
        {
            HYP_LOG(Editor, Error, "No project loaded; cannot add world grid layer!");

            return;
        }

        Handle<Scene> activeScene = subsystem->GetActiveScene();

        if (!activeScene.IsValid() || !activeScene->GetWorld())
        {
            HYP_LOG(Editor, Error, "No active scene/world; cannot add world grid layer!");

            return;
        }

        World* world = activeScene->GetWorld();

        Handle<WorldGrid> worldGrid = world->GetWorldGrid();

        if (!worldGrid.IsValid())
        {
            HYP_LOG(Editor, Error, "Active world has no WorldGrid (streaming disabled); cannot add world grid layer!");

            return;
        }

        BoxedValue instanceData;
        if (!layerClass->CreateInstance(instanceData, /* allowAbstract */ false))
        {
            HYP_LOG(Editor, Error, "Failed to create instance of world grid layer class '{}'!", GetArgument(0));

            return;
        }

        AssertDebug(instanceData.Is<Handle<WorldGridLayer>>());

        Handle<WorldGridLayer>& layer = instanceData.Get<Handle<WorldGridLayer>>();
        AssertDebug(layer != nullptr);

        layer->SetName(CreateLayerName(layerClass));

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            HYP_FORMAT("Add {} Layer", GetArgument(0)),
            Proc<EditorActionFunctions()>(
                [layer, worldGrid]() -> EditorActionFunctions
                {
                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [layer, worldGrid](EditorSubsystem*, EditorProject*)
                            {
                                worldGrid->AddLayer(layer);
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [layer, worldGrid](EditorSubsystem*, EditorProject*)
                            {
                                worldGrid->RemoveLayer(layer.Get());
                            })
                    };
                }));

        InitObject(action);

        currentProject->GetActionStack()->PushAction(action);
    }
};

DEFINE_EDITOR_COMMAND(AddWorldGridLayer);

#pragma endregion AddWorldGridLayer

#pragma region ToggleTerrainSculptMode

class EditorCommandToggleTerrainSculptMode final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandToggleTerrainSculptMode);

public:
    virtual ~EditorCommandToggleTerrainSculptMode() override = default;

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        if (IsOnThread(g_simThread))
        {
                    subsystem->GetTerrainState()->SetEnabled(!subsystem->GetTerrainState()->IsEnabled());
        }
        else
        {
            GetThreadById(g_simThread)->GetScheduler().Enqueue(
                [subsystem = MakeStrongRef(subsystem)]()
                {
            subsystem->GetTerrainState()->SetEnabled(!subsystem->GetTerrainState()->IsEnabled());
                },
                TaskEnqueueFlags::FIRE_AND_FORGET);
        }
    }
};

DEFINE_EDITOR_COMMAND(ToggleTerrainSculptMode);

#pragma endregion ToggleTerrainSculptMode

#pragma region SetTerrainSculptMode

class EditorCommandSetTerrainSculptMode final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandSetTerrainSculptMode);

public:
    virtual ~EditorCommandSetTerrainSculptMode() override = default;

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        TerrainSculptMode mode = TerrainSculptMode::Raise;

        if (GetArgument(0) == "lower")
        {
            mode = TerrainSculptMode::Lower;
        }
        else if (GetArgument(0) == "paint")
        {
            mode = TerrainSculptMode::PaintSplat;
        }

        subsystem->GetTerrainState()->SetEnabled(true);
        subsystem->GetTerrainState()->SetMode(mode);
    }
};

DEFINE_EDITOR_COMMAND(SetTerrainSculptMode);

#pragma endregion SetTerrainSculptMode

#pragma region SetTerrainSculptRadius

class EditorCommandSetTerrainSculptRadius final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandSetTerrainSculptRadius);

public:
    virtual ~EditorCommandSetTerrainSculptRadius() override = default;

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        float radius = 0.0f;

        if (!StringUtil::Parse(GetArgument(0), &radius))
        {
            HYP_LOG(Editor, Warning, "EditorCommandSetTerrainSculptRadius: invalid radius '{}'", GetArgument(0));

            return;
        }

        subsystem->GetTerrainState()->SetRadius(radius);
    }
};

DEFINE_EDITOR_COMMAND(SetTerrainSculptRadius);

#pragma endregion SetTerrainSculptRadius

#pragma region SetTerrainSculptStrength

class EditorCommandSetTerrainSculptStrength final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandSetTerrainSculptStrength);

public:
    virtual ~EditorCommandSetTerrainSculptStrength() override = default;

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        float strength = 0.0f;

        if (!StringUtil::Parse(GetArgument(0), &strength))
        {
            HYP_LOG(Editor, Warning, "EditorCommandSetTerrainSculptStrength: invalid strength '{}'", GetArgument(0));

            return;
        }

        subsystem->GetTerrainState()->SetStrength(strength);
    }
};

DEFINE_EDITOR_COMMAND(SetTerrainSculptStrength);

#pragma endregion SetTerrainSculptStrength

#pragma region SetTerrainSculptPaintLayer

class EditorCommandSetTerrainSculptPaintLayer final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandSetTerrainSculptPaintLayer);

public:
    virtual ~EditorCommandSetTerrainSculptPaintLayer() override = default;

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        int paintLayer = 0;

        if (!StringUtil::Parse(GetArgument(0), &paintLayer))
        {
            HYP_LOG(Editor, Warning, "EditorCommandSetTerrainSculptPaintLayer: invalid layer '{}'", GetArgument(0));

            return;
        }

        subsystem->GetTerrainState()->SetPaintLayer(paintLayer);
    }
};

DEFINE_EDITOR_COMMAND(SetTerrainSculptPaintLayer);

#pragma endregion SetTerrainSculptPaintLayer

#pragma region AddNormalizedCubeSphere

class EditorCommandAddNormalizedCubeSphere final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandAddNormalizedCubeSphere);

public:
    virtual ~EditorCommandAddNormalizedCubeSphere() override = default;

    virtual String GetText() const override
    {
        return "Add Normalized Cube Sphere";
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        Handle<EditorProject> currentProject = subsystem->GetCurrentProject();
        if (!currentProject.IsValid())
        {
            HYP_LOG(Editor, Error, "No project loaded; cannot add normalized cube sphere!");

            return;
        }

        uint32 numDivisions = 8;
        if (NumArguments() > 0)
        {
            StringUtil::Parse(GetArgument(0), &numDivisions);
        }
        numDivisions = MathUtil::Max(numDivisions, 1u);

        const Vec3f insertionPoint = subsystem->CalculateSceneInsertionPoint(5.0f, 0.5f);

        // Use mesh builder to create the normalized cube sphere mesh

        Handle<Mesh> cubeSphereMesh = MeshBuilder::NormalizedCubeSphere(numDivisions);
        cubeSphereMesh->SetName(NAME("NormalizedCubeSphereMesh"));

        MaterialAttributes attributes;
        attributes.shaderName = NAME("GeometryPass");

        Handle<Material> material = MakeHandle<Material>(NAME("NormalizedCubeSphereMaterial"), attributes);

        Handle<Entity> entity = MakeHandle<Entity>();
        entity->SetName(NAME("NormalizedCubeSphereEntity"));

        entity->SetWorldTranslation(insertionPoint);

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            GetText(),
            Proc<EditorActionFunctions()>(
                [cubeSphereMesh, entity, material]() -> EditorActionFunctions
                {
                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [&](EditorSubsystem* subsystem, EditorProject* project)
                            {
                                GetCurrentAssetRegistry()->PutAsset(cubeSphereMesh);

                                // Make an entity, assign MeshComponent w/ Mesh and a base Material

                                Handle<Scene> activeScene = subsystem->GetActiveScene();

                                if (activeScene.IsValid())
                                {
                                    activeScene->GetRoot()->AddChild(entity);

                                    GetCurrentAssetRegistry()->PutAsset(cubeSphereMesh);
                                    GetCurrentAssetRegistry()->PutAsset(material);

                                    // assign mesh component
                                    MeshComponent meshComponent;
                                    meshComponent.mesh = cubeSphereMesh;
                                    meshComponent.material = material;
                                    entity->AddComponent<MeshComponent>(meshComponent);

                                    entity->SetLocalBounds(cubeSphereMesh->GetAABB());
                                }
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [&](EditorSubsystem*, EditorProject* project)
                            {
                                GetCurrentAssetRegistry()->RemoveAsset(cubeSphereMesh);
                                GetCurrentAssetRegistry()->RemoveAsset(material);

                                entity->Remove();
                            })
                    };
                }));

        InitObject(action);

        currentProject->GetActionStack()->PushAction(action);
    }
};

DEFINE_EDITOR_COMMAND(AddNormalizedCubeSphere);

#pragma endregion AddNormalizedCubeSphere

#pragma region CopySwatchProperties

class EditorCommandCopySwatchProperties final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandCopySwatchProperties);

public:
    virtual ~EditorCommandCopySwatchProperties() override = default;

    virtual String GetText() const override
    {
        return m_text.Length() ? m_text : EditorCommandBase::GetText();
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        uint64 entityAddress = 0;

        if (!StringUtil::Parse(GetArgument(0), &entityAddress) || entityAddress == 0)
        {
            HYP_LOG(Editor, Error, "EditorCommandCopySwatchProperties: invalid entity address");

            return;
        }

        if (NumArguments() < 5)
        {
            HYP_LOG(Editor, Error,
                "EditorCommandCopySwatchProperties: expected <entity address> <source is base> <source swatch> <target is base> <target swatch>");

            return;
        }

        uint32 sourceIsBaseValue = 0;
        uint32 targetIsBaseValue = 0;
        StringUtil::Parse(GetArgument(1), &sourceIsBaseValue);
        StringUtil::Parse(GetArgument(3), &targetIsBaseValue);

        const bool sourceIsBase = sourceIsBaseValue != 0;
        const bool targetIsBase = targetIsBaseValue != 0;

        const Name sourceSwatch = sourceIsBase ? Name::Invalid() : Name(ANSIString(GetArgument(2)));
        const Name targetSwatch = targetIsBase ? Name::Invalid() : Name(ANSIString(GetArgument(4)));

        if (!sourceIsBase && !sourceSwatch.IsValid())
        {
            HYP_LOG(Editor, Error, "EditorCommandCopySwatchProperties: invalid source swatch");

            return;
        }

        if (!targetIsBase && !targetSwatch.IsValid())
        {
            HYP_LOG(Editor, Error, "EditorCommandCopySwatchProperties: invalid target swatch");

            return;
        }

        if (sourceSwatch.IsValid() && sourceSwatch == targetSwatch)
        {
            // Copying a swatch onto itself is a no-op
            return;
        }

        Handle<Entity> entity = MakeStrongRef(reinterpret_cast<Entity *>(entityAddress));

        if (!entity.IsValid())
        {
            HYP_LOG(Editor, Error, "EditorCommandCopySwatchProperties: invalid entity");

            return;
        }

        World *world = entity->GetWorld();

        if (!world)
        {
            HYP_LOG(Editor, Error, "EditorCommandCopySwatchProperties: entity is not part of a World");

            return;
        }

        SwatchOverrideSystem *swatchOverrideSystem = world->GetSystem<SwatchOverrideSystem>();

        if (!swatchOverrideSystem)
        {
            HYP_LOG(Editor, Error, "EditorCommandCopySwatchProperties: World has no SwatchOverrideSystem");

            return;
        }

        Array<SwatchPropertyCopyEntry> plan = swatchOverrideSystem->BuildSwatchPropertyCopyPlan(entity.Get(), sourceSwatch, targetSwatch);

        const char *sourceDisplay = sourceIsBase ? "Base" : sourceSwatch.LookupString();
        const char *targetDisplay = targetIsBase ? "Base" : targetSwatch.LookupString();

        if (plan.Empty())
        {
            HYP_LOG(Editor, Info, "Copy swatch properties {} -> {}: no differences", sourceDisplay, targetDisplay);

            return;
        }

        m_text = HYP_FORMAT("Copy Properties ({} -> {})", sourceDisplay, targetDisplay);

        HYP_LOG(Editor, Info, "Copy swatch properties {} -> {}: {} propert{} changed",
            sourceDisplay, targetDisplay, plan.Size(), plan.Size() == 1 ? "y" : "ies");

        const Handle<EditorProject> &currentProject = subsystem->GetCurrentProject();

        if (!currentProject.IsValid())
        {
            HYP_LOG(Editor, Warning, "EditorCommandCopySwatchProperties: no project loaded - applying without undo");

            swatchOverrideSystem->ApplySwatchPropertyCopyEntries(entity.Get(), targetSwatch, plan, true);

            return;
        }

        auto planPtr = MakeShared<Array<SwatchPropertyCopyEntry>>(std::move(plan));
        Handle<Entity> capturedEntity = entity;
        Name capturedTargetSwatch = targetSwatch;

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            GetText(),
            Proc<EditorActionFunctions()>(
                [capturedEntity, capturedTargetSwatch, planPtr]() -> EditorActionFunctions
                {
                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem *, EditorProject *)>(
                            [capturedEntity, capturedTargetSwatch, planPtr](EditorSubsystem *, EditorProject *)
                            {
                                if (SwatchOverrideSystem *system = ResolveSwatchOverrideSystem(capturedEntity))
                                {
                                    system->ApplySwatchPropertyCopyEntries(capturedEntity.Get(), capturedTargetSwatch, *planPtr, true);
                                }
                            }),
                        .revert = Proc<void(EditorSubsystem *, EditorProject *)>(
                            [capturedEntity, capturedTargetSwatch, planPtr](EditorSubsystem *, EditorProject *)
                            {
                                if (SwatchOverrideSystem *system = ResolveSwatchOverrideSystem(capturedEntity))
                                {
                                    system->ApplySwatchPropertyCopyEntries(capturedEntity.Get(), capturedTargetSwatch, *planPtr, false);
                                }
                            })
                    };
                }));

        InitObject(action);

        currentProject->GetActionStack()->PushAction(action);
    }

private:
    static SwatchOverrideSystem *ResolveSwatchOverrideSystem(const Handle<Entity> &entity)
    {
        if (!entity.IsValid())
        {
            return nullptr;
        }

        World *world = entity->GetWorld();

        if (!world)
        {
            return nullptr;
        }

        return world->GetSystem<SwatchOverrideSystem>();
    }

    String m_text;
};

DEFINE_EDITOR_COMMAND(CopySwatchProperties);

#pragma endregion CopySwatchProperties

#pragma region ResetSwatchOverrides

class EditorCommandResetSwatchOverrides final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandResetSwatchOverrides);

public:
    virtual ~EditorCommandResetSwatchOverrides() override = default;

    virtual String GetText() const override
    {
        return m_text.Length() ? m_text : EditorCommandBase::GetText();
    }

    virtual void Execute(EditorSubsystem* subsystem) override
    {
        AssertOnThread(g_simThread);

        uint64 entityAddress = 0;

        if (!StringUtil::Parse(GetArgument(0), &entityAddress) || entityAddress == 0)
        {
            HYP_LOG(Editor, Error, "EditorCommandResetSwatchOverrides: invalid entity address");

            return;
        }

        Handle<Entity> entity = MakeStrongRef(reinterpret_cast<Entity *>(entityAddress));

        if (!entity.IsValid())
        {
            HYP_LOG(Editor, Error, "EditorCommandResetSwatchOverrides: invalid entity");

            return;
        }

        World *world = entity->GetWorld();

        if (!world)
        {
            HYP_LOG(Editor, Error, "EditorCommandResetSwatchOverrides: entity is not part of a World");

            return;
        }

        SwatchOverrideSystem *swatchOverrideSystem = world->GetSystem<SwatchOverrideSystem>();

        if (!swatchOverrideSystem)
        {
            HYP_LOG(Editor, Error, "EditorCommandResetSwatchOverrides: World has no SwatchOverrideSystem");

            return;
        }

        const Name activeSwatch = world->GetActiveSwatchName();

        if (!activeSwatch.IsValid())
        {
            HYP_LOG(Editor, Error, "EditorCommandResetSwatchOverrides: World has no active swatch");

            return;
        }

        Array<Pair<Name, BoxedValue>> previousOverrides = swatchOverrideSystem->GetSwatchOverrideEntries(entity.Get(), activeSwatch);

        if (previousOverrides.Empty())
        {
            HYP_LOG(Editor, Info, "Reset swatch overrides: entity '{}' has no overrides for active swatch '{}'",
                entity->GetName(), activeSwatch.LookupString());

            return;
        }

        const bool wasApplied = swatchOverrideSystem->GetAppliedOverrideSwatch(entity.Get()) == activeSwatch;

        m_text = HYP_FORMAT("Reset Swatch Overrides ({})", activeSwatch.LookupString());

        HYP_LOG(Editor, Info, "Reset swatch overrides for active swatch '{}' on entity '{}': {} override(s) removed",
            activeSwatch.LookupString(), entity->GetName(), previousOverrides.Size());

        const Handle<EditorProject> &currentProject = subsystem->GetCurrentProject();

        if (!currentProject.IsValid())
        {
            HYP_LOG(Editor, Warning, "EditorCommandResetSwatchOverrides: no project loaded - applying without undo");

            swatchOverrideSystem->RemoveSwatchOverrideSet(entity.Get(), activeSwatch);

            return;
        }

        auto previousOverridesPtr = MakeShared<Array<Pair<Name, BoxedValue>>>(std::move(previousOverrides));
        Handle<Entity> capturedEntity = entity;
        Name capturedSwatch = activeSwatch;
        bool capturedWasApplied = wasApplied;

        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            GetText(),
            Proc<EditorActionFunctions()>(
                [capturedEntity, capturedSwatch, capturedWasApplied, previousOverridesPtr]() -> EditorActionFunctions
                {
                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem *, EditorProject *)>(
                            [capturedEntity, capturedSwatch](EditorSubsystem *, EditorProject *)
                            {
                                if (SwatchOverrideSystem *system = ResolveSwatchOverrideSystemFor(capturedEntity))
                                {
                                    // Reverts the applied overrides first, restoring base values
                                    system->RemoveSwatchOverrideSet(capturedEntity.Get(), capturedSwatch);
                                }
                            }),
                        .revert = Proc<void(EditorSubsystem *, EditorProject *)>(
                            [capturedEntity, capturedSwatch, capturedWasApplied, previousOverridesPtr](EditorSubsystem *, EditorProject *)
                            {
                                SwatchOverrideSystem *system = ResolveSwatchOverrideSystemFor(capturedEntity);

                                if (!system)
                                {
                                    return;
                                }

                                if (!system->HasSwatchOverrideSet(capturedEntity.Get(), capturedSwatch))
                                {
                                    system->AddSwatchOverrideSet(capturedEntity.Get(), capturedSwatch);
                                }

                                for (const Pair<Name, BoxedValue> &entry : *previousOverridesPtr)
                                {
                                    system->SetSwatchOverrideValue(capturedEntity.Get(), capturedSwatch, entry.first, entry.second);
                                }

                                // Re-apply when the reset swatch is (still) the active one
                                if (capturedWasApplied)
                                {
                                    World *entityWorld = capturedEntity->GetWorld();

                                    if (entityWorld && entityWorld->GetActiveSwatchName() == capturedSwatch)
                                    {
                                        system->ApplyOverrides(capturedEntity.Get(), capturedSwatch);
                                    }
                                }
                            })
                    };
                }));

        InitObject(action);

        currentProject->GetActionStack()->PushAction(action);
    }

private:
    static SwatchOverrideSystem *ResolveSwatchOverrideSystemFor(const Handle<Entity> &entity)
    {
        if (!entity.IsValid())
        {
            return nullptr;
        }

        World *world = entity->GetWorld();

        if (!world)
        {
            return nullptr;
        }

        return world->GetSystem<SwatchOverrideSystem>();
    }

    String m_text;
};

DEFINE_EDITOR_COMMAND(ResetSwatchOverrides);

#pragma endregion ResetSwatchOverrides

#pragma region SyncPhysicsShapeToLocalBounds

class EditorCommandSyncPhysicsShapeToLocalBounds final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandSyncPhysicsShapeToLocalBounds);

public:
    virtual ~EditorCommandSyncPhysicsShapeToLocalBounds() override = default;

    virtual void Execute(EditorSubsystem *subsystem) override
    {
        AssertOnThread(g_simThread);

        uint64 entityAddress = 0;

        if (!StringUtil::Parse(GetArgument(0), &entityAddress) || entityAddress == 0)
        {
            HYP_LOG(Editor, Error, "EditorCommandSyncPhysicsShapeToLocalBounds: invalid entity address");

            return;
        }

        Handle<Entity> entity = MakeStrongRef(reinterpret_cast<Entity *>(entityAddress));

        if (!entity.IsValid())
        {
            HYP_LOG(Editor, Error, "EditorCommandSyncPhysicsShapeToLocalBounds: invalid entity");

            return;
        }

        subsystem->SyncBoxPhysicsShapeToLocalBounds(entity.Get());
    }
};

DEFINE_EDITOR_COMMAND(SyncPhysicsShapeToLocalBounds);

#pragma endregion SyncPhysicsShapeToLocalBounds

} // namespace Hyperion
