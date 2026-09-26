/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <Editor/EditorSubsystem.hpp>
#include <Editor/EditorDelegates.hpp>
#include <Editor/EditorCamera.hpp>
#include <Editor/EditorTask.hpp>
#include <Editor/EditorProject.hpp>
#include <Editor/EditorActionStack.hpp>
#include <Editor/EditorAction.hpp>
#include <Editor/EditorState.hpp>
#include <Editor/EditorViewport.hpp>
#include <Editor/EditorCommand.hpp>
#include <Editor/EditorConfig.hpp>
#include <Editor/EditorAssetDrop.hpp>
#include <Editor/EditorPlayerSetup.hpp>
#include <Editor/EditorTemplateLibrary.hpp>

#include <Editor/Tasks/EditorTasks.hpp>

#include <Editor/Terrain/EditorTerrainState.hpp>
#include <Editor/Decal/EditorDecalPainterState.hpp>

#include <Scene/Systems/Editor/EditorSpriteSystem.hpp>

#include <DotNET/DotNETHost.hpp>

#include <Scene/Scene.hpp>
#include <Scene/World.hpp>
#include <Scene/Light/Light.hpp>
#include <Scene/EnvProbe.hpp>
#include <Scene/FogVolume.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/Prefab.hpp>
#include <Scene/Swatch.hpp>

#include <Scene/LOD.hpp>
#include <Scene/System.hpp>
#include <Scene/Systems/ScriptSystem.hpp>
#include <Scene/Systems/MeshSystem.hpp>
#include <Scene/Systems/SwatchOverrideSystem.hpp>

#include <Scene/WorldGrid/WorldGrid.hpp>
#include <Scene/WorldGrid/WorldGridLayer.hpp>

#include <Scene/Sky/DynamicSkySystem.hpp>

#include <Scene/Camera/Camera.hpp>

#include <Scene/Util/SceneHelpers.hpp>

#include <Scene/Components/MeshComponent.hpp>
#include <Scene/Components/VisibilityStateComponent.hpp>
#include <Scene/Components/BoundingBoxComponent.hpp>
#include <Scene/Components/TransformComponent.hpp>
#include <Scene/Components/RigidBodyComponent.hpp>
#include <Scene/Components/CharacterControllerComponent.hpp>
#include <Scene/Components/TerrainCellComponent.hpp>

#include <Physics/PhysicsShape.hpp>
#include <Physics/ConvexDecomposition.hpp>

#include <Scene/LightmapVolume.hpp>
#include <Scene/Volume.hpp>
#include <Scene/Sprite.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>
#include <Asset/AssetBatch.hpp>

#include <UI/UISubsystem.hpp>
#include <UI/UIObject.hpp>
#include <UI/UIStage.hpp>
#include <UI/UIImage.hpp>
#include <UI/UIListView.hpp>
#include <UI/UIWindow.hpp>
#include <UI/UIGrid.hpp>
#include <UI/UIText.hpp>
#include <UI/UIButton.hpp>
#include <UI/UIMenuBar.hpp>
#include <UI/UIDataSource.hpp>
#include <UI/UITextbox.hpp>

#include <Input/InputManager.hpp>
#include <Input/Event.hpp>

#include <System/AppContext.hpp>
#include <System/OpenFileDialog.hpp>
#include <System/MessageBox.hpp>

#include <Core/Threading/TaskSystem.hpp>

#include <Core/Utilities/DeferredScope.hpp>
#include <Core/Utilities/GlobalContext.hpp>

#include <Core/IO/ByteWriter.hpp>

#include <Rendering/Mesh.hpp>
#include <Rendering/Material.hpp>

#include <Rendering/Util/MeshBuilder.hpp>

#include <Framework/View.hpp>
#include <Framework/EngineGlobals.hpp>
#include <Framework/CVarManager.hpp>

#include <Framework/Server/GameServer.hpp>

#include <Rendering/Texture.hpp>
#include <Rendering/RendererMain.hpp>
#include <Rendering/RenderProxyList.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/DebugDrawer.hpp>
#include <Rendering/Passes/EditorGridPass.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Baking/BakerSubsystem.hpp>
#include <Baking/BakeData.hpp>
#include <Baking/Baker.hpp>
#include <Baking/BakeEpoch.hpp>
#include <Baking/BakeLayer.hpp>

#include <UI/Overlays/MessagesOverlay.hpp>
#include <UI/Overlays/StatsOverlay.hpp>

// for EnumToString
#include <Core/Reflection/Enum.hpp>
#include <Core/Reflection/ClassRegistry.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Scripting/ScriptingService.hpp>

#include <Framework/Game.hpp>
#include <Framework/Client/GameClient.hpp>

#include <Framework/EngineDriver.hpp>
#include <Framework/Threads/RenderThread.hpp>

#include <HyperionEngine.hpp>

#include <EditorSubsystem.generated.inl>

namespace Hyperion {

HYP_DEFINE_LOG_CHANNEL(Editor);

CVar<CVarString> g_cvCodeEditor { "Editor.CodeEditor", "VSCode" };
static CVar<bool> s_cvDebugDrawPhysics { "Physics.DebugDraw", false };
static CVar<bool> s_cvShowMeshLods { "Editor.ShowMeshLods", false };
static CVar<bool> s_cvDebugDrawProbes { "Editor.DebugDrawProbes", false };

static constexpr const char* PlayNetModeConfigKey = "PlayInEditor.NetMode";
static constexpr const char* PlayNetHostConfigKey = "PlayInEditor.Host";
static constexpr const char* PlayNetPortConfigKey = "PlayInEditor.Port";
static constexpr const char* PlayNetCachePortConfigKey = "PlayInEditor.CachePort";

struct SuppressIdleThrottlingContext {};

#pragma region Helpers

static RenderableAttributeSet PhysicsWireframeAttributes()
{
    RenderableAttributeSet attributes;

    MeshAttributes& meshAttributes = attributes.GetMeshAttributes();
    meshAttributes.inputLayout = StaticVertexInputLayout<VT_Simple>;
    meshAttributes.topology = Topology::Triangles;

    MaterialAttributes& materialAttributes = attributes.GetMaterialAttributes();
    materialAttributes.bucket = RenderBucket::Debug;
    materialAttributes.fillMode = FillMode::Line;
    materialAttributes.blendFunction = BlendFunction::None();
    materialAttributes.flags = MAF_DEPTH_TEST;

    return attributes;
}

static Vec3f ComputeMeshEditDragPlaneNormal(const Handle<Camera>& camera, const Vec3f& axisDirection)
{
    if (axisDirection.LengthSquared() < MathUtil::epsilonF)
    {
        return -camera->GetDirection();
    }

    const Vec3f crossUp = axisDirection.Cross(camera->GetUpVector());
    const Vec3f crossSide = axisDirection.Cross(camera->GetSideVector());

    return (crossUp.LengthSquared() > crossSide.LengthSquared() ? crossUp : crossSide).Normalized();
}

#pragma endregion Helpers

#pragma region Terrain

Handle<EditorTerrainState> EditorSubsystem::GetTerrainState()
{
    if (!m_terrainSculpting.IsValid())
    {
        m_terrainSculpting = MakeHandle<EditorTerrainState>();
        InitObject(m_terrainSculpting);

        m_terrainSculpting->Initialize(this);
    }

    return m_terrainSculpting;
}

Handle<EditorDecalPainterState> EditorSubsystem::GetDecalPainterState()
{
    if (!m_decalPainter.IsValid())
    {
        m_decalPainter = MakeHandle<EditorDecalPainterState>();
        InitObject(m_decalPainter);

        m_decalPainter->Initialize(this);
    }

    return m_decalPainter;
}

#pragma endregion Terrain

bool EditorSubsystem::IsMeshEditModeEnabled() const
{
    AssertOnThread(g_simThread);

    return m_meshEditState.enabled;
}

Node* EditorSubsystem::ResolveMeshEditTarget(MeshComponent** outMeshComponent) const
{
    Handle<Node> node = m_meshEditState.enabled
        ? m_meshEditState.targetNode.Lock()
        : m_focusedNode.Lock();

    if (!node.IsValid())
    {
        return nullptr;
    }

    Entity* entity = DynamicCast<Entity>(node.Get());

    if (!entity)
    {
        return nullptr;
    }

    MeshComponent* meshComponent = entity->TryGetComponent<MeshComponent>();

    if (!meshComponent || !meshComponent->mesh.IsValid())
    {
        return nullptr;
    }

    if (outMeshComponent)
    {
        *outMeshComponent = meshComponent;
    }

    return node.Get();
}

bool EditorSubsystem::CanEnableMeshEditMode() const
{
    if (m_meshEditState.enabled)
    {
        return true;
    }

    if (!m_currentProject.IsValid() || IsSimulating())
    {
        return false;
    }

    return ResolveMeshEditTarget() != nullptr;
}

Node* EditorSubsystem::GetMeshEditTargetNode() const
{
    if (!m_meshEditState.enabled)
    {
        return nullptr;
    }

    return m_meshEditState.targetNode.Lock().Get();
}

bool EditorSubsystem::HasMeshEditFaceSelected() const
{
    return m_meshEditState.selectedFace.HasValue();
}

bool EditorSubsystem::IsMeshEditDragActive() const
{
    return m_meshEditState.dragData.HasValue();
}

int EditorSubsystem::GetMeshEditLockedAxis() const
{
    if (!m_meshEditState.dragData)
    {
        return -1;
    }

    return m_meshEditState.dragData->lockedAxis;
}

bool EditorSubsystem::HasPendingMeshEdits() const
{
    return m_meshEditState.baselinePositions.Any();
}

EditorActionStack* EditorSubsystem::GetActiveActionStack() const
{
    if (m_meshEditState.enabled && m_meshEditState.actionStack.IsValid())
    {
        return m_meshEditState.actionStack.Get();
    }

    if (!m_currentProject.IsValid())
    {
        return nullptr;
    }

    return m_currentProject->GetActionStack().Get();
}

bool EditorSubsystem::IsSimulating() const
{
    // If the pre-sim project is set, we're in simulation mode
    return m_preSimulationProject.IsValid();
}

bool EditorSubsystem::CanCreateAssets() const
{
    return m_currentProject.IsValid() && !IsSimulating();
}

bool EditorSubsystem::IsSnapToGridEnabled() const
{
    return m_gizmoController->IsSnapToGridEnabled();
}

void EditorSubsystem::SetSnapToGridEnabled(bool snapToGrid)
{
    m_gizmoController->SetSnapToGridEnabled(snapToGrid);
}

bool EditorSubsystem::IsGridVisible() const
{
    return g_cvEditorGrid.Get();
}

void EditorSubsystem::SetGridVisible(bool visible)
{
    g_cvEditorGrid.Set(visible);
}

float EditorSubsystem::GetGridSize() const
{
    return g_cvEditorGridSize.Get();
}

void EditorSubsystem::SetGridSize(float gridSize)
{
    g_cvEditorGridSize.Set(MathUtil::Max(gridSize, 0.001f));
}

Vec3f EditorSubsystem::GetGridOffset() const
{
    return Vec3f(g_cvEditorGridOffsetX.Get(), g_cvEditorGridOffsetY.Get(), g_cvEditorGridOffsetZ.Get());
}

void EditorSubsystem::SetGridOffset(Vec3f gridOffset)
{
    g_cvEditorGridOffsetX.Set(gridOffset.x);
    g_cvEditorGridOffsetY.Set(gridOffset.y);
    g_cvEditorGridOffsetZ.Set(gridOffset.z);
}

#pragma region Entity Swatch Overrides

Array<Name> EditorSubsystem::GetEntitySwatchOverrideSets(Entity* entity) const
{
    if (!entity)
    {
        return {};
    }

    if (SwatchOverrideSystem* overrideSystem = SceneHelpers::GetSwatchOverrideSystemFor(*entity))
    {
        return overrideSystem->GetSetSwatchNames(entity);
    }

    return {};
}

bool EditorSubsystem::EntityHasSwatchOverrideSet(Entity* entity, Name swatchName) const
{
    if (!entity)
    {
        return false;
    }

    SwatchOverrideSystem* overrideSystem = SceneHelpers::GetSwatchOverrideSystemFor(*entity);

    return overrideSystem && overrideSystem->HasSwatchOverrideSet(entity, swatchName);
}

bool EditorSubsystem::EntityHasSwatchOverrideValues(Entity* entity, Name swatchName) const
{
    if (!entity)
    {
        return false;
    }

    SwatchOverrideSystem* overrideSystem = SceneHelpers::GetSwatchOverrideSystemFor(*entity);

    return overrideSystem && overrideSystem->HasAnyOverriddenProperty(entity, swatchName);
}

void EditorSubsystem::EntityAddSwatchOverrideSet(Entity* entity, Name swatchName) const
{
    if (!entity)
    {
        return;
    }

    if (SwatchOverrideSystem* overrideSystem = SceneHelpers::GetSwatchOverrideSystemFor(*entity))
    {
        overrideSystem->AddSwatchOverrideSet(entity, swatchName);
    }
}

bool EditorSubsystem::EntityRemoveSwatchOverrideSet(Entity* entity, Name swatchName) const
{
    if (!entity)
    {
        return false;
    }

    SwatchOverrideSystem* overrideSystem = SceneHelpers::GetSwatchOverrideSystemFor(*entity);

    return overrideSystem && overrideSystem->RemoveSwatchOverrideSet(entity, swatchName);
}

bool EditorSubsystem::IsEntityPropertyOverridden(Entity* entity, Name swatchName, Name propertyName) const
{
    if (!entity)
    {
        return false;
    }

    SwatchOverrideSystem* overrideSystem = SceneHelpers::GetSwatchOverrideSystemFor(*entity);

    return overrideSystem && overrideSystem->IsPropertyOverriddenInSwatch(entity, swatchName, propertyName);
}

bool EditorSubsystem::EntityRemoveSwatchOverrideValue(Entity* entity, Name swatchName, Name propertyName) const
{
    if (!entity)
    {
        return false;
    }

    SwatchOverrideSystem* overrideSystem = SceneHelpers::GetSwatchOverrideSystemFor(*entity);

    return overrideSystem && overrideSystem->RemoveSwatchOverrideValue(entity, swatchName, propertyName);
}

Name EditorSubsystem::GetEntityAppliedOverrideSwatch(Entity* entity) const
{
    if (!entity)
    {
        return Name::Invalid();
    }

    if (SwatchOverrideSystem* overrideSystem = SceneHelpers::GetSwatchOverrideSystemFor(*entity))
    {
        return overrideSystem->GetAppliedOverrideSwatch(entity);
    }

    return Name::Invalid();
}

void EditorSubsystem::EntityApplySwatchOverrides(Entity* entity, Name swatchName) const
{
    if (!entity)
    {
        return;
    }

    if (SwatchOverrideSystem* overrideSystem = SceneHelpers::GetSwatchOverrideSystemFor(*entity))
    {
        overrideSystem->ApplyOverrides(entity, swatchName);
    }
}

void EditorSubsystem::EntityRevertSwatchOverrides(Entity* entity) const
{
    if (!entity)
    {
        return;
    }

    if (SwatchOverrideSystem* overrideSystem = SceneHelpers::GetSwatchOverrideSystemFor(*entity))
    {
        overrideSystem->RevertOverrides(entity);
    }
}

#pragma endregion Entity Swatch Overrides

#pragma region MeshEditMode

static Vec3f ReadMeshVertexPosition(const VertexArrayView& vertexView, size_t vertexSizeInFloats, uint32 vertexIndex)
{
    const TVertexPacket<VT_Position>* packet = reinterpret_cast<const TVertexPacket<VT_Position>*>(
        vertexView.floatData + (vertexIndex * vertexSizeInFloats));

    return Vec3f(packet->posX, packet->posY, packet->posZ);
}

static Optional<uint32> FindCoplanarAdjacentTriangle(
    Mesh* mesh,
    uint8 lodIndex,
    uint32 triangleIndex,
    Span<const uint32> triangleVertexIndices)
{
    auto readScope = mesh->GetReadScope();

    const VertexArrayView vertexView = mesh->GetVertexData(lodIndex);
    const size_t vertexSizeInFloats = vertexView.layoutDesc.VertexSize() / sizeof(float);

    const Vec3f trianglePositions[3] = {
        ReadMeshVertexPosition(vertexView, vertexSizeInFloats, triangleVertexIndices[0]),
        ReadMeshVertexPosition(vertexView, vertexSizeInFloats, triangleVertexIndices[1]),
        ReadMeshVertexPosition(vertexView, vertexSizeInFloats, triangleVertexIndices[2])
    };

    const Vec3f triangleNormal = (trianglePositions[1] - trianglePositions[0])
                                      .Cross(trianglePositions[2] - trianglePositions[0])
                                      .Normalized();

    const Span<const ubyte> indexData = mesh->GetIndexData(lodIndex);
    const Span<const uint32> indices(reinterpret_cast<const uint32*>(indexData.Data()), indexData.Size() / sizeof(uint32));

    const uint32 numTriangles = uint32(indices.Size() / 3);

    // Vertex-normal deviation allowed for a neighboring triangle to still be considered part of the same quad.
    constexpr float coplanarNormalEpsilon = 0.01f;

    for (uint32 candidateTriangleIndex = 0; candidateTriangleIndex < numTriangles; candidateTriangleIndex++)
    {
        if (candidateTriangleIndex == triangleIndex)
        {
            continue;
        }

        const uint32 candidateVertexIndices[3] = {
            indices[candidateTriangleIndex * 3 + 0],
            indices[candidateTriangleIndex * 3 + 1],
            indices[candidateTriangleIndex * 3 + 2]
        };

        uint32 sharedVertexCount = 0;

        for (uint32 candidateVertexIndex : candidateVertexIndices)
        {
            if (std::find(triangleVertexIndices.Begin(), triangleVertexIndices.End(), candidateVertexIndex) != triangleVertexIndices.End())
            {
                sharedVertexCount++;
            }
        }

        // A quad neighbor shares exactly one edge (two vertices) with the picked triangle.
        if (sharedVertexCount != 2)
        {
            continue;
        }

        const Vec3f candidatePositions[3] = {
            ReadMeshVertexPosition(vertexView, vertexSizeInFloats, candidateVertexIndices[0]),
            ReadMeshVertexPosition(vertexView, vertexSizeInFloats, candidateVertexIndices[1]),
            ReadMeshVertexPosition(vertexView, vertexSizeInFloats, candidateVertexIndices[2])
        };

        const Vec3f candidateNormal = (candidatePositions[1] - candidatePositions[0])
                                           .Cross(candidatePositions[2] - candidatePositions[0])
                                           .Normalized();

        if (candidateNormal.Dot(triangleNormal) >= 1.0f - coplanarNormalEpsilon)
        {
            return candidateTriangleIndex;
        }
    }

    return {};
}

static Array<uint32, EditorAllocator> FindWeldedVertexIndices(
    Mesh* mesh,
    uint8 lodIndex,
    Span<const uint32> faceVertexIndices)
{
    auto readScope = mesh->GetReadScope();

    const VertexArrayView vertexView = mesh->GetVertexData(lodIndex);
    const size_t vertexSizeInFloats = vertexView.layoutDesc.VertexSize() / sizeof(float);

    Array<Vec3f, EditorAllocator> faceVertexPositions;
    faceVertexPositions.Reserve(faceVertexIndices.Size());

    for (uint32 vertexIndex : faceVertexIndices)
    {
        faceVertexPositions.PushBack(ReadMeshVertexPosition(vertexView, vertexSizeInFloats, vertexIndex));
    }

    Array<uint32, EditorAllocator> affectedVertexIndices = faceVertexIndices;

    static constexpr float WeldDistanceSquared = 0.0001f * 0.0001f;

    for (uint32 candidateVertexIndex = 0; candidateVertexIndex < vertexView.vertexCount; candidateVertexIndex++)
    {
        if (affectedVertexIndices.Contains(candidateVertexIndex))
        {
            continue;
        }

        const Vec3f candidatePosition = ReadMeshVertexPosition(vertexView, vertexSizeInFloats, candidateVertexIndex);

        for (const Vec3f& faceVertexPosition : faceVertexPositions)
        {
            if ((candidatePosition - faceVertexPosition).LengthSquared() <= WeldDistanceSquared)
            {
                affectedVertexIndices.PushBack(candidateVertexIndex);
                break;
            }
        }
    }

    return affectedVertexIndices;
}

static void ApplyMeshEditVertexPositions(
    const Handle<Node>& node,
    uint8 lodIndex,
    const Array<uint32, EditorAllocator>& vertexIndices,
    const Array<Vec3f, EditorAllocator>& localPositions,
    uint32 expectedVertexCount,
    bool recomputeDerivedData)
{
    Entity* entity = DynamicCast<Entity>(node.Get());
    MeshComponent* meshComponent = entity ? entity->TryGetComponent<MeshComponent>() : nullptr;

    if (!meshComponent || !meshComponent->mesh.IsValid())
    {
        return;
    }

    if (vertexIndices.Size() != localPositions.Size())
    {
        return;
    }

    Mesh* mesh = meshComponent->mesh;

    {
        Array<float, EditorAllocator> mutableVertexData;
        Array<ubyte, EditorAllocator> indexData;

        size_t vertexSizeInFloats;
        VertexInputLayoutDesc layoutDesc;
        uint32 vertexCount;

        { // read scope needed to access the data.
            auto readScope = mesh->GetReadScope();

            const VertexArrayView vertexView = mesh->GetVertexData(lodIndex);

            layoutDesc = vertexView.layoutDesc;
            vertexSizeInFloats = vertexView.layoutDesc.VertexSize() / sizeof(float);
            vertexCount = uint32(vertexView.vertexCount);

            // the LOD was regenerated or swapped since these edits were captured
            if (vertexCount != expectedVertexCount)
            {
                HYP_LOG(Editor, Warning, "Mesh {} LOD {} has {} vertices but the edit expected {}; skipping stale mesh edit",
                    mesh->GetName(), lodIndex, vertexCount, expectedVertexCount);

                return;
            }

            mutableVertexData = Array<float, EditorAllocator>(vertexView.floatData, vertexView.vertexCount * vertexSizeInFloats);
            indexData = mesh->GetIndexData(lodIndex);
        }

        Assert(vertexSizeInFloats != 0);

        for (uint32 vertexIndex : vertexIndices)
        {
            if (vertexIndex >= vertexCount)
            {
                HYP_LOG(Editor, Warning, "Mesh edit vertex index {} out of range for mesh {} LOD {} ({} vertices); skipping",
                    vertexIndex, mesh->GetName(), lodIndex, vertexCount);

                return;
            }
        }

        auto writeScope = mesh->GetWriteScope();

        for (size_t i = 0; i < vertexIndices.Size(); i++)
        {
            TVertexPacket<VT_Position>* packet = reinterpret_cast<TVertexPacket<VT_Position>*>(
                mutableVertexData.Data() + (vertexIndices[i] * vertexSizeInFloats));

            packet->posX = localPositions[i].x;
            packet->posY = localPositions[i].y;
            packet->posZ = localPositions[i].z;
        }

        VertexArrayView newVertexView {};
        newVertexView.floatData = mutableVertexData.Data();
        newVertexView.vertexCount = mutableVertexData.Size() / vertexSizeInFloats;
        newVertexView.layoutDesc = layoutDesc;

        mesh->SetVertexData(lodIndex, newVertexView);

        // We set index data anyway, since it'll need to be resident in memory,
        // which is not guaranteed since we only have a write scope active.
        // Hacky solution, but it works.
        mesh->SetIndexData(lodIndex, indexData);

        // Bounds and the mesh BVH describe LOD 0; a coarser LOD's edits must not overwrite them.
        if (lodIndex == 0)
        {
            const BoundingBox meshBounds = mesh->CalculateAABB();

            if (recomputeDerivedData)
            {
                mesh->SetAABB(meshBounds);

                BVHNode bvh;
                mesh->BuildBVH(bvh);

                mesh->SetBVH(std::move(bvh));
            }

            entity->SetLocalBounds(meshBounds);
        }
    }

    // Update editor pick cache, so we don't test against old verts
    g_editorState->GetPickCache().PutEntry(mesh, /* invalidate */ true);

    mesh->UploadGpuData();
}

static bool ReadAllMeshVertexPositions(Mesh* mesh, uint8 lodIndex, Array<Vec3f, EditorAllocator>& outPositions)
{
    if (!mesh)
    {
        return false;
    }

    auto readScope = mesh->GetReadScope();

    if (!readScope)
    {
        return false;
    }

    const VertexArrayView vertexView = mesh->GetVertexData(lodIndex);
    const size_t vertexSizeInFloats = vertexView.layoutDesc.VertexSize() / sizeof(float);

    outPositions.Clear();
    outPositions.Reserve(vertexView.vertexCount);

    for (uint32 vertexIndex = 0; vertexIndex < vertexView.vertexCount; vertexIndex++)
    {
        outPositions.PushBack(ReadMeshVertexPosition(vertexView, vertexSizeInFloats, vertexIndex));
    }

    return true;
}

static void WriteAllMeshVertexPositions(
    const Handle<Node>& node,
    uint8 lodIndex,
    const Array<Vec3f, EditorAllocator>& positions)
{
    Entity* entity = DynamicCast<Entity>(node.Get());
    MeshComponent* meshComponent = entity ? entity->TryGetComponent<MeshComponent>() : nullptr;

    if (!meshComponent || !meshComponent->mesh.IsValid())
    {
        return;
    }

    Array<uint32, EditorAllocator> vertexIndices;
    vertexIndices.Reserve(positions.Size());

    for (uint32 i = 0; i < uint32(positions.Size()); i++)
    {
        vertexIndices.PushBack(i);
    }

    ApplyMeshEditVertexPositions(node, lodIndex, vertexIndices, positions, uint32(positions.Size()), /* recomputeDerivedData */ true);
}

void EditorSubsystem::EnterMeshEditMode()
{
    AssertOnThread(g_simThread);

    if (m_meshEditState.enabled)
    {
        return;
    }

    if (!m_currentProject.IsValid())
    {
        return;
    }

    Node* target = ResolveMeshEditTarget();

    if (!target)
    {
        HYP_LOG(Editor, Warning, "Cannot enter mesh edit mode: the focused node has no editable mesh");

        return;
    }

    m_meshEditState.targetNode = MakeWeakRef(target);

    m_meshEditState.manipulationModeBeforeMeshEdit = m_gizmoController->GetSelectedManipulationMode();

    m_meshEditState.enabled = true;

    // Edit whatever LOD is on screen, and hold the entity there so LOD selection can't swap it mid-edit.
    SetMeshEditLod(ResolveMeshEditLod(target));

    m_meshEditState.actionStack = MakeHandle<EditorActionStack>(m_currentProject.ToWeak());

    m_meshEditState.baselinePositions.Clear();
    m_meshEditState.baselineMesh.Reset();

    m_meshEditState.isChanging = true;
    SetSelectedManipulationMode(EditorManipulationMode::None);
    m_meshEditState.isChanging = false;

    OnMeshEditStateChanged();
}

void EditorSubsystem::ExitMeshEditMode(bool saveEdits)
{
    AssertOnThread(g_simThread);

    if (!m_meshEditState.enabled)
    {
        return;
    }

    EndMeshEditDrag(/* saveEdits */ true);

    if (saveEdits)
    {
        CommitMeshEdits();
    }
    else
    {
        DiscardMeshEdits();
    }

    m_meshEditState.enabled = false;

    SetSelectedMeshEditFace({});

    ClearMeshLodOverride();

    m_meshEditState.hoveredFace.Unset();
    m_meshEditState.targetNode.Reset();
    m_meshEditState.actionStack.Reset();

    m_meshEditState.lodIndex = 0;
    m_meshEditState.lodPickBvh.Reset();
    m_meshEditState.lodPickBvhMesh.Reset();
    m_meshEditState.lodPickBvhDirty = true;

    m_meshEditState.isChanging = true;
    SetSelectedManipulationMode(m_meshEditState.manipulationModeBeforeMeshEdit);
    m_meshEditState.isChanging = false;

    OnMeshEditStateChanged();
}

bool EditorSubsystem::BackOutOfMeshEditState()
{
    if (!m_meshEditState.enabled)
    {
        return false;
    }

    if (IsMeshEditDragActive())
    {
        EndMeshEditDrag(false);

        return true;
    }

    if (m_meshEditState.selectedFace)
    {
        SetSelectedMeshEditFace({});

        return true;
    }

    ExitMeshEditMode(/* saveEdits */ true);

    return true;
}

MeshEditFaceMode EditorSubsystem::GetMeshEditFaceMode() const
{
    AssertOnThread(g_simThread);

    return m_meshEditState.faceMode;
}

void EditorSubsystem::SetMeshEditFaceMode(MeshEditFaceMode faceMode)
{
    AssertOnThread(g_simThread);

    if (faceMode == m_meshEditState.faceMode)
    {
        return;
    }

    m_meshEditState.faceMode = faceMode;

    // The existing selection was built for the other mode's vertex count, so it can't carry over.
    SetSelectedMeshEditFace({});
    m_meshEditState.hoveredFace.Unset();
}

// LOD is chosen per view when draw calls are built, so when the editor has to name a single LOD on the sim
// thread it recreates that choice from the largest screen size any of the world's LOD-collecting views sees.
static uint8 SelectMeshLodForViews(Span<const LODViewData> viewDatas, const MeshComponent& meshComponent, const BoundingBox& worldAabb)
{
    if (!meshComponent.mesh.IsValid())
    {
        return 0;
    }

    MeshLodSelectionParams params;
    params.numLods = MathUtil::Max(meshComponent.mesh->GetMeshDesc().GetNumLods(), uint8(1));
    params.forcedLod = meshComponent.forcedLod;
    params.lodBias = meshComponent.lodBias;

    const BoundingSphere boundingSphere { worldAabb };

    float screenSize = 0.0f;

    for (const LODViewData& viewData : viewDatas)
    {
        screenSize = MathUtil::Max(screenSize, viewData.ComputeScreenSize(boundingSphere));
    }

    return SelectMeshLod(meshComponent.mesh->GetMeshDesc(), params, screenSize, 0);
}

uint8 EditorSubsystem::ResolveMeshEditLod(Node* targetNode) const
{
    Entity* entity = DynamicCast<Entity>(targetNode);

    if (entity == nullptr)
    {
        return 0;
    }

    MeshComponent* meshComponent = entity->TryGetComponent<MeshComponent>();

    if (meshComponent == nullptr || !meshComponent->mesh.IsValid())
    {
        return 0;
    }

    BoundingBoxComponent* boundingBoxComponent = entity->TryGetComponent<BoundingBoxComponent>();

    if (boundingBoxComponent == nullptr)
    {
        return 0;
    }

    Array<LODViewData, SceneTempAllocator> viewDatas;
    GetCurrentProject()->GetWorld()->CollectLODViewDatas(viewDatas);

    return SelectMeshLodForViews(viewDatas.ToSpan(), *meshComponent, boundingBoxComponent->worldAabb);
}

uint8 EditorSubsystem::GetMeshEditLod() const
{
    AssertOnThread(g_simThread);

    return m_meshEditState.lodIndex;
}

void EditorSubsystem::SetMeshEditLod(uint8 lodIndex)
{
    AssertOnThread(g_simThread);

    if (!m_meshEditState.enabled)
    {
        return;
    }

    Handle<Node> targetNode = m_meshEditState.targetNode.Lock();
    Entity* entity = DynamicCast<Entity>(targetNode.Get());

    if (entity == nullptr)
    {
        return;
    }

    MeshComponent* meshComponent = entity->TryGetComponent<MeshComponent>();

    if (meshComponent == nullptr || !meshComponent->mesh.IsValid())
    {
        return;
    }

    const uint8 numLods = MathUtil::Max(meshComponent->mesh->GetMeshDesc().GetNumLods(), uint8(1));
    const uint8 clampedLodIndex = MathUtil::Min(lodIndex, uint8(numLods - 1));

    if (m_meshEditState.lodIndex != clampedLodIndex)
    {
        // baseline + undo entries index into the old LOD's vertex buffer, so settle them before switching
        EndMeshEditDrag(/* saveEdits */ true);
        CommitMeshEdits();

        m_meshEditState.actionStack = MakeHandle<EditorActionStack>(m_currentProject.ToWeak());

        SetSelectedMeshEditFace({});
        m_meshEditState.hoveredFace.Unset();
    }

    m_meshEditState.lodIndex = clampedLodIndex;
    m_meshEditState.lodPickBvhDirty = true;

    // every view must keep rendering the LOD being edited, whatever screen size the entity is at
    SetMeshLodOverride(entity->Id(), clampedLodIndex);

    OnMeshEditStateChanged();
}

uint8 EditorSubsystem::GetMeshEditNumLods() const
{
    AssertOnThread(g_simThread);

    MeshComponent* meshComponent = nullptr;

    if (!ResolveMeshEditTarget(&meshComponent) || meshComponent == nullptr || !meshComponent->mesh.IsValid())
    {
        return 0;
    }

    return meshComponent->mesh->GetMeshDesc().GetNumLods();
}

bool EditorSubsystem::AreMeshEditLodsOutOfDate() const
{
    AssertOnThread(g_simThread);

    MeshComponent* meshComponent = nullptr;

    if (!ResolveMeshEditTarget(&meshComponent) || meshComponent == nullptr || !meshComponent->mesh.IsValid())
    {
        return false;
    }

    auto readScope = meshComponent->mesh->GetReadScope();

    return meshComponent->mesh->AreLodsOutOfDate();
}

void EditorSubsystem::RegenerateMeshEditLods()
{
    AssertOnThread(g_simThread);

    MeshComponent* meshComponent = nullptr;

    if (!ResolveMeshEditTarget(&meshComponent) || meshComponent == nullptr || !meshComponent->mesh.IsValid())
    {
        return;
    }

    // LOD 0 is the source and survives regeneration; anything captured against a coarser LOD doesn't
    if (m_meshEditState.lodIndex != 0)
    {
        EndMeshEditDrag(/* saveEdits */ true);
        CommitMeshEdits();

        m_meshEditState.actionStack = MakeHandle<EditorActionStack>(m_currentProject.ToWeak());

        SetSelectedMeshEditFace({});
        m_meshEditState.hoveredFace.Unset();
    }

    meshComponent->mesh->GenerateLods();

    m_meshEditState.lodPickBvhDirty = true;

    SetMeshEditLod(m_meshEditState.lodIndex);
}

bool EditorSubsystem::IsMeshEditAlignToNormal() const
{
    AssertOnThread(g_simThread);

    return m_meshEditState.alignToNormal;
}

void EditorSubsystem::SetMeshEditAlignToNormal(bool alignToNormal)
{
    AssertOnThread(g_simThread);

    if (alignToNormal == m_meshEditState.alignToNormal)
    {
        return;
    }

    m_meshEditState.alignToNormal = alignToNormal;

    OnMeshEditStateChanged();
}

void EditorSubsystem::CaptureMeshEditBaseline()
{
    AssertOnThread(g_simThread);

    if (m_meshEditState.baselinePositions.Any())
    {
        return;
    }

    MeshComponent* meshComponent = nullptr;
    Node* target = ResolveMeshEditTarget(&meshComponent);

    if (!target || !meshComponent)
    {
        return;
    }

    Mesh* mesh = meshComponent->mesh;

    if (!ReadAllMeshVertexPositions(mesh, m_meshEditState.lodIndex, m_meshEditState.baselinePositions))
    {
        HYP_LOG(Editor, Warning, "Failed to capture mesh edit baseline for {}; edits will not be undoable as a unit", target->GetName());

        return;
    }

    m_meshEditState.baselineMesh = MakeWeakRef(mesh);
}

void EditorSubsystem::SyncBoxPhysicsShapeToLocalBounds(Entity* entity)
{
    if (entity == nullptr)
    {
        return;
    }

    RigidBodyComponent* rigidBodyComponent = entity->TryGetComponent<RigidBodyComponent>();

    if (rigidBodyComponent == nullptr)
    {
        return;
    }

    if (!rigidBodyComponent->shape.IsValid() || rigidBodyComponent->shape->GetType() != PhysicsShapeType::Box)
    {
        return;
    }

    // Clone the shape first to prevent stomping something used by another
    Handle<PhysicsShape> uniqueShape = EnsureUniquePhysicsShape(entity);
    Handle<BoxPhysicsShape> boxShape = DynamicCast<BoxPhysicsShape>(uniqueShape);

    if (!boxShape.IsValid())
    {
        return;
    }

    boxShape->SetAABB(entity->GetLocalBounds());
    boxShape->Invalidate();

    entity->AddTag<EntityTag::UpdatePhysicsShape>();
}

void EditorSubsystem::CommitMeshEdits()
{
    AssertOnThread(g_simThread);

    if (!HasPendingMeshEdits())
    {
        return;
    }

    Handle<EditorProject> project = GetCurrentProject();
    Handle<Node> node = m_meshEditState.targetNode.Lock();
    Handle<Mesh> baselineMesh = m_meshEditState.baselineMesh.Lock();

    Array<Vec3f, EditorAllocator> baselinePositions = std::move(m_meshEditState.baselinePositions);

    m_meshEditState.baselinePositions.Clear();
    m_meshEditState.baselineMesh.Reset();

    if (!project.IsValid() || !node.IsValid() || !baselineMesh.IsValid())
    {
        return;
    }

    Array<Vec3f, EditorAllocator> finalPositions;

    if (!ReadAllMeshVertexPositions(baselineMesh.Get(), m_meshEditState.lodIndex, finalPositions))
    {
        HYP_LOG(Editor, Warning, "Failed to read final mesh state for {}; mesh edits will not be undoable", node->GetName());

        return;
    }

    if (finalPositions.Size() != baselinePositions.Size())
    {
        HYP_LOG(Editor, Warning, "Mesh {} changed vertex count during editing; cannot record an undoable action", node->GetName());

        return;
    }

    if (finalPositions == baselinePositions)
    {
        return;
    }

    const uint8 lodIndex = m_meshEditState.lodIndex;

    project->GetActionStack()->PushAction(MakeHandle<FunctionalEditorAction>(
        "Apply Mesh Edits",
        [nodeWeak = node.ToWeak(), baselinePositions, finalPositions, lodIndex]() -> EditorActionFunctions
        {
            return {
                [nodeWeak, finalPositions, lodIndex](EditorSubsystem* editorSubsystem, EditorProject* editorProject)
                {
                    if (Handle<Node> node = nodeWeak.Lock(); node.IsValid())
                    {
                        WriteAllMeshVertexPositions(node, lodIndex, finalPositions);
                        editorSubsystem->SyncBoxPhysicsShapeToLocalBounds(DynamicCast<Entity>(node.Get()));
                    }
                },
                [nodeWeak, baselinePositions, lodIndex](EditorSubsystem* editorSubsystem, EditorProject* editorProject)
                {
                    if (Handle<Node> node = nodeWeak.Lock(); node.IsValid())
                    {
                        WriteAllMeshVertexPositions(node, lodIndex, baselinePositions);
                        editorSubsystem->SyncBoxPhysicsShapeToLocalBounds(DynamicCast<Entity>(node.Get()));
                    }
                }
            };
        }));
}

void EditorSubsystem::DiscardMeshEdits()
{
    AssertOnThread(g_simThread);

    if (!HasPendingMeshEdits())
    {
        return;
    }

    Handle<Node> node = m_meshEditState.targetNode.Lock();

    Array<Vec3f, EditorAllocator> baselinePositions = std::move(m_meshEditState.baselinePositions);

    m_meshEditState.baselinePositions.Clear();
    m_meshEditState.baselineMesh.Reset();

    if (!node.IsValid())
    {
        return;
    }

    WriteAllMeshVertexPositions(node, m_meshEditState.lodIndex, baselinePositions);
}

static void EnsureUniqueMeshEditTarget(const Handle<Node>& node, MeshComponent* meshComponent)
{
    if (node->GetTag("HYP_MeshEditUniqueMesh"_sh))
    {
        return;
    }

    Mesh* sourceMesh = meshComponent->mesh;
    Assert(sourceMesh != nullptr);

    if (!sourceMesh)
    {
        return;
    }

    Handle<Mesh> clonedMesh = sourceMesh->Clone();
    clonedMesh->SetName(sourceMesh->GetName()); // Will be unique'd anyway

    if (!clonedMesh->GetBVH().IsValid())
    {
        BVHNode bvh;
        clonedMesh->BuildBVH(bvh);

        auto writeScope = clonedMesh->GetWriteScope();
        clonedMesh->SetBVH(std::move(bvh));
    }

    GetCurrentAssetRegistry()->PutAssetUnique(clonedMesh);

    clonedMesh->UploadGpuData();

    meshComponent->mesh = clonedMesh;

    node->AddTag(NodeTag(NAME("HYP_MeshEditUniqueMesh"), int(1)));

    if (Entity* entity = DynamicCast<Entity>(node.Get()))
    {
        entity->AddTag<EntityTag::UpdateRenderProxy>();
    }
}

// Picking a coarser LOD can't go through Node::TestRay, which only knows the mesh's LOD 0 BVH.
bool EditorSubsystem::PickMeshEditFaceTriangle(const Ray& ray, const Handle<Node>& targetNode, Mesh* mesh, uint8 lodIndex, uint32& outTriangleIndex)
{
    if (lodIndex >= mesh->GetMeshDesc().GetNumLods())
    {
        return false;
    }

    if (m_meshEditState.lodPickBvhDirty
        || m_meshEditState.lodPickBvhMesh.GetUnsafe() != mesh
        || m_meshEditState.lodPickBvhLodIndex != lodIndex)
    {
        auto readScope = mesh->GetReadScope();

        UniquePtr<BVHNode, EditorAllocator> bvh = MakeUniqueWithAllocator<BVHNode, EditorAllocator>();
        mesh->BuildBVH(*bvh, /* maxDepth */ 3, lodIndex);

        m_meshEditState.lodPickBvh = std::move(bvh);
        m_meshEditState.lodPickBvhMesh = MakeWeakRef(mesh);
        m_meshEditState.lodPickBvhLodIndex = lodIndex;
        m_meshEditState.lodPickBvhDirty = false;
    }

    if (!m_meshEditState.lodPickBvh || !m_meshEditState.lodPickBvh->IsValid())
    {
        return false;
    }

    auto readScope = mesh->GetReadScope();

    const VertexArrayView vertexData = mesh->GetVertexData(lodIndex);
    const Span<const ubyte> indexData = mesh->GetIndexData(lodIndex);

    const Ray localSpaceRay = targetNode->GetWorldMatrix().Inverse() * ray;

    RayTestResults results = m_meshEditState.lodPickBvh->TestRay(
        localSpaceRay,
        vertexData,
        Span<const uint32>(reinterpret_cast<const uint32*>(indexData.Data()), indexData.Size() / sizeof(uint32)));

    const RayHit* closestHit = nullptr;

    for (const RayHit& hit : results)
    {
        if (hit.triangleIndex == ~0u)
        {
            continue;
        }

        if (closestHit && hit.distance >= closestHit->distance)
        {
            continue;
        }

        closestHit = &hit;
    }

    if (!closestHit)
    {
        return false;
    }

    outTriangleIndex = closestHit->triangleIndex;

    return true;
}

bool EditorSubsystem::TryPickMeshEditFace(const Ray& ray, MeshEditFaceSelection& outSelection, bool ensureUniqueMesh)
{
    AssertOnThread(g_simThread);

    MeshComponent* meshComponent = nullptr;
    Node* targetNodeRaw = ResolveMeshEditTarget(&meshComponent);

    if (!targetNodeRaw || !meshComponent)
    {
        return false;
    }

    Handle<Node> targetNode = MakeStrongRef(targetNodeRaw);

    const uint8 lodIndex = MathUtil::Min(m_meshEditState.lodIndex, uint8(MathUtil::Max(meshComponent->mesh->GetMeshDesc().GetNumLods(), 1) - 1));

    uint32 pickedTriangleIndex = ~0u;

    if (lodIndex == 0)
    {
        RayTestResults results;

        if (!targetNode->TestRay(ray, results, RayTestFlags::TestBVH | RayTestFlags::EditorPick))
        {
            return false;
        }

        const RayHit* closestHit = nullptr;

        for (const RayHit& hit : results)
        {
            if (hit.node != targetNode.Get() || hit.triangleIndex == ~0u)
            {
                continue;
            }

            if (closestHit && hit.distance >= closestHit->distance)
            {
                continue;
            }

            closestHit = &hit;
        }

        if (!closestHit)
        {
            return false;
        }

        pickedTriangleIndex = closestHit->triangleIndex;
    }
    else if (!PickMeshEditFaceTriangle(ray, targetNode, meshComponent->mesh, lodIndex, pickedTriangleIndex))
    {
        return false;
    }

    if (ensureUniqueMesh)
    {
        EnsureUniqueMeshEditTarget(targetNode, meshComponent);
    }

    Mesh* mesh = meshComponent->mesh;

    auto readScope = mesh->GetReadScope();

    const Span<const ubyte> indexData = mesh->GetIndexData(lodIndex);
    const Span<const uint32> indices(reinterpret_cast<const uint32*>(indexData.Data()), indexData.Size() / sizeof(uint32));

    const uint32 triangleIndex = pickedTriangleIndex;

    Array<uint32, EditorAllocator> vertexIndices {
        indices[triangleIndex * 3 + 0],
        indices[triangleIndex * 3 + 1],
        indices[triangleIndex * 3 + 2]
    };

    if (m_meshEditState.faceMode == MeshEditFaceMode::Quad)
    {
        if (Optional<uint32> adjacentTriangleIndex = FindCoplanarAdjacentTriangle(mesh, lodIndex, triangleIndex, vertexIndices))
        {
            for (uint32 i = 0; i < 3; i++)
            {
                const uint32 vertexIndex = indices[(*adjacentTriangleIndex) * 3 + i];

                if (!vertexIndices.Contains(vertexIndex))
                {
                    vertexIndices.PushBack(vertexIndex);
                }
            }
        }
    }

    outSelection.node = targetNode.ToWeak();
    outSelection.vertexIndices = std::move(vertexIndices);
    outSelection.lodIndex = lodIndex;

    return true;
}

void EditorSubsystem::SetSelectedMeshEditFace(Optional<MeshEditFaceSelection> selection)
{
    AssertOnThread(g_simThread);

    EndMeshEditDrag(true);

    m_meshEditState.selectedFace = std::move(selection);

    OnMeshEditSelectionChanged();
    OnMeshEditStateChanged();
}

void EditorSubsystem::UpdateHoveredMeshEditFace(const Ray& ray)
{
    AssertOnThread(g_simThread);

    MeshEditFaceSelection hoveredFace;

    if (TryPickMeshEditFace(ray, hoveredFace, /* ensureUniqueMesh */ false))
    {
        m_meshEditState.hoveredFace = hoveredFace;
    }
    else
    {
        m_meshEditState.hoveredFace.Unset();
    }
}

// Mesh edit overlays draw *through* the geometry they annotate. Depth-testing them against the very
// surface they sit on makes the highlight z-fight and flicker, which reads as the tool being broken
// rather than as a depth artifact - so the overlay is drawn on top and alpha blended instead.
static RenderableAttributeSet MeshEditOverlayAttributes(FillMode fillMode)
{
    RenderableAttributeSet attributes;

    MeshAttributes& meshAttributes = attributes.GetMeshAttributes();
    meshAttributes.inputLayout = StaticVertexInputLayout<VT_Simple>;
    meshAttributes.topology = Topology::Triangles;

    MaterialAttributes& materialAttributes = attributes.GetMaterialAttributes();
    materialAttributes.bucket = RenderBucket::Debug;
    materialAttributes.fillMode = fillMode;
    materialAttributes.blendFunction = BlendFunction::AlphaBlending();
    materialAttributes.flags = MAF_NONE;

    return attributes;
}

// Axis-lock feedback reuses the conventional X=red / Y=green / Z=blue mapping, so a locked drag is
// identifiable at a glance without reading the status bar.
static Color MeshEditAxisColor(int axis)
{
    switch (axis)
    {
    case 0:
        return Color(1.0f, 0.25f, 0.3f, 1.0f);
    case 1:
        return Color(0.4f, 1.0f, 0.35f, 1.0f);
    case 2:
        return Color(0.3f, 0.55f, 1.0f, 1.0f);
    default:
        return Color(1.0f, 0.7f, 0.1f, 1.0f);
    }
}

// Draws a face as a translucent fill plus a solid wireframe. The wireframe is what actually makes
// the face readable - a flat fill against a lit surface of similar colour is easy to miss entirely.
static void DrawMeshEditFace(
    DebugDrawCommandList& debugDrawCommandList,
    Span<const Vec3f> worldPositions,
    const Color& fillColor,
    const Color& outlineColor)
{
    if (worldPositions.Size() < 3)
    {
        return;
    }

    static const RenderableAttributeSet fillAttributes = MeshEditOverlayAttributes(FillMode::Fill);
    static const RenderableAttributeSet outlineAttributes = MeshEditOverlayAttributes(FillMode::Line);

    debugDrawCommandList.triangle(worldPositions[0], worldPositions[1], worldPositions[2], fillColor, fillAttributes);
    debugDrawCommandList.triangle(worldPositions[0], worldPositions[1], worldPositions[2], outlineColor, outlineAttributes);

    if (worldPositions.Size() == 4)
    {
        debugDrawCommandList.triangle(worldPositions[0], worldPositions[2], worldPositions[3], fillColor, fillAttributes);
        debugDrawCommandList.triangle(worldPositions[0], worldPositions[2], worldPositions[3], outlineColor, outlineAttributes);
    }
}

// Resolves a face selection's vertices to world space. When \p localDelta is non-null the positions
// are offset by it in local space first, which is how the in-progress drag previews its result
// without having written anything back to the mesh yet.
static bool BuildMeshEditFaceWorldPositions(
    const Handle<Node>& node,
    const MeshEditFaceSelection& selection,
    const Vec3f* localDelta,
    const Array<Vec3f, EditorAllocator>* overrideLocalPositions,
    Array<Vec3f, EditorAllocator>& outWorldPositions)
{
    Entity* entity = DynamicCast<Entity>(node.Get());
    MeshComponent* meshComponent = entity ? entity->TryGetComponent<MeshComponent>() : nullptr;

    if (!meshComponent || !meshComponent->mesh.IsValid())
    {
        return false;
    }

    const Mat4f& worldMatrix = node->GetWorldMatrix();
    const uint32 faceVertexCount = uint32(selection.vertexIndices.Size());

    outWorldPositions.Clear();
    outWorldPositions.Reserve(faceVertexCount);

    const auto PushWorldPosition = [&](const Vec3f& localPosition)
    {
        Vec4f transformedPosition = worldMatrix.TransformVector(Vec4f(localPosition, 1.0f));
        transformedPosition /= transformedPosition.w;

        outWorldPositions.PushBack(transformedPosition.GetXYZ());
    };

    if (overrideLocalPositions)
    {
        if (overrideLocalPositions->Size() < faceVertexCount)
        {
            return false;
        }

        for (uint32 i = 0; i < faceVertexCount; i++)
        {
            PushWorldPosition((*overrideLocalPositions)[i] + (localDelta ? *localDelta : Vec3f::Zero()));
        }

        return true;
    }

    Mesh* mesh = meshComponent->mesh;

    auto readScope = mesh->GetReadScope();

    if (!readScope)
    {
        return false;
    }

    const VertexArrayView vertexView = mesh->GetVertexData(selection.lodIndex);
    const size_t vertexSizeInFloats = vertexView.layoutDesc.VertexSize() / sizeof(float);

    for (uint32 vertexIndex : selection.vertexIndices)
    {
        PushWorldPosition(ReadMeshVertexPosition(vertexView, vertexSizeInFloats, vertexIndex)
            + (localDelta ? *localDelta : Vec3f::Zero()));
    }

    return true;
}

void EditorSubsystem::DebugDrawMeshEditSelection(DebugDrawCommandList& debugDrawCommandList)
{
    Array<Vec3f, EditorAllocator> worldPositions;

    // Hover: faint, cool-toned, and only when it isn't the already-selected face - otherwise the
    // hover tint sits on top of the selection and washes out the distinction between the two.
    if (m_meshEditState.hoveredFace
        && !IsMeshEditDragActive()
        && (!m_meshEditState.selectedFace
            || m_meshEditState.hoveredFace->node != m_meshEditState.selectedFace->node
            || m_meshEditState.hoveredFace->vertexIndices != m_meshEditState.selectedFace->vertexIndices))
    {
        if (Handle<Node> hoveredNode = m_meshEditState.hoveredFace->node.Lock(); hoveredNode.IsValid())
        {
            if (BuildMeshEditFaceWorldPositions(hoveredNode, *m_meshEditState.hoveredFace, nullptr, nullptr, worldPositions))
            {
                DrawMeshEditFace(
                    debugDrawCommandList,
                    worldPositions.ToSpan(),
                    Color(0.45f, 0.75f, 1.0f, 0.18f),
                    Color(0.6f, 0.85f, 1.0f, 0.9f));
            }
        }
    }

    if (!m_meshEditState.selectedFace)
    {
        return;
    }

    Handle<Node> node = m_meshEditState.selectedFace->node.Lock();

    if (!node.IsValid())
    {
        return;
    }

    if (m_meshEditState.dragData)
    {
        const Color axisColor = MeshEditAxisColor(m_meshEditState.dragData->lockedAxis);

        // Ghost of where the face started, so the drag distance is visible rather than something
        // you have to infer from memory of where the face used to be.
        if (BuildMeshEditFaceWorldPositions(node, *m_meshEditState.selectedFace, nullptr, &m_meshEditState.dragData->vertexOriginalPositions, worldPositions))
        {
            DrawMeshEditFace(
                debugDrawCommandList,
                worldPositions.ToSpan(),
                Color(1.0f, 1.0f, 1.0f, 0.06f),
                Color(1.0f, 1.0f, 1.0f, 0.35f));
        }

        if (BuildMeshEditFaceWorldPositions(node, *m_meshEditState.selectedFace, &m_meshEditState.dragData->currentLocalDelta, &m_meshEditState.dragData->vertexOriginalPositions, worldPositions))
        {
            DrawMeshEditFace(
                debugDrawCommandList,
                worldPositions.ToSpan(),
                Color(axisColor.GetRed(), axisColor.GetGreen(), axisColor.GetBlue(), 0.35f),
                axisColor);
        }

        return;
    }

    if (BuildMeshEditFaceWorldPositions(node, *m_meshEditState.selectedFace, nullptr, nullptr, worldPositions))
    {
        DrawMeshEditFace(
            debugDrawCommandList,
            worldPositions.ToSpan(),
            Color(1.0f, 0.7f, 0.1f, 0.3f),
            Color(1.0f, 0.8f, 0.15f, 1.0f));
    }
}

void EditorSubsystem::StartMeshEditDrag(const Handle<Camera>& camera, const MouseEvent& mouseEvent)
{
    if (!m_meshEditState.selectedFace)
    {
        return;
    }

    Handle<Node> node = m_meshEditState.selectedFace->node.Lock();

    if (!node.IsValid())
    {
        return;
    }

    Entity* entity = DynamicCast<Entity>(node.Get());
    MeshComponent* meshComponent = entity ? entity->TryGetComponent<MeshComponent>() : nullptr;

    if (!meshComponent || !meshComponent->mesh.IsValid())
    {
        return;
    }

    Mesh* mesh = meshComponent->mesh;
    const uint8 lodIndex = m_meshEditState.selectedFace->lodIndex;

    const Array<uint32, EditorAllocator> affectedVertexIndices = FindWeldedVertexIndices(mesh, lodIndex, m_meshEditState.selectedFace->vertexIndices);

    MeshEditDragData dragData;
    dragData.affectedVertexIndices = affectedVertexIndices;
    dragData.vertexOriginalPositions.Reserve(affectedVertexIndices.Size());

    {
        auto readScope = mesh->GetReadScope();

        const VertexArrayView vertexView = mesh->GetVertexData(lodIndex);
        const size_t vertexSizeInFloats = vertexView.layoutDesc.VertexSize() / sizeof(float);

        for (uint32 vertexIndex : affectedVertexIndices)
        {
            dragData.vertexOriginalPositions.PushBack(ReadMeshVertexPosition(vertexView, vertexSizeInFloats, vertexIndex));
        }
    }

    Vec3f centroidLocalPosition = Vec3f::Zero();

    for (uint32 i = 0; i < m_meshEditState.selectedFace->vertexIndices.Size(); i++)
    {
        centroidLocalPosition += dragData.vertexOriginalPositions[i];
    }

    centroidLocalPosition /= float(m_meshEditState.selectedFace->vertexIndices.Size());

    const Mat4f& worldMatrix = node->GetWorldMatrix();
    Vec4f transformedCentroid = worldMatrix.TransformVector(Vec4f(centroidLocalPosition, 1.0f));
    transformedCentroid /= transformedCentroid.w;

    dragData.faceCentroidWorldOrigin = transformedCentroid.GetXYZ();
    dragData.currentLocalDelta = Vec3f::Zero();

    if (m_meshEditState.alignToNormal)
    {
        const Vec3f faceNormalLocal = (dragData.vertexOriginalPositions[1] - dragData.vertexOriginalPositions[0])
                                           .Cross(dragData.vertexOriginalPositions[2] - dragData.vertexOriginalPositions[0])
                                           .Normalized();

        const Mat4f normalMatrix = worldMatrix.Transpose().Inverse();
        dragData.defaultAxisDirection = normalMatrix.TransformVector(Vec4f(faceNormalLocal, 0.0f)).GetXYZ().Normalized();
    }
    else
    {
        dragData.defaultAxisDirection = Vec3f::Zero();
    }

    dragData.axisDirection = dragData.defaultAxisDirection;
    dragData.planeNormal = ComputeMeshEditDragPlaneNormal(camera, dragData.axisDirection);

    const Ray ray = camera->GetPickRay(mouseEvent.relativePos);

    if (Optional<RayHit> planeHit = ray.TestPlane(dragData.faceCentroidWorldOrigin, dragData.planeNormal))
    {
        dragData.hitpointOrigin = planeHit->hitpoint;
    }
    else
    {
        dragData.hitpointOrigin = dragData.faceCentroidWorldOrigin;
    }

    m_meshEditState.dragData = dragData;

    OnMeshEditStateChanged();
}

void EditorSubsystem::UpdateMeshEditDrag(const Handle<Camera>& camera, const MouseEvent& mouseEvent)
{
    if (!m_meshEditState.dragData || !m_meshEditState.selectedFace)
    {
        return;
    }

    Handle<Node> node = m_meshEditState.selectedFace->node.Lock();

    if (!node.IsValid())
    {
        return;
    }

    const Ray ray = camera->GetPickRay(mouseEvent.relativePos);

    Optional<RayHit> planeHit = ray.TestPlane(m_meshEditState.dragData->faceCentroidWorldOrigin, m_meshEditState.dragData->planeNormal);

    if (!planeHit)
    {
        return;
    }

    Vec3f worldDelta;

    if (m_meshEditState.dragData->axisDirection.LengthSquared() < MathUtil::epsilonF)
    {
        worldDelta = planeHit->hitpoint - m_meshEditState.dragData->hitpointOrigin;

        if (m_gizmoController->IsSnapToGridEnabled())
        {
            const Vec3f centroidOrigin = m_meshEditState.dragData->faceCentroidWorldOrigin;

            worldDelta = m_gizmoController->SnapToGrid(centroidOrigin + worldDelta) - centroidOrigin;
        }
    }
    else
    {
        float t = (planeHit->hitpoint - m_meshEditState.dragData->hitpointOrigin).Dot(m_meshEditState.dragData->axisDirection);

        if (m_gizmoController->IsSnapToGridEnabled())
        {
            t = m_gizmoController->SnapToGridAlongAxis(m_meshEditState.dragData->faceCentroidWorldOrigin, m_meshEditState.dragData->axisDirection, t);
        }

        worldDelta = m_meshEditState.dragData->axisDirection * t;
    }

    const Mat4f inverseWorldMatrix = node->GetWorldMatrix().Inverse();

    m_meshEditState.dragData->currentLocalDelta = inverseWorldMatrix.TransformVector(Vec4f(worldDelta, 0.0f)).GetXYZ();
}

void EditorSubsystem::EndMeshEditDrag(bool saveEdits)
{
    Handle<EditorProject> project = GetCurrentProject();
    if (!project.IsValid())
    {
        return;
    }

    if (!m_meshEditState.dragData)
    {
        return;
    }

    HYP_DEFER({ OnMeshEditStateChanged(); });

    if (!m_meshEditState.selectedFace)
    {
        m_meshEditState.dragData.Unset();
        return;
    }

    Handle<Node> node = m_meshEditState.selectedFace->node.Lock();

    if (!node.IsValid())
    {
        m_meshEditState.dragData.Unset();
        return;
    }

    Entity* entity = DynamicCast<Entity>(node.Get());
    MeshComponent* meshComponent = entity ? entity->TryGetComponent<MeshComponent>() : nullptr;

    if (!meshComponent || !meshComponent->mesh.IsValid())
    {
        m_meshEditState.dragData.Unset();
        return;
    }

    const uint8 lodIndex = m_meshEditState.selectedFace->lodIndex;

    if (m_meshEditState.dragData->currentLocalDelta == Vec3f::Zero())
    {
        m_meshEditState.dragData.Unset();
        return;
    }

    if (saveEdits && m_meshEditState.actionStack.IsValid())
    {
        CaptureMeshEditBaseline();

        Array<Vec3f, EditorAllocator> updatedLocalPositions;
        updatedLocalPositions.Reserve(m_meshEditState.dragData->vertexOriginalPositions.Size());

        for (const Vec3f& originalPosition : m_meshEditState.dragData->vertexOriginalPositions)
        {
            updatedLocalPositions.PushBack(originalPosition + m_meshEditState.dragData->currentLocalDelta);
        }

        Array<uint32, EditorAllocator> vertexIndices = m_meshEditState.dragData->affectedVertexIndices;
        Array<Vec3f, EditorAllocator> originalPositions = m_meshEditState.dragData->vertexOriginalPositions;

        uint32 vertexCount;

        {
            auto readScope = meshComponent->mesh->GetReadScope();

            vertexCount = uint32(meshComponent->mesh->GetVertexData(lodIndex).vertexCount);
        }

        m_meshEditState.actionStack->PushAction(MakeHandle<FunctionalEditorAction>(
            "Move Face",
            [nodeWeak = node.ToWeak(), lodIndex, vertexCount, vertexIndices, originalPositions, updatedLocalPositions]() -> EditorActionFunctions
            {
                return {
                    [nodeWeak, lodIndex, vertexCount, vertexIndices, updatedLocalPositions](EditorSubsystem* editorSubsystem, EditorProject* editorProject)
                    {
                        if (Handle<Node> node = nodeWeak.Lock(); node.IsValid())
                        {
                            ApplyMeshEditVertexPositions(node, lodIndex, vertexIndices, updatedLocalPositions, vertexCount, /* recomputeDerivedData */ true);

                            editorSubsystem->m_meshEditState.lodPickBvhDirty = true;
                        }
                    },
                    [nodeWeak, lodIndex, vertexCount, vertexIndices, originalPositions](EditorSubsystem* editorSubsystem, EditorProject* editorProject)
                    {
                        if (Handle<Node> node = nodeWeak.Lock(); node.IsValid())
                        {
                            ApplyMeshEditVertexPositions(node, lodIndex, vertexIndices, originalPositions, vertexCount, /* recomputeDerivedData */ true);

                            editorSubsystem->m_meshEditState.lodPickBvhDirty = true;
                        }
                    }
                };
            }));
    }

    m_meshEditState.dragData.Unset();
}

void EditorSubsystem::SetMeshEditDragLockedAxis(const Handle<Camera>& camera, const KeyboardEvent& keyboardEvent, int axis)
{
    if (!m_meshEditState.dragData)
    {
        return;
    }

    Vec3f worldAxisDirection = Vec3f::Zero();
    worldAxisDirection[axis] = 1.0f;

    // Pressing the same axis again releases the constraint, so the key acts as a toggle.
    const bool alreadyLockedToThisAxis = m_meshEditState.dragData->lockedAxis == axis;

    m_meshEditState.dragData->lockedAxis = alreadyLockedToThisAxis ? -1 : axis;
    m_meshEditState.dragData->axisDirection = alreadyLockedToThisAxis ? m_meshEditState.dragData->defaultAxisDirection : worldAxisDirection;

    m_meshEditState.dragData->planeNormal = ComputeMeshEditDragPlaneNormal(camera, m_meshEditState.dragData->axisDirection);

    AssertDebug(keyboardEvent.inputManager != nullptr);
    
    // Needs re-anchoring
    const Ray ray = camera->GetPickRay(keyboardEvent.inputManager->GetVirtualMousePositionNormalized());

    if (Optional<RayHit> planeHit = ray.TestPlane(m_meshEditState.dragData->faceCentroidWorldOrigin, m_meshEditState.dragData->planeNormal))
    {
        m_meshEditState.dragData->hitpointOrigin = planeHit->hitpoint;
    }

    OnMeshEditStateChanged();
}

#pragma endregion MeshEditMode

#pragma region EditorSubsystem Gizmos

void EditorSubsystem::InitializeGizmos()
{
    m_gizmoController->Initialize(this);
}

void EditorSubsystem::ShutdownGizmos()
{
    m_gizmoController->Shutdown();
}

EditorManipulationMode EditorSubsystem::GetSelectedManipulationMode() const
{
    return m_gizmoController->GetSelectedManipulationMode();
}

void EditorSubsystem::SetSelectedManipulationMode(EditorManipulationMode mode)
{
    AssertOnThread(g_simThread);

    if (!m_meshEditState.isChanging)
    {
        ExitMeshEditMode(/* saveEdits */ true);
    }

    m_gizmoController->SetSelectedManipulationMode(mode);
}

EditorGizmoBase* EditorSubsystem::GetSelectedGizmo() const
{
    return m_gizmoController->GetSelectedGizmo();
}

EditorGizmoBase* EditorSubsystem::GetGizmo(EditorManipulationMode mode) const
{
    return m_gizmoController->GetGizmo(mode);
}

const EditorSubsystem::EditorGizmoSet& EditorSubsystem::GetGizmos() const
{
    return m_gizmoController->GetGizmos();
}

void EditorSubsystem::UpdateGizmoProximityVisibility()
{
    m_gizmoController->UpdateGizmoProximityVisibility();
}

#pragma endregion EditorSubsystem Gizmos

#pragma region EditorSubsystem

#ifdef HYP_EDITOR

EditorSubsystem::EditorSubsystem()
    : m_gizmoController(MakeUnique<EditorGizmoController>()),
      m_swatchOverrideMode(false),
      m_editorCameraEnabled(false),
      m_shouldCancelNextClick(false)
{
    m_editorDelegates = new EditorDelegates();

    // Create eagerly so the managed side can always fetch it, regardless of the calling thread.
    GetTerrainState();
    GetDecalPainterState();

    m_bakeStatusUpdateTimer = ClockTimer { 0.5f };

    LoadPlayNetSettings();

    OnSelectedGizmoChanged
        .Bind(this, [this](EditorGizmoBase* newGizmo, EditorGizmoBase* prevGizmo)
              {
                  SetHoveredGizmo(MouseEvent {}, nullptr, Handle<Node>::Null());

                  if (prevGizmo && prevGizmo->GetManipulationMode() != EditorManipulationMode::None)
                  {
                      if (prevGizmo->GetNode().IsValid())
                      {
                          prevGizmo->GetNode()->Remove();
                      }

                      prevGizmo->SetFocusedNode(Handle<Node>::Null());
                  }

                  if (newGizmo && newGizmo->GetManipulationMode() != EditorManipulationMode::None)
                  {
                      newGizmo->SetFocusedNode(m_focusedNode.Lock());

                      if (!newGizmo->GetNode().IsValid())
                      {
                          HYP_LOG(Editor, Warning, "Gizmo has no valid node; cannot attach to scene");

                          return;
                      }

                      m_editorScene->GetRoot()->AddChild(newGizmo->GetNode());
                  }
              })
        .Detach();
}

EditorSubsystem::~EditorSubsystem()
{
    if (m_currentProject)
    {
        g_editorState->SetCurrentProject(nullptr, /* isSimulationStateChange */ false);

        m_currentProject->SetEditorSubsystem(nullptr);
        m_currentProject->Close();

        m_currentProject.Reset();
    }

    delete m_editorDelegates;
}

const Handle<World>& EditorSubsystem::GetProjectWorld() const
{
    if (m_currentProject.IsValid())
    {
        return m_currentProject->GetWorld();
    }

    return Handle<World>::Null();
}

void EditorSubsystem::OnAddedToWorld()
{
    World* world = Subsystem::GetWorld();

    if (!world->GetSubsystem<UISubsystem>())
    {
        HYP_FAIL("EditorSubsystem requires UISubsystem to be initialized");
    }

    m_editorScene = MakeHandle<Scene>(NAME("EditorScene"), SceneFlags::FOREGROUND | SceneFlags::EDITOR);
    m_editorScene->SetIsTransient(true);
    world->AddScene(m_editorScene);

    InitViewport();

    if (Handle<AssetCollector> baseAssetCollector = g_assetManager->GetBaseAssetCollector(); baseAssetCollector.IsValid())
    {
        baseAssetCollector->StartWatching();
    }

    g_assetManager->OnAssetCollectorAdded
        .Bind([](const Handle<AssetCollector>& assetCollector)
              {
                  assetCollector->StartWatching();
              })
        .Detach();

    g_assetManager->OnAssetCollectorRemoved
        .Bind([](const Handle<AssetCollector>& assetCollector)
              {
                  assetCollector->StopWatching();
              })
        .Detach();

    m_delegateHandlers.Add(NAME("OnScriptReloaded"), ScriptSystem::OnScriptReloaded.Bind([this]()
        {
            OnScriptReloaded();
        }));

    if (const String startupProjectPath = g_editorState->GetStartupProjectPath(); startupProjectPath.Any())
    {
        TResult<Handle<EditorProject>> loadProjectResult = EditorProject::Load(FilePath(startupProjectPath));

        if (loadProjectResult.HasError())
        {
            HYP_LOG(Editor, Error, "Failed to open last project '{}': {}", startupProjectPath, loadProjectResult.GetError().GetMessage());
        }
        else if (loadProjectResult.GetValue().IsValid())
        {
            OpenProject(loadProjectResult.GetValue());

            return;
        }
    }

    NewProject();
}

void EditorSubsystem::OnRemovedFromWorld()
{
    m_delegateHandlers.Remove("OnScriptReloaded"_sh);

    for (const Handle<EditorViewport>& vp : m_editorViewports)
    {
        vp->OnSceneRemoved(m_editorScene);
    }
    
    World* world = Subsystem::GetWorld();

    world->RemoveScene(m_editorScene);

    if (m_currentProject)
    {
        g_editorState->SetCurrentProject(nullptr, /* isSimulationStateChange */ false);
        
        ShutdownProjectWorld(m_currentProject);
        OnProjectClosing(m_currentProject);

        m_currentProject->Close();
        m_currentProject.Reset();
    }

    m_editorViewports.Clear();
}

bool EditorSubsystem::IsPhysicsDebugDrawEnabled() const
{
    return s_cvDebugDrawPhysics.Get();
}

void EditorSubsystem::SetPhysicsDebugDrawEnabled(bool enabled)
{
    s_cvDebugDrawPhysics.Set(enabled);
}

bool EditorSubsystem::IsGhostModeEnabled() const
{
    return SceneHelpers::IsGhostModeEnabled();
}

void EditorSubsystem::SetGhostModeEnabled(bool enabled)
{
    SceneHelpers::SetGhostModeEnabled(enabled);
}

bool EditorSubsystem::IsShowStatsEnabled() const
{
    return StatsOverlay::IsStatsOverlayEnabled();
}

void EditorSubsystem::SetShowStatsEnabled(bool enabled)
{
    StatsOverlay::SetStatsOverlayEnabled(enabled);
}

// Deep-copy a PhysicsShape asset (there is no reflection-based clone, so switch on the concrete type).
static Handle<PhysicsShape> ClonePhysicsShape(const Handle<PhysicsShape>& source)
{
    if (!source.IsValid())
    {
        return nullptr;
    }

    const Name name = source->GetName();

    switch (source->GetType())
    {
    case PhysicsShapeType::Box:
        return MakeHandle<BoxPhysicsShape>(name, static_cast<BoxPhysicsShape*>(source.Get())->GetAABB());
    case PhysicsShapeType::Sphere:
        return MakeHandle<SpherePhysicsShape>(name, static_cast<SpherePhysicsShape*>(source.Get())->GetSphere());
    case PhysicsShapeType::Capsule:
    {
        const CapsulePhysicsShape* capsule = static_cast<CapsulePhysicsShape*>(source.Get());
        return MakeHandle<CapsulePhysicsShape>(name, capsule->GetRadius(), capsule->GetHeight());
    }
    case PhysicsShapeType::Plane:
        return MakeHandle<PlanePhysicsShape>(name, static_cast<PlanePhysicsShape*>(source.Get())->GetPlane());
    case PhysicsShapeType::ConvexHull:
    {
        const ConvexHullPhysicsShape* hull = static_cast<ConvexHullPhysicsShape*>(source.Get());
        VertexArrayView view {};
        view.floatData = hull->GetVertexData();
        view.vertexCount = hull->NumVertices();
        view.layoutDesc = StaticVertexInputLayout<VT_Position>;
        return MakeHandle<ConvexHullPhysicsShape>(name, view);
    }
    case PhysicsShapeType::Compound:
    {
        const CompoundPhysicsShape* compound = static_cast<CompoundPhysicsShape*>(source.Get());

        Array<float> positions;
        Array<uint32> indices;

        for (uint32 hullIndex = 0; hullIndex < compound->NumHulls(); hullIndex++)
        {
            positions.Concat(compound->GetHullVertices(hullIndex));
            indices.Concat(compound->GetHullIndices(hullIndex));
        }

        Handle<CompoundPhysicsShape> clonedShape = MakeHandle<CompoundPhysicsShape>(name);
        clonedShape->SetHulls(
            Span<const float>(positions.Data(), positions.Size()),
            Span<const uint32>(indices.Data(), indices.Size()),
            Span<const ConvexHullRange>(compound->GetHulls().Data(), compound->GetHulls().Size()));
        clonedShape->SetDecompositionSettings(compound->GetDecompositionSettings());

        return clonedShape;
    }
    default:
        return nullptr;
    }
}

Entity* EditorSubsystem::ResolveCollisionTargetEntity(Node* node) const
{
    if (!m_currentProject.IsValid() || IsSimulating())
    {
        return nullptr;
    }

    if (node == nullptr)
    {
        Handle<Node> focusedNode = m_focusedNode.Lock();

        return DynamicCast<Entity>(focusedNode.Get());
    }

    return DynamicCast<Entity>(node);
}

bool EditorSubsystem::CanFitPhysicsShapeToMesh(Node* node) const
{
    Entity* entity = ResolveCollisionTargetEntity(node);

    if (entity == nullptr)
    {
        return false;
    }

    MeshComponent* meshComponent = entity->TryGetComponent<MeshComponent>();
    RigidBodyComponent* rigidBodyComponent = entity->TryGetComponent<RigidBodyComponent>();

    return meshComponent != nullptr
        && meshComponent->mesh.IsValid()
        && rigidBodyComponent != nullptr
        && DynamicCast<BoxPhysicsShape>(rigidBodyComponent->shape).IsValid();
}

bool EditorSubsystem::IsPhysicsShapeShared(Entity* entity, const Handle<PhysicsShape>& shape) const
{
    if (!shape.IsValid() || !m_currentProject.IsValid())
    {
        return false;
    }

    for (Scene* scene : GetCurrentProject()->GetWorld()->GetScenes())
    {
        for (auto [otherEntity, otherRigidBodyComponent, otherTransformComponent] : scene->GetEntityManager()->GetEntitySet<RigidBodyComponent, TransformComponent>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            (void)otherTransformComponent;

            if (otherEntity != entity && otherRigidBodyComponent.shape == shape)
            {
                return true;
            }
        }
    }

    return false;
}

Handle<PhysicsShape> EditorSubsystem::EnsureUniquePhysicsShape(Entity* entity)
{
    RigidBodyComponent* rigidBodyComponent = entity->TryGetComponent<RigidBodyComponent>();

    if (rigidBodyComponent == nullptr)
    {
        return nullptr;
    }

    Handle<PhysicsShape> shape = rigidBodyComponent->shape;

    if (!shape.IsValid() || !IsPhysicsShapeShared(entity, shape))
    {
        return shape;
    }

    // The shape is shared with at least one other entity; clone it so this entity gets its own copy
    // that we can mutate without affecting the others.
    Handle<PhysicsShape> clone = ClonePhysicsShape(shape);
    clone->SetName(NAME_FMT("{}_{}_PhysicsShape", entity->GetName(), entity->Id().Value()));
    GetCurrentAssetRegistry()->PutAsset(clone);

    rigidBodyComponent->shape = clone;
    entity->AddTag<EntityTag::UpdatePhysicsShape>();

    return clone;
}

void EditorSubsystem::FitPhysicsShapeToMesh(Node* node)
{
    Handle<EditorProject> project = GetCurrentProject();
    if (!project.IsValid())
    {
        return;
    }

    Entity* entity = ResolveCollisionTargetEntity(node);

    if (entity == nullptr)
    {
        return;
    }

    MeshComponent* meshComponent = entity->TryGetComponent<MeshComponent>();
    RigidBodyComponent* rigidBodyComponent = entity->TryGetComponent<RigidBodyComponent>();

    if (meshComponent == nullptr || !meshComponent->mesh.IsValid() || rigidBodyComponent == nullptr)
    {
        return;
    }

    if (!rigidBodyComponent->shape.IsValid() || rigidBodyComponent->shape->GetType() != PhysicsShapeType::Box)
    {
        return;
    }

    // Clone the shape first if any other entity references it, so we never mutate a shared asset.
    Handle<PhysicsShape> uniqueShape = EnsureUniquePhysicsShape(entity);
    Handle<BoxPhysicsShape> boxShape = DynamicCast<BoxPhysicsShape>(uniqueShape);

    if (!boxShape.IsValid())
    {
        return;
    }

    // The box shape lives in the entity's local space, so matching the mesh's local AABB aligns it
    // with the rendered geometry regardless of the entity's world transform.
    const BoundingBox meshAabb = meshComponent->mesh->GetAABB();
    const BoundingBox oldAabb = boxShape->GetAABB();

    project->GetActionStack()->PushAction(MakeHandle<FunctionalEditorAction>(
        "Fit Physics Shape to Mesh",
        [boxShape, entity = MakeStrongRef(entity), meshAabb, oldAabb]() -> EditorActionFunctions
        {
            return {
                [boxShape, entity, meshAabb](EditorSubsystem*, EditorProject*)
                {
                    if (boxShape.IsValid())
                    {
                        boxShape->SetAABB(meshAabb);

                        boxShape->Invalidate();
                    }

                    if (entity.IsValid())
                    {
                        entity->AddTag<EntityTag::UpdatePhysicsShape>();
                    }
                },
                [boxShape, entity, oldAabb](EditorSubsystem*, EditorProject*)
                {
                    if (boxShape.IsValid())
                    {
                        boxShape->SetAABB(oldAabb);
                        boxShape->Invalidate();
                    }

                    if (entity.IsValid())
                    {
                        entity->AddTag<EntityTag::UpdatePhysicsShape>();
                    }
                }
            };
        }));
}

bool EditorSubsystem::CanGenerateConvexCollision(Node* node) const
{
    if (!IsConvexDecompositionSupported())
    {
        return false;
    }

    Entity* entity = ResolveCollisionTargetEntity(node);

    if (entity == nullptr)
    {
        return false;
    }

    MeshComponent* meshComponent = entity->TryGetComponent<MeshComponent>();
    RigidBodyComponent* rigidBodyComponent = entity->TryGetComponent<RigidBodyComponent>();

    return meshComponent != nullptr
        && meshComponent->mesh.IsValid()
        && rigidBodyComponent != nullptr;
}

void EditorSubsystem::GenerateConvexCollision(Node* node)
{
    if (!CanGenerateConvexCollision(node))
    {
        return;
    }

    Entity* entity = ResolveCollisionTargetEntity(node);
    Assert(entity != nullptr);

    MeshComponent* meshComponent = entity->TryGetComponent<MeshComponent>();
    Handle<Mesh> mesh = meshComponent->mesh;

    // The shape carries these settings from here on, so they can be tuned and regenerated in the inspector.
    const ConvexDecompositionSettings settings = ConvexDecompositionSettings {};

    EditorTaskScope* editorTaskScope = new EditorTaskScope(
        TickableEditorTask::StaticClass(),
        []()
        { /* no tick function */ },
        "Generating Convex Collision",
        mesh->GetName().ToString(),
        /* isForegroundTask */ true);

    TaskSystem::GetInstance().Enqueue(
        [editorTaskScope, mesh, entityRef = MakeStrongRef(entity), settings]()
        {
            TResult<ConvexDecompositionResult> decompositionResult = DecomposeMesh(mesh.Get(), settings);

            if (decompositionResult.HasError())
            {
                HYP_LOG(Editor, Error, "Failed to generate convex collision for {}: {}",
                    mesh->GetName(), decompositionResult.GetError().GetMessage());

                delete editorTaskScope;

                return;
            }

            GetThreadById(g_simThread)->GetScheduler().Enqueue(
                [mesh, entityRef, settings, result = std::move(decompositionResult.GetValue())]()
                {
                    Handle<EditorSubsystem> subsystem = g_editorState ? g_editorState->GetEditorSubsystem() : Handle<EditorSubsystem>();

                    if (!entityRef.IsValid() || !subsystem.IsValid() || !subsystem->GetCurrentProject().IsValid())
                    {
                        return;
                    }

                    RigidBodyComponent* rigidBodyComponent = entityRef->TryGetComponent<RigidBodyComponent>();

                    if (rigidBodyComponent == nullptr)
                    {
                        return;
                    }

                    Handle<CompoundPhysicsShape> compoundShape = MakeHandle<CompoundPhysicsShape>(NAME_FMT("{}_Collision", mesh->GetName()));
                    compoundShape->SetHulls(
                        Span<const float>(result.positions.Data(), result.positions.Size()),
                        Span<const uint32>(result.indices.Data(), result.indices.Size()),
                        Span<const ConvexHullRange>(result.hulls.Data(), result.hulls.Size()));
                    compoundShape->SetDecompositionSettings(settings);

                    {
                        auto readScope = mesh->GetReadScope();

                        compoundShape->SetSource(mesh, mesh->ComputeLod0DataHash());
                    }

                    GetCurrentAssetRegistry()->PutAssetUnique(compoundShape);

                    Handle<PhysicsShape> previousShape = rigidBodyComponent->shape;

                    subsystem->GetCurrentProject()->GetActionStack()->PushAction(MakeHandle<FunctionalEditorAction>(
                        "Generate Convex Collision",
                        [entityRef, compoundShape, previousShape]() -> EditorActionFunctions
                        {
                            return {
                                [entityRef, compoundShape](EditorSubsystem*, EditorProject*)
                                {
                                    if (RigidBodyComponent* rigidBodyComponent = entityRef.IsValid() ? entityRef->TryGetComponent<RigidBodyComponent>() : nullptr)
                                    {
                                        rigidBodyComponent->shape = compoundShape;
                                        entityRef->AddTag<EntityTag::UpdatePhysicsShape>();
                                    }
                                },
                                [entityRef, previousShape](EditorSubsystem*, EditorProject*)
                                {
                                    if (RigidBodyComponent* rigidBodyComponent = entityRef.IsValid() ? entityRef->TryGetComponent<RigidBodyComponent>() : nullptr)
                                    {
                                        rigidBodyComponent->shape = previousShape;
                                        entityRef->AddTag<EntityTag::UpdatePhysicsShape>();
                                    }
                                }
                            };
                        }));
                },
                TaskEnqueueFlags::FIRE_AND_FORGET);

            delete editorTaskScope;
        },
        TaskThreadPoolName::THREAD_POOL_BACKGROUND,
        TaskEnqueueFlags::FIRE_AND_FORGET);
}

String EditorSubsystem::GetSourcePrefabName(Node* node) const
{
    const UUID prefabUUID = Prefab::GetSourcePrefabUUID(node);

    if (prefabUUID == UUID::Invalid())
    {
        return String::empty;
    }

    Handle<Prefab> prefab = Prefab::FindByUUID(prefabUUID);

    if (!prefab.IsValid())
    {
        return String::empty;
    }

    return prefab->GetName().ToString();
}

Array<Name> EditorSubsystem::GetTemplateNames() const
{
    return EditorTemplateLibrary::GetTemplateNames();
}

bool EditorSubsystem::HasTemplate(Name templateName) const
{
    return EditorTemplateLibrary::HasTemplate(templateName);
}

String EditorSubsystem::GetTemplatesDirectory() const
{
    return EditorTemplateLibrary::GetDirectory();
}

int32 EditorSubsystem::GetViewportForcedLod() const
{
    return g_cvMeshLodForceLod.Get();
}

void EditorSubsystem::SetViewportForcedLod(int32 lodIndex)
{
    g_cvMeshLodForceLod.Set(MathUtil::Clamp(lodIndex, -1, int32(MaxMeshLods) - 1));
}

static VolumeBase* ResolveFittableVolume(Node* node)
{
    VolumeBase* volume = DynamicCast<VolumeBase>(node);

    // unbounded volumes (e.g. sky probe) have no box to fit
    if (volume == nullptr || !volume->GetLocalBounds().IsFinite())
    {
        return nullptr;
    }

    return volume;
}

static BoundingBox CalculateSelectionWorldBounds(Span<const Handle<Node>> selectedNodes, const Node* excludedNode)
{
    BoundingBox selectionBounds = BoundingBox::Empty();

    for (const Handle<Node>& node : selectedNodes)
    {
        if (!node.IsValid() || node.Get() == excludedNode)
        {
            continue;
        }

        const BoundingBox nodeBounds = node->GetWorldBounds();

        if (nodeBounds.IsValid() && nodeBounds.IsFinite() && !nodeBounds.IsZero())
        {
            selectionBounds = selectionBounds.Union(nodeBounds);
        }
    }

    return selectionBounds;
}

static void ApplyVolumeLocalBounds(EditorSubsystem* editorSubsystem, const Handle<VolumeBase>& volume, const BoundingBox& localBounds)
{
    if (!volume.IsValid())
    {
        return;
    }

    volume->SetLocalBounds(localBounds);

    // the volume edit gizmo caches the face positions of the volume it's attached to
    if (Handle<Node> focusedNode = editorSubsystem->GetFocusedNode(); focusedNode.Get() == volume.Get())
    {
        if (EditorGizmoBase* gizmo = editorSubsystem->GetSelectedGizmo())
        {
            gizmo->SetFocusedNode(focusedNode);
        }
    }
}

bool EditorSubsystem::CanFitVolumeToSelection(Node* volume) const
{
    AssertOnThread(g_simThread);

    if (!m_currentProject.IsValid() || IsSimulating() || ResolveFittableVolume(volume) == nullptr)
    {
        return false;
    }

    return CalculateSelectionWorldBounds(GetSelectedNodes(), volume).IsValid();
}

void EditorSubsystem::FitVolumeToSelection(Node* volume)
{
    AssertOnThread(g_simThread);

    Handle<EditorProject> project = GetCurrentProject();

    if (!project.IsValid() || IsSimulating())
    {
        return;
    }

    VolumeBase* fittableVolume = ResolveFittableVolume(volume);

    if (fittableVolume == nullptr)
    {
        return;
    }

    const BoundingBox selectionBounds = CalculateSelectionWorldBounds(GetSelectedNodes(), fittableVolume);

    if (!selectionBounds.IsValid())
    {
        HYP_LOG(Editor, Warning, "Fit volume to selection: no other selected node has finite bounds");

        return;
    }

    const BoundingBox fittedLocalBounds = fittableVolume->GetWorldMatrix().Inverse() * selectionBounds;
    const BoundingBox previousLocalBounds = fittableVolume->GetLocalBounds();

    project->GetActionStack()->PushAction(MakeHandle<FunctionalEditorAction>(
        "Fit Volume to Selection",
        [volumeRef = MakeStrongRef(fittableVolume), fittedLocalBounds, previousLocalBounds]() -> EditorActionFunctions
        {
            return {
                [volumeRef, fittedLocalBounds](EditorSubsystem* editorSubsystem, EditorProject*)
                {
                    ApplyVolumeLocalBounds(editorSubsystem, volumeRef, fittedLocalBounds);
                },
                [volumeRef, previousLocalBounds](EditorSubsystem* editorSubsystem, EditorProject*)
                {
                    ApplyVolumeLocalBounds(editorSubsystem, volumeRef, previousLocalBounds);
                }
            };
        }));
}

// Which LOD each mesh is rendering, without needing a shader path for it.
void EditorSubsystem::DebugDrawMeshLods(DebugDrawCommandList& debugDrawCommandList)
{
    if (!s_cvShowMeshLods.Get() || !m_currentProject.IsValid())
    {
        return;
    }

    static const RenderableAttributeSet wireframeAttributes = PhysicsWireframeAttributes();

    static const Color lodColors[] = {
        Color(0.3f, 1.0f, 0.4f, 1.0f),
        Color(1.0f, 0.9f, 0.3f, 1.0f),
        Color(1.0f, 0.5f, 0.2f, 1.0f),
        Color(1.0f, 0.25f, 0.25f, 1.0f)
    };

    World* world = GetCurrentProject()->GetWorld();

    Array<LODViewData, SceneTempAllocator> viewDatas;
    world->CollectLODViewDatas(viewDatas);

    for (Scene* scene : world->GetScenes())
    {
        for (auto [entity, meshComponent, boundingBoxComponent] : scene->GetEntityManager()->GetEntitySet<MeshComponent, BoundingBoxComponent>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            if (!meshComponent.mesh.IsValid() || meshComponent.mesh->GetMeshDesc().GetNumLods() <= 1)
            {
                continue;
            }

            const BoundingSphere boundingSphere { boundingBoxComponent.worldAabb };

            if (boundingSphere.radius <= 0.0f)
            {
                continue;
            }

            const uint8 selectedLod = SelectMeshLodForViews(viewDatas.ToSpan(), meshComponent, boundingBoxComponent.worldAabb);

            debugDrawCommandList.sphere(
                boundingSphere.center,
                boundingSphere.radius,
                lodColors[MathUtil::Min(selectedLod, uint8(GetArrayCount(lodColors) - 1))],
                wireframeAttributes);
        }
    }
}

// Hulls of one decomposition are easier to tell apart when each gets its own colour.
static Color HullDebugColor(uint32 hullIndex)
{
    static const Color hullColors[] = {
        Color(1.0f, 0.85f, 0.2f, 1.0f),
        Color(0.3f, 0.9f, 1.0f, 1.0f),
        Color(1.0f, 0.4f, 0.7f, 1.0f),
        Color(0.5f, 1.0f, 0.4f, 1.0f),
        Color(1.0f, 0.6f, 0.25f, 1.0f),
        Color(0.7f, 0.6f, 1.0f, 1.0f)
    };

    return hullColors[hullIndex % GetArrayCount(hullColors)];
}

void EditorSubsystem::DebugDrawPhysicsShapes(DebugDrawCommandList& debugDrawCommandList)
{
    if (!s_cvDebugDrawPhysics.Get())
    {
        return;
    }

    static const RenderableAttributeSet wireframeAttributes = PhysicsWireframeAttributes();

    if (!m_currentProject.IsValid())
    {
        return;
    }

    static constexpr auto BoxPhysicsShapeTypeId = CONSTEXPR_TYPE_ID(BoxPhysicsShape);
    static constexpr auto SpherePhysicsShapeTypeId = CONSTEXPR_TYPE_ID(SpherePhysicsShape);
    static constexpr auto PlanePhysicsShapeTypeId = CONSTEXPR_TYPE_ID(PlanePhysicsShape);
    static constexpr auto CapsulePhysicsShapeTypeId = CONSTEXPR_TYPE_ID(CapsulePhysicsShape);
    static constexpr auto ConvexHullPhysicsShapeTypeId = CONSTEXPR_TYPE_ID(ConvexHullPhysicsShape);
    static constexpr auto CompoundPhysicsShapeTypeId = CONSTEXPR_TYPE_ID(CompoundPhysicsShape);

    static constexpr float PlaneDebugHalfExtent = 5.0f;

    for (Scene* scene : GetCurrentProject()->GetWorld()->GetScenes())
    {
        for (auto [entity, rigidBodyComponent, transformComponent] : scene->GetEntityManager()->GetEntitySet<RigidBodyComponent, TransformComponent>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            (void)transformComponent;

            PhysicsShape* shape = rigidBodyComponent.shape.Get();

            if (!shape)
            {
                continue;
            }

            const bool selected = IsNodeSelected(MakeStrongRef(static_cast<Node*>(entity)));
            const Color color = selected ? Color::Yellow() : Color::Green();

            const Transform entityWorldTransform(entity->GetWorldTranslation(), entity->GetWorldScale(), entity->GetWorldRotation());
            const Mat4f& entityWorldMatrix = entity->GetWorldMatrix();
            const Vec3f entityWorldScale = entity->GetWorldScale();
            const float maxEntityScale = MathUtil::Max(MathUtil::Max(entityWorldScale.x, entityWorldScale.y), entityWorldScale.z);

            switch (shape->InstanceClass()->GetTypeId().Value())
            {
            case BoxPhysicsShapeTypeId:
            {
                const BoundingBox& aabb = static_cast<BoxPhysicsShape*>(shape)->GetAABB();
                const Transform boxWorldTransform = entityWorldTransform * Transform(aabb.GetCenter(), aabb.GetExtent() * 0.5f, Quat4f::Identity());
                debugDrawCommandList.box(boxWorldTransform, color, wireframeAttributes);
                break;
            }
            case SpherePhysicsShapeTypeId:
            {
                const BoundingSphere& sphere = static_cast<SpherePhysicsShape*>(shape)->GetSphere();
                const Vec3f worldCenter = entityWorldMatrix.TransformVector(Vec4f(sphere.GetCenter(), 1.0f)).GetXYZ();
                debugDrawCommandList.sphere(worldCenter, sphere.GetRadius() * maxEntityScale, color, wireframeAttributes);
                break;
            }
            case CapsulePhysicsShapeTypeId:
            {
                const float radius = static_cast<CapsulePhysicsShape*>(shape)->GetRadius();
                const float height = static_cast<CapsulePhysicsShape*>(shape)->GetHeight(); // cylindrical part (Bullet convention)
                // Capsule is Y-axis aligned in local space. Unit cylinder mesh has radius 1 and height 1.
                const Transform cylinderWorldTransform = entityWorldTransform * Transform(Vec3f::Zero(), Vec3f(radius, height, radius), Quat4f::Identity());
                debugDrawCommandList.cylinder(cylinderWorldTransform, color, wireframeAttributes);
                const float worldRadius = radius * maxEntityScale;
                const Vec3f topWorld = entityWorldMatrix.TransformVector(Vec4f(Vec3f(0.0f, height * 0.5f, 0.0f), 1.0f)).GetXYZ();
                const Vec3f bottomWorld = entityWorldMatrix.TransformVector(Vec4f(Vec3f(0.0f, -height * 0.5f, 0.0f), 1.0f)).GetXYZ();
                debugDrawCommandList.sphere(topWorld, worldRadius, color, wireframeAttributes);
                debugDrawCommandList.sphere(bottomWorld, worldRadius, color, wireframeAttributes);
                break;
            }
            case PlanePhysicsShapeTypeId:
            {
                // Planes are infinite; draw a finite wireframe quad at the entity origin oriented to the plane normal.
                const Vec4f& plane = static_cast<PlanePhysicsShape*>(shape)->GetPlane();
                Vec3f normal(plane.x, plane.y, plane.z);
                if (normal.Length() < MathUtil::epsilonF)
                {
                    normal = Vec3f::UnitY();
                }
                else
                {
                    normal.Normalize();
                }
                const Vec3f reference = MathUtil::Abs(normal.y) < 0.99f ? Vec3f::UnitY() : Vec3f::UnitX();
                const Vec3f tangent = (reference - normal * normal.Dot(reference)).Normalize();
                const Vec3f bitangent = normal.Cross(tangent).Normalize();
                const float e = PlaneDebugHalfExtent;
                const Vec3f c0 = (-tangent - bitangent) * e;
                const Vec3f c1 = ( tangent - bitangent) * e;
                const Vec3f c2 = ( tangent + bitangent) * e;
                const Vec3f c3 = (-tangent + bitangent) * e;
                const Mat4f& m = entityWorldMatrix;
                const FixedArray<Vec3f, 4> worldCorners = {
                    m.TransformVector(Vec4f(c0, 1.0f)).GetXYZ(),
                    m.TransformVector(Vec4f(c1, 1.0f)).GetXYZ(),
                    m.TransformVector(Vec4f(c2, 1.0f)).GetXYZ(),
                    m.TransformVector(Vec4f(c3, 1.0f)).GetXYZ()
                };
                debugDrawCommandList.plane(worldCorners, color, wireframeAttributes);
                break;
            }
            case ConvexHullPhysicsShapeTypeId:
            {
                // Without recomputing the hull, render the local AABB of the hull points as an oriented box.
                const ConvexHullPhysicsShape* hull = static_cast<ConvexHullPhysicsShape*>(shape);
                const size_t numVertices = hull->NumVertices();
                const float* vertexData = hull->GetVertexData();

                if (numVertices == 0 || vertexData == nullptr)
                {
                    break;
                }

                BoundingBox hullAabb(Vec3f(vertexData[0], vertexData[1], vertexData[2]), Vec3f(vertexData[0], vertexData[1], vertexData[2]));
                for (size_t i = 1; i < numVertices; i++)
                {
                    const Vec3f v(vertexData[i * 3 + 0], vertexData[i * 3 + 1], vertexData[i * 3 + 2]);
                    hullAabb = hullAabb.Union(v);
                }

                const Transform hullWorldTransform = entityWorldTransform * Transform(hullAabb.GetCenter(), hullAabb.GetExtent() * 0.5f, Quat4f::Identity());
                debugDrawCommandList.box(hullWorldTransform, color, wireframeAttributes);
                break;
            }
            case CompoundPhysicsShapeTypeId:
            {
                const CompoundPhysicsShape* compoundShape = static_cast<CompoundPhysicsShape*>(shape);

                for (uint32 hullIndex = 0; hullIndex < compoundShape->NumHulls(); hullIndex++)
                {
                    const Span<const float> hullVertices = compoundShape->GetHullVertices(hullIndex);

                    if (hullVertices.Size() < 3 * 3)
                    {
                        continue;
                    }

                    // one debug entry per triangle adds up fast, so only the selected entity gets real hulls
                    if (!selected)
                    {
                        BoundingBox hullAabb = BoundingBox::Empty();

                        for (size_t vertexIndex = 0; vertexIndex + 2 < hullVertices.Size(); vertexIndex += 3)
                        {
                            hullAabb = hullAabb.Union(Vec3f(hullVertices[vertexIndex], hullVertices[vertexIndex + 1], hullVertices[vertexIndex + 2]));
                        }

                        const Transform hullWorldTransform = entityWorldTransform * Transform(hullAabb.GetCenter(), hullAabb.GetExtent() * 0.5f, Quat4f::Identity());
                        debugDrawCommandList.box(hullWorldTransform, color, wireframeAttributes);

                        continue;
                    }

                    const Span<const uint32> hullIndices = compoundShape->GetHullIndices(hullIndex);
                    const Color hullColor = HullDebugColor(hullIndex);

                    for (size_t index = 0; index + 2 < hullIndices.Size(); index += 3)
                    {
                        const uint32 i0 = hullIndices[index] * 3;
                        const uint32 i1 = hullIndices[index + 1] * 3;
                        const uint32 i2 = hullIndices[index + 2] * 3;

                        if (i0 + 2 >= hullVertices.Size() || i1 + 2 >= hullVertices.Size() || i2 + 2 >= hullVertices.Size())
                        {
                            continue;
                        }

                        const Vec3f v0 = entityWorldMatrix.TransformVector(Vec4f(Vec3f(hullVertices[i0], hullVertices[i0 + 1], hullVertices[i0 + 2]), 1.0f)).GetXYZ();
                        const Vec3f v1 = entityWorldMatrix.TransformVector(Vec4f(Vec3f(hullVertices[i1], hullVertices[i1 + 1], hullVertices[i1 + 2]), 1.0f)).GetXYZ();
                        const Vec3f v2 = entityWorldMatrix.TransformVector(Vec4f(Vec3f(hullVertices[i2], hullVertices[i2 + 1], hullVertices[i2 + 2]), 1.0f)).GetXYZ();

                        debugDrawCommandList.triangle(v0, v1, v2, hullColor, wireframeAttributes);
                    }
                }

                break;
            }
            default:
                break;
            }
        }
    }
}

void EditorSubsystem::Update(float delta)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    m_editorDelegates->Update();

    if (g_appContext.IsValid() && g_appContext->GetMainWindow() != nullptr)
    {
        const EnumFlags<MouseButtonState> buttonStates = g_appContext->GetMainWindow()->GetInputManager()->GetButtonStates();

        if (!(buttonStates & (MouseButtonState::LEFT | MouseButtonState::RIGHT)))
        {
            if (EditorGizmoBase* gizmo = GetSelectedGizmo(); gizmo != nullptr && gizmo->IsDragging())
            {
                EditorViewport* activeViewport = GetActiveViewport();

                gizmo->OnDragEnd(activeViewport != nullptr ? activeViewport->GetCamera() : Handle<Camera>(), MouseEvent {});
            }

            if (IsMeshEditDragActive())
            {
                EndMeshEditDrag(/* saveEdits */ true);
            }
        }
    }

    m_gizmoController->UpdateGizmoScreenSize();

    UpdateGizmoProximityVisibility();

    GetTerrainState()->Update();
    GetDecalPainterState()->Update();
    UpdateBakeStatus();
    UpdatePlayNetState();

    if (m_thumbnailService)
    {
        m_thumbnailService->Update();
    }

    if (m_materialPreviewRenderer)
    {
        m_materialPreviewRenderer->Update();
    }

    DebugDrawCommandList& dbg = DebugDrawer::GetInstance().CreateCommandList();

    DebugDrawMeshEditSelection(dbg);
    DebugDrawPhysicsShapes(dbg);
    DebugDrawMeshLods(dbg);
    GetTerrainState()->DebugDrawCursor(dbg);
    GetDecalPainterState()->DebugDrawCursor(dbg);

    if (m_currentProject.IsValid())
    {
        const Handle<World>& world = m_currentProject->GetWorld();

        // World might be invalid if simulation is starting and the project is loading.
        if (world.IsValid() && s_cvDebugDrawProbes.Get())
        {
            for (Scene* scene : world->GetScenes())
            {
                for (auto [probe] : scene->GetEntityManager()->GetEntitySet<EntityType<EnvProbe>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
                {
                    static constexpr auto ReflectionProbeTypeId = CONSTEXPR_TYPE_ID(ReflectionProbe);
                    static constexpr auto SkyProbeTypeId = CONSTEXPR_TYPE_ID(SkyProbe);
                    static constexpr auto IrradianceProbeTypeId = CONSTEXPR_TYPE_ID(IrradianceProbe);

                    switch (probe->InstanceClass()->GetTypeId().Value())
                    {
                    case ReflectionProbeTypeId:
                        dbg.reflectionProbe(probe->GetWorldTranslation(), 1.0f, *probe);
                        break;
                    case SkyProbeTypeId:
                        // dbg.reflectionProbe(probe->GetWorldTranslation(), 1.0f, *probe);
                        break;
                    case IrradianceProbeTypeId:
                        dbg.ambientProbe(probe->GetWorldTranslation(), 1.0f, *probe);
                        break;
                    default:
                        HYP_LOG_ONCE(Editor, Warning, "Unknown probe type class: {}", probe->InstanceClass()->GetName());
                        break;
                    }
                }
            }
        }
    }

    if (!m_selectedNodes.Empty())
    {
        for (const Handle<Node>& node : m_selectedNodes)
        {
            if (node.IsValid())
            {
                const BoundingBox worldBounds = node->GetWorldBounds();

                if (worldBounds.IsFinite())
                {
                    dbg.box(worldBounds.GetCenter(), worldBounds.GetExtent() * 0.5f + Vec3f(FLT_EPSILON), Color::Cyan());
                }
            }
        }
    }

    DebugDrawAssetDropTarget(dbg);

    RenderProxyList& pickRpl = g_editorState->GetPickCache().GetRenderProxyList();
    pickRpl.GetMeshes().Advance();

    if (m_currentProject.IsValid())
    {
        for (const Handle<EditorViewport>& vp : m_editorViewports)
        {
            const Handle<View>& view = vp->GetView();
            AssertDebug(view != nullptr);

            if (!view)
            {
                continue;
            }

            if (!(view->GetViewDesc().flags & ViewFlags::GBUFFER))
            {
                continue; // skip non-primary views
            }

            for (Mesh* mesh : view->GetRenderProxyList(GetRingIndex())->GetMeshes())
            {
                pickRpl.GetMeshes().Track(mesh->Id(), mesh);
            }
        }

        /// @TODO : Prioritize meshes based on distance from camera
        for (Mesh* mesh : pickRpl.GetMeshes())
        {
            g_editorState->GetPickCache().PutEntry(mesh);
        }
    }
}

void EditorSubsystem::OnSceneAttached(const Handle<Scene>& scene)
{
}

void EditorSubsystem::OnSceneDetached(Scene* scene)
{
}

bool EditorSubsystem::StartSimulation()
{
    if (!m_currentProject.IsValid())
    {
        return false;
    }

    // Save the edits to meshes before simulating.
    ExitMeshEditMode(/* saveEdits */ true);

    // The terrain tools edit the source world, not the snapshot that simulation runs against.
    if (m_terrainSculpting.IsValid())
    {
        m_terrainSculpting->SetEnabled(false);
    }

    if (m_decalPainter.IsValid())
    {
        m_decalPainter->SetEnabled(false);
    }

    const GameState& gameState = m_currentProject->GetGame()->GetGameState();

    const bool isSimulatingOrPaused = gameState.mode == GameStateMode::SIMULATING
        || gameState.mode == GameStateMode::PAUSED;

    if (isSimulatingOrPaused)
    {
        // already simulating, unpause if paused, otherwise do nothing.
        if (gameState.mode == GameStateMode::PAUSED)
        {
            m_currentProject->GetGame()->StartSimulating();
        }

        return true;
    }

    const EditorPlayNetMode netMode = m_playNetState.m_playNetMode;

    if (netMode == EditorPlayNetMode::Client && g_gameClient == nullptr)
    {
        HYP_LOG(Editor, Error, "Cannot Play As Client: no GameClient instance exists in this process");

        return false;
    }

    bool isServerStarted = false;
    bool isSimulationStarted = false;

    HYP_DEFER({
        if (isServerStarted && !isSimulationStarted)
        {
            g_gameServer->Stop();
        }
    });

    if (netMode == EditorPlayNetMode::DedicatedServer)
    {
        if (g_gameServer == nullptr)
        {
            HYP_LOG(Editor, Error, "Cannot Play As Dedicated Server: no GameServer instance exists in this process");

            return false;
        }

        // Must be listening before the snapshot world initializes, PlayerSystem binds to the server as it's added
        if (Result listenResult = g_gameServer->Start(uint16(m_playNetState.m_playNetPort)); listenResult.HasError())
        {
            HYP_LOG(Editor, Error, "Play As Dedicated Server: could not listen on port {}: {}", m_playNetState.m_playNetPort, listenResult.GetError().GetMessage());

            return false;
        }

        isServerStarted = true;
    }

    // Save the current project state as a snapshot to restore from when simulation ends.
    if (Result saveResult = m_currentProject->Save(); saveResult.HasError())
    {
        HYP_LOG(Editor, Error, "Failed to save project snapshot before simulation: {}", saveResult.GetError().GetMessage());

        return false;
    }

    m_preSimulationProject = m_currentProject;
    m_simulationSnapshotPath = m_preSimulationProject->GetFilePath();

    // Keep the world alive, we need it to persist otherwise we'd need to load it again and undo/redo history would be lost.
    CloseProject(/* shutdownWorld */ false);

    FilePath snapshotPath = std::move(m_simulationSnapshotPath);

    TResult<Handle<EditorProject>> loadResult = EditorProject::Load(snapshotPath);

    if (loadResult.HasError())
    {
        HYP_LOG(Editor, Error, "Failed to load project when starting simulation!! Error was: {}", loadResult.GetError().GetMessage());

        // reopen the editing project, otherwise IsSimulating() stays true with no project and Stop can never run
        OpenProject(m_preSimulationProject);
        m_preSimulationProject.Reset();

        return false;
    }

    if (netMode == EditorPlayNetMode::DedicatedServer)
    {
        (*loadResult)->GetGame()->SetIsServerGame(true);
    }

    // Open project to start simulating
    OpenProject(*loadResult);

    Game* gameInstance = m_currentProject->GetGame();
    Assert(gameInstance != nullptr);

    Assert(gameInstance->GetWorld().IsValid());
    Assert(m_currentProject.IsValid() && m_currentProject->GetWorld().IsValid());
    
#if 0
    Camera* primaryCamera = nullptr;

    for (Scene* scene : m_currentProject->GetWorld()->GetScenes())
    {
        if (!scene)
        {
            continue;
        }

        if ((scene->GetSceneFlags() & (SceneFlags::FOREGROUND | SceneFlags::UI | SceneFlags::DETACHED)) != SceneFlags::FOREGROUND)
        {
            continue;
        }

        if (Camera* camera = scene->GetPrimaryCamera())
        {
            primaryCamera = camera;
            break;
        }
    }

    if (primaryCamera)
    {
        ViewDesc viewDesc {};
        viewDesc.flags = ViewFlags::DEFAULT | ViewFlags::GBUFFER | ViewFlags::MATCH_CAMERA_DIMENSIONS;
        viewDesc.framebufferDesc.extent = Vec2u(primaryCamera->GetDimensions());
        viewDesc.camera = primaryCamera;

        m_simulationView = MakeHandle<View>(viewDesc);
        m_simulationView->SetName(NAME("SimulationView"));
        InitObject(m_simulationView);

        for (Scene* scene : m_currentProject->GetWorld()->GetScenes())
        {
            if (!scene)
            {
                continue;
            }

            if ((scene->GetSceneFlags() & (SceneFlags::FOREGROUND | SceneFlags::UI | SceneFlags::DETACHED)) != SceneFlags::FOREGROUND)
            {
                continue;
            }

            m_simulationView->AddScene(scene);
        }

        m_currentProject->GetWorld()->AddView(m_simulationView);
    }
    else
    {
        HYP_LOG(Editor, Warning, "no primary camera found!");

        // Show a messagebox since this can be really annoying and frustrating if this happens
        // should make it easier to narrow down the issue at least.

        SystemMessageBox(MessageBoxType::WARNING)
            .Title("No primary camera found")
            .Text("No primary camera was found in any foreground scene. Simulation requires a primary camera in order to properly visualize the scene, without this you will just see a blank / not updating screen in your viewport. Ensure a camera exists with the PrimaryCamera EntityTag set!")
            .Show();
    }
#endif

    gameInstance->StartSimulating();

    if (UISubsystem* uiSubsystem = Subsystem::GetWorld()->GetSubsystem<UISubsystem>())
    {
        uiSubsystem->SetDebugOverlaysSuppressed(true);
    }

    // Keep simulating at full rate while another window (e.g. a connected client) has focus.
    // Pushed on the render thread since that's where the throttling is checked and global contexts are per-thread.
    GetThreadById(g_renderThread)->GetScheduler().Enqueue(
        []()
        {
            PushGlobalContext(SuppressIdleThrottlingContext {});
        },
        TaskEnqueueFlags::FIRE_AND_FORGET);

    m_playNetState.m_activeNetMode = netMode;

    isSimulationStarted = true;

    if (m_playNetState.m_activeNetMode != EditorPlayNetMode::Standalone
        && !(m_currentProject->GetWorld()->GetWorldFlags() & WorldFlags::IsReplicated))
    {
        HYP_LOG(Editor, Warning, "World '{}' is not flagged IsReplicated, so nothing will be replicated in this session", m_currentProject->GetWorld()->GetName());
    }

    if (m_playNetState.m_activeNetMode == EditorPlayNetMode::Client)
    {
        // Connects to an already running server
        ConnectPlayNetClient();
    }
    else if (m_playNetState.m_activeNetMode == EditorPlayNetMode::DedicatedServer)
    {
        HYP_LOG(Editor, Info, "Play As Dedicated Server: listening on port {}", m_playNetState.m_playNetPort);

        SetPlayNetStatus(EditorPlayNetStatus::Hosting);
    }

    return true;
}

bool EditorSubsystem::StopSimulation()
{
    if (m_currentProject.IsValid())
    {
        Game* gameInstance = m_currentProject->GetGame();
        Assert(gameInstance != nullptr);

        AssertOnThread(g_simThread);
        DebugDrawer::GetInstance().DiscardPendingCommands();

        GetThreadById(g_renderThread)->GetScheduler().Enqueue(
            []()
            {
                DebugDrawer::GetInstance().ClearCommands();
            },
            TaskEnqueueFlags::FIRE_AND_FORGET);

        gameInstance->StopSimulating();

        Assert(IsSimulating());

        SceneHelpers::SetGhostModeEnabled(false);

        {
            EditorTaskScope taskScope(
                TickableEditorTask::StaticClass(),
                "Stopping Simulation",
                "Please wait for the world to finish unloading",
                /* isForegroundTask */ true);

            OpenProject(m_preSimulationProject);
        }

        m_preSimulationProject.Reset();

        // Only once the simulation world is gone, so its PlayerSystem / ReplicationSystem no longer use the connection
        if (m_playNetState.m_activeNetMode == EditorPlayNetMode::Client && g_gameClient != nullptr)
        {
            g_gameClient->Disconnect();
        }
        else if (m_playNetState.m_activeNetMode == EditorPlayNetMode::DedicatedServer && g_gameServer != nullptr)
        {
            g_gameServer->Stop();

            HYP_LOG(Editor, Info, "Play As Dedicated Server: stopped listening on port {}", m_playNetState.m_playNetPort);
        }

        m_playNetState.m_activeNetMode = EditorPlayNetMode::Standalone;

        SetPlayNetStatus(EditorPlayNetStatus::None);

        if (UISubsystem* uiSubsystem = Subsystem::GetWorld()->GetSubsystem<UISubsystem>())
        {
            uiSubsystem->SetDebugOverlaysSuppressed(false);
        }

        GetThreadById(g_renderThread)->GetScheduler().Enqueue(
            []()
            {
                if (IsGlobalContextActive<SuppressIdleThrottlingContext>())
                {
                    PopGlobalContext<SuppressIdleThrottlingContext>();
                }
            },
            TaskEnqueueFlags::FIRE_AND_FORGET);

        return true;
    }

    return true;
}

bool EditorSubsystem::PauseSimulation()
{
    if (m_currentProject.IsValid())
    {
        m_currentProject->GetGame()->PauseSimulation();

        return true;
    }

    return false;
}

void EditorSubsystem::LoadPlayNetSettings()
{
    EditorConfig config;

    // A missing file just means nothing has been saved yet
    if (!config.Load())
    {
        return;
    }

    if (const ConfigValue& modeValue = config.Get(PlayNetModeConfigKey); modeValue.IsString())
    {
        const String modeString = modeValue.ToString();

        if (modeString == "Client")
        {
            m_playNetState.m_playNetMode = EditorPlayNetMode::Client;
        }
        else if (modeString == "DedicatedServer")
        {
            m_playNetState.m_playNetMode = EditorPlayNetMode::DedicatedServer;
        }
        else
        {
            m_playNetState.m_playNetMode = EditorPlayNetMode::Standalone;
        }
    }

    if (const ConfigValue& hostValue = config.Get(PlayNetHostConfigKey); hostValue.IsString() && hostValue.ToString().Any())
    {
        m_playNetState.m_playNetHost = hostValue.ToString();
    }

    if (const ConfigValue& portValue = config.Get(PlayNetPortConfigKey); portValue.IsNumber())
    {
        const uint32 port = portValue.ToUInt32();

        if (port > 0 && port <= MathUtil::MaxSafeValue<uint16>())
        {
            m_playNetState.m_playNetPort = port;
        }
    }

    if (const ConfigValue& cachePortValue = config.Get(PlayNetCachePortConfigKey); cachePortValue.IsNumber())
    {
        const uint32 port = cachePortValue.ToUInt32();

        if (port > 0 && port <= MathUtil::MaxSafeValue<uint16>())
        {
            m_playNetState.m_playNetCachePort = port;
        }
    }
}

void EditorSubsystem::SavePlayNetSettings()
{
    EditorConfig config;
    config.Load();

    const char* modeString = "Standalone";

    switch (m_playNetState.m_playNetMode)
    {
    case EditorPlayNetMode::Client:
        modeString = "Client";
        break;
    case EditorPlayNetMode::DedicatedServer:
        modeString = "DedicatedServer";
        break;
    default:
        break;
    }

    config.Set(PlayNetModeConfigKey, ConfigValue(String(modeString)));
    config.Set(PlayNetHostConfigKey, ConfigValue(m_playNetState.m_playNetHost));
    config.Set(PlayNetPortConfigKey, ConfigValue(m_playNetState.m_playNetPort));
    config.Set(PlayNetCachePortConfigKey, ConfigValue(m_playNetState.m_playNetCachePort));

    if (!config.Save())
    {
        HYP_LOG(Editor, Warning, "Failed to save Play In Editor network settings");
    }
}

void EditorSubsystem::SetPlayNetMode(EditorPlayNetMode mode)
{
    if (mode == m_playNetState.m_playNetMode)
    {
        return;
    }

    m_playNetState.m_playNetMode = mode;

    SavePlayNetSettings();
}

void EditorSubsystem::SetPlayNetHost(const String& host)
{
    const String trimmedHost = host.Trimmed();

    if (trimmedHost.Empty() || trimmedHost == m_playNetState.m_playNetHost)
    {
        return;
    }

    m_playNetState.m_playNetHost = trimmedHost;

    SavePlayNetSettings();
}

void EditorSubsystem::SetPlayNetPort(uint32 port)
{
    if (port == 0 || port > MathUtil::MaxSafeValue<uint16>())
    {
        HYP_LOG(Editor, Warning, "Ignoring invalid Play In Editor port {}", port);

        return;
    }

    if (port == m_playNetState.m_playNetPort)
    {
        return;
    }

    m_playNetState.m_playNetPort = port;

    SavePlayNetSettings();
}

void EditorSubsystem::SetPlayNetCachePort(uint32 port)
{
    if (port == 0 || port > MathUtil::MaxSafeValue<uint16>())
    {
        HYP_LOG(Editor, Warning, "Ignoring invalid Play In Editor cache server port {}", port);

        return;
    }

    if (port == m_playNetState.m_playNetCachePort)
    {
        return;
    }

    m_playNetState.m_playNetCachePort = port;

    SavePlayNetSettings();
}

String EditorSubsystem::GetPlayNetProjectDirectory() const
{
    if (!m_preSimulationProject.IsValid())
    {
        return String::empty;
    }

    return m_preSimulationProject->GetFilePath().BasePath();
}

void EditorSubsystem::ConnectPlayNetClient()
{
    Assert(g_gameClient != nullptr);

    HYP_LOG(Editor, Info, "Play As Client: connecting to {}:{}", m_playNetState.m_playNetHost, m_playNetState.m_playNetPort);

    if (Result connectResult = g_gameClient->Connect(m_playNetState.m_playNetHost.ToAnsi(), uint16(m_playNetState.m_playNetPort)); connectResult.HasError())
    {
        HYP_LOG(Editor, Error, "Play As Client: failed to connect to {}:{}: {}", m_playNetState.m_playNetHost, m_playNetState.m_playNetPort, connectResult.GetError().GetMessage());

        SetPlayNetStatus(EditorPlayNetStatus::Failed);

        return;
    }

    SetPlayNetStatus(EditorPlayNetStatus::Connecting);
}

void EditorSubsystem::UpdatePlayNetState()
{
    if (m_playNetState.m_activeNetMode != EditorPlayNetMode::Client || g_gameClient == nullptr)
    {
        return;
    }

    switch (g_gameClient->GetConnectionState())
    {
    case NetClientConnectionState::Connected:
        if (m_playNetState.status != EditorPlayNetStatus::Connected)
        {
            HYP_LOG(Editor, Info, "Play As Client: connected to {}:{}", m_playNetState.m_playNetHost, m_playNetState.m_playNetPort);

            SetPlayNetStatus(EditorPlayNetStatus::Connected);
        }

        break;
    case NetClientConnectionState::Disconnected:
        if (m_playNetState.status == EditorPlayNetStatus::Connecting)
        {
            const Result lastError = g_gameClient->GetLastError();

            HYP_LOG(Editor, Error, "Play As Client: could not connect to {}:{}: {}", m_playNetState.m_playNetHost, m_playNetState.m_playNetPort,
                lastError.HasError() ? lastError.GetError().GetMessage() : "unknown error");

            SetPlayNetStatus(EditorPlayNetStatus::Failed);
        }
        else if (m_playNetState.status == EditorPlayNetStatus::Connected)
        {
            HYP_LOG(Editor, Warning, "Play As Client: lost connection to {}:{}", m_playNetState.m_playNetHost, m_playNetState.m_playNetPort);

            SetPlayNetStatus(EditorPlayNetStatus::Disconnected);
        }

        break;
    default:
        break;
    }
}

void EditorSubsystem::SetPlayNetStatus(EditorPlayNetStatus status)
{
    if (status == m_playNetState.status)
    {
        return;
    }

    m_playNetState.status = status;

    OnPlayNetStatusChanged(status);
}

void EditorSubsystem::InitViewport()
{
    for (const Handle<EditorViewport>& vp : m_editorViewports)
    {
        vp->OnRemoved(this);
    }
    m_editorViewports.Clear();

    UISubsystem* uiSubsystem = Subsystem::GetWorld()->GetSubsystem<UISubsystem>();
    Assert(uiSubsystem != nullptr);

    Handle<UIPanel> backdropPanel = uiSubsystem->GetUIStage()->CreateUIObject<UIPanel>(NAME("Editor_BackdropPanel"), Vec2i::Zero(), UIObjectSize(100, UIObjectSize::PERCENT));
    Assert(backdropPanel != nullptr);

    backdropPanel->SetBackgroundColor(Color::Transparent());
    uiSubsystem->GetUIStage()->AddChildUIObject(backdropPanel);

    uiSubsystem->GetUIStage()->UpdateSize(true);

    backdropPanel->OnClick.RemoveAllFromSet(m_delegateHandlers);
    m_delegateHandlers.Add(backdropPanel->OnClick.Bind(
        backdropPanel.Get(),
        [this](const MouseEvent& event)
        {
            if (m_shouldCancelNextClick)
            {
                return UIEventHandlerResult::STOP_BUBBLING;
            }

            EditorViewport* activeViewport = GetActiveViewport();
            if (!activeViewport)
            {
                return UIEventHandlerResult::OK;
            }

            // if (m_camera->GetCameraController()->GetInputHandler()->OnClick(event))
            // {
            //     return UIEventHandlerResult::STOP_BUBBLING;
            // }

            if (GetTerrainState()->IsEnabled() || GetDecalPainterState()->IsEnabled())
            {
                // Strokes are applied from OnMouseDown / OnMouseDrag / the per-frame update;
                // clicking just shouldn't fall through to scene picking.
                return UIEventHandlerResult::STOP_BUBBLING;
            }

            if (m_meshEditState.enabled)
            {
                const Ray ray = activeViewport->GetCamera()->GetPickRay(event.relativePos);

                MeshEditFaceSelection faceSelection;

                if (TryPickMeshEditFace(ray, faceSelection, /* ensureUniqueMesh */ true))
                {
                    SetSelectedMeshEditFace(faceSelection);
                }
                else
                {
                    SetSelectedMeshEditFace({});
                }

                return UIEventHandlerResult::STOP_BUBBLING;
            }

            if (IsHoveringGizmo())
            {
                return UIEventHandlerResult::STOP_BUBBLING;
            }

            const Ray ray = activeViewport->GetCamera()->GetPickRay(event.relativePos);

            RayTestResults results;

            if (TestPickRay(ray, results))
            {
                for (const RayHit& hit : results)
                {
                    if (hit.node != nullptr)
                    {
                        Handle<Node> nodeStrong = MakeStrongRef(hit.node);

                        bool shouldMutateSelection = false;

                        InputManager* inputManager = g_appContext->GetMainWindow()->GetInputManager();

                        // If CTRL key is down, add/remove from current selection.
                        if (inputManager->IsCtrlDown())
                        {
                            shouldMutateSelection = true;
                        }

                        if (shouldMutateSelection)
                        {
                            // If already in selection, remove, otherwise add
                            if (m_selectedNodes.Contains(nodeStrong))
                            {
                                m_selectedNodes.Erase(nodeStrong);
                                // Don't set focused node if deselecting this node.
                            }
                            else
                            {
                                m_selectedNodes.Add(nodeStrong);
                                SetFocusedNode(nodeStrong, true);
                            }
                        }
                        else
                        {
                            m_selectedNodes = { nodeStrong };
                            SetFocusedNode(nodeStrong, true);
                        }

                        OnSelectionChanged();

                        break;
                    }
                }

                return UIEventHandlerResult::STOP_BUBBLING;
            }

            return UIEventHandlerResult::OK;
        }));

    backdropPanel->OnMouseLeave.RemoveAllFromSet(m_delegateHandlers);
    m_delegateHandlers.Add(backdropPanel->OnMouseLeave.Bind(
        backdropPanel.Get(),
        [this](const MouseEvent& event)
        {
            EditorViewport* activeViewport = GetActiveViewport();
            if (!activeViewport)
            {
                return UIEventHandlerResult::OK;
            }

            if (IsHoveringGizmo())
            {
                SetHoveredGizmo(event, nullptr, Handle<Node>::Null());
            }

            CameraController* controller = activeViewport->GetCamera()->GetCameraController();

            if (controller != nullptr)
            {
                InputHandlerBase* inputHandler = controller->GetInputHandler();

                if (inputHandler != nullptr)
                {
                    inputHandler->OnMouseLeave(event);
                }
            }

            return UIEventHandlerResult::OK;
        }));

    backdropPanel->OnMouseDrag.RemoveAllFromSet(m_delegateHandlers);
    m_delegateHandlers.Add(backdropPanel->OnMouseDrag.Bind(
        backdropPanel.Get(),
        [this, uiStage = uiSubsystem->GetUIStage().Get()](const MouseEvent& event)
        {
            // prevent click being triggered on release once mouse has been dragged
            m_shouldCancelNextClick = true;

            EditorViewport* activeViewport = GetActiveViewport();
            if (!activeViewport)
            {
                return UIEventHandlerResult::OK;
            }

            if (GetTerrainState()->IsEnabled() && event.mouseButtons[MouseButtonState::LEFT])
            {
                InputManager* inputManager = g_appContext->GetMainWindow()->GetInputManager();

                GetTerrainState()->UpdateStroke(event.relativePos, /* invert */ inputManager->IsShiftDown());

                return UIEventHandlerResult::STOP_BUBBLING;
            }

            if (GetDecalPainterState()->IsEnabled() && event.mouseButtons[MouseButtonState::LEFT])
            {
                InputManager* inputManager = g_appContext->GetMainWindow()->GetInputManager();

                GetDecalPainterState()->UpdateStroke(event.relativePos, /* erase */ inputManager->IsShiftDown());

                return UIEventHandlerResult::STOP_BUBBLING;
            }

            if (IsMeshEditDragActive())
            {
                UpdateMeshEditDrag(activeViewport->GetCamera(), event);

                return UIEventHandlerResult::STOP_BUBBLING;
            }

            if (IsHoveringGizmo())
            {
                // If the mouse is currently over a gizmo, don't allow camera to handle the event
                Handle<EditorGizmoBase> gizmo = m_gizmoController->GetHoveredGizmo().Lock();
                Handle<Node> node = m_gizmoController->GetHoveredGizmoNode().Lock();

                if (!gizmo || !node)
                {
                    HYP_LOG(Editor, Warning, "Failed to lock hovered gizmo or node");

                    return UIEventHandlerResult::ERR;
                }

                if (gizmo->OnMouseMove(activeViewport->GetCamera(), event, node))
                {
                    return UIEventHandlerResult::STOP_BUBBLING;
                }
            }

            CameraController* controller = activeViewport->GetCamera()->GetCameraController();
            
            if (controller != nullptr)
            {
                InputHandlerBase* inputHandler = controller->GetInputHandler();

                if (inputHandler != nullptr)
                {
                    if (inputHandler->OnMouseDrag(event))
                    {
                        return UIEventHandlerResult::STOP_BUBBLING;
                    }
                }
            }

            return UIEventHandlerResult::OK;
        }));

    backdropPanel->OnMouseMove.RemoveAllFromSet(m_delegateHandlers);
    m_delegateHandlers.Add(backdropPanel->OnMouseMove.Bind(
        backdropPanel.Get(),
        [this, uiStage = uiSubsystem->GetUIStage().Get()](const MouseEvent& event)
        {
            EditorViewport* activeViewport = GetActiveViewport();
            if (!activeViewport)
            {
                return UIEventHandlerResult::OK;
            }

            if (GetDecalPainterState()->IsEnabled())
            {
                GetDecalPainterState()->UpdateHover(event.relativePos);

                if (GetDecalPainterState()->IsStroking() && event.mouseButtons[MouseButtonState::LEFT])
                {
                    InputManager* inputManager = g_appContext->GetMainWindow()->GetInputManager();

                    GetDecalPainterState()->UpdateStroke(event.relativePos, /* erase */ inputManager->IsShiftDown());

                    return UIEventHandlerResult::STOP_BUBBLING;
                }

                if (!event.mouseButtons[MouseButtonState::LEFT])
                {
                    return UIEventHandlerResult::STOP_BUBBLING;
                }
            }

            if (GetTerrainState()->IsEnabled())
            {
                GetTerrainState()->UpdateHover(event.relativePos);

                if (GetTerrainState()->IsStroking() && event.mouseButtons[MouseButtonState::LEFT])
                {
                    InputManager* inputManager = g_appContext->GetMainWindow()->GetInputManager();

                    GetTerrainState()->UpdateStroke(event.relativePos, /* invert */ inputManager->IsShiftDown());

                    return UIEventHandlerResult::STOP_BUBBLING;
                }

                if (!event.mouseButtons[MouseButtonState::LEFT])
                {
                    return UIEventHandlerResult::STOP_BUBBLING;
                }
            }

            if (m_meshEditState.enabled && !event.mouseButtons[MouseButtonState::LEFT] && !IsMeshEditDragActive())
            {
                const Ray ray = activeViewport->GetCamera()->GetPickRay(event.relativePos);

                UpdateHoveredMeshEditFace(ray);

                return UIEventHandlerResult::STOP_BUBBLING;
            }

            // Hover over a gizmo when mouse is not down
            if (!event.mouseButtons[MouseButtonState::LEFT]
                && GetSelectedManipulationMode() != EditorManipulationMode::None
                && !AreGizmosHiddenByProximity())
            {
                // Ray test the gizmo

                const Ray ray = activeViewport->GetCamera()->GetPickRay(event.relativePos);

                RayTestResults results;

                EditorGizmoBase* gizmo = GetSelectedGizmo();

                bool testRayReturnedHit = gizmo && gizmo->GetNode()->TestRay(ray, results, RayTestFlags::TestBVH | RayTestFlags::EditorPick);

                if (testRayReturnedHit)
                {
                    for (const RayHit& rayHit : results)
                    {
                        if (!rayHit.node)
                            continue;

                        if (rayHit.node == m_gizmoController->GetHoveredGizmoNode().GetUnsafe())
                        {
                            return UIEventHandlerResult::STOP_BUBBLING;
                        }

                        Handle<Node> nodeHandle = MakeStrongRef(rayHit.node);

                        if (gizmo->OnMouseHover(activeViewport->GetCamera(), event, nodeHandle))
                        {
                            SetHoveredGizmo(event, gizmo, nodeHandle);

                            return UIEventHandlerResult::STOP_BUBBLING;
                        }
                    }
                }

                SetHoveredGizmo(event, nullptr, Handle<Node>::Null());
            }

            CameraController* controller = activeViewport->GetCamera()->GetCameraController();
            
            if (controller != nullptr)
            {
                InputHandlerBase* inputHandler = controller->GetInputHandler();

                if (inputHandler != nullptr)
                {
                    if (inputHandler->OnMouseMove(event))
                    {
                        return UIEventHandlerResult::STOP_BUBBLING;
                    }
                }
            }

            return UIEventHandlerResult::OK;
        }));

    backdropPanel->OnMouseDown.RemoveAllFromSet(m_delegateHandlers);
    m_delegateHandlers.Add(backdropPanel->OnMouseDown.Bind(
        backdropPanel.Get(),
        [this, uiStageWeak = uiSubsystem->GetUIStage().ToWeak()](const MouseEvent& event)
        {
            m_shouldCancelNextClick = false;

            EditorViewport* activeViewport = GetActiveViewport();
            if (!activeViewport)
            {
                return UIEventHandlerResult::OK;
            }

            if (GetTerrainState()->IsEnabled())
            {
                InputManager* inputManager = g_appContext->GetMainWindow()->GetInputManager();

                GetTerrainState()->BeginStroke(event.relativePos, /* invert */ inputManager->IsShiftDown());

                return UIEventHandlerResult::STOP_BUBBLING;
            }

            if (GetDecalPainterState()->IsEnabled())
            {
                InputManager* inputManager = g_appContext->GetMainWindow()->GetInputManager();

                GetDecalPainterState()->BeginStroke(event.relativePos, /* erase */ inputManager->IsShiftDown());

                return UIEventHandlerResult::STOP_BUBBLING;
            }

            if (m_meshEditState.enabled && m_meshEditState.selectedFace)
            {
                StartMeshEditDrag(activeViewport->GetCamera(), event);

                return UIEventHandlerResult::STOP_BUBBLING;
            }

            if (IsHoveringGizmo())
            {
                Handle<EditorGizmoBase> gizmo = m_gizmoController->GetHoveredGizmo().Lock();
                Handle<Node> node = m_gizmoController->GetHoveredGizmoNode().Lock();

                if (gizmo && node && !gizmo->IsDragging())
                {
                    const Ray ray = activeViewport->GetCamera()->GetPickRay(event.relativePos);

                    RayTestResults results;

                    if (node->TestRay(ray, results, RayTestFlags::TestBVH | RayTestFlags::EditorPick))
                    {
                        for (const RayHit& rayHit : results)
                        {
                            TrackBakedEnvProbePlacements();

                            gizmo->OnDragStart(activeViewport->GetCamera(), event, node, rayHit.hitpoint);

                            return UIEventHandlerResult::STOP_BUBBLING;
                        }
                    }
                }
            }

            CameraController* controller = activeViewport->GetCamera()->GetCameraController();
            
            if (controller != nullptr)
            {
                InputHandlerBase* inputHandler = controller->GetInputHandler();

                if (inputHandler != nullptr)
                {
                    if (inputHandler->OnMouseDown(event))
                    {
                        return UIEventHandlerResult::STOP_BUBBLING;
                    }
                }
            }

            return UIEventHandlerResult::OK;
        }));

    backdropPanel->OnMouseUp.RemoveAllFromSet(m_delegateHandlers);
    m_delegateHandlers.Add(backdropPanel->OnMouseUp.Bind(
        backdropPanel.Get(),
        [this](const MouseEvent& event)
        {
            m_shouldCancelNextClick = false;

            EditorViewport* activeViewport = GetActiveViewport();
            if (!activeViewport)
            {
                return UIEventHandlerResult::OK;
            }

            if (GetTerrainState()->IsEnabled())
            {
                GetTerrainState()->EndStroke();
            }

            if (GetDecalPainterState()->IsEnabled())
            {
                GetDecalPainterState()->EndStroke();
            }

            CameraController* controller = activeViewport->GetCamera()->GetCameraController();
            
            if (controller != nullptr)
            {
                InputHandlerBase* inputHandler = controller->GetInputHandler();

                if (inputHandler != nullptr)
                {
                    inputHandler->OnMouseUp(event);
                }
            }

            if (EditorGizmoBase* gizmo = GetSelectedGizmo(); gizmo && gizmo->IsDragging())
            {
                gizmo->OnDragEnd(activeViewport->GetCamera(), event);
            }

            if (IsMeshEditDragActive())
            {
                EndMeshEditDrag(true);
            }

            return UIEventHandlerResult::OK;
        }));

    backdropPanel->OnKeyDown.RemoveAllFromSet(m_delegateHandlers);
    m_delegateHandlers.Add(backdropPanel->OnKeyDown.Bind(
        backdropPanel.Get(),
        [this](const KeyboardEvent& event)
        {
            EditorViewport* activeViewport = GetActiveViewport();
            if (!activeViewport)
            {
                return UIEventHandlerResult::OK;
            }

            if (event.keyCode == KeyCode::KEY_ESCAPE && m_meshEditState.enabled)
            {
                if (BackOutOfMeshEditState())
                {
                    return UIEventHandlerResult::STOP_BUBBLING;
                }
            }

            ///Keyboard shortcuts
            if (g_appContext.IsValid()
                && g_appContext->GetMainWindow() != nullptr
                && g_appContext->GetMainWindow()->GetInputManager()->IsCtrlDown()
                && !IsSimulating()
                && !IsMeshEditDragActive())
            {
                const bool isShiftDown = g_appContext->GetMainWindow()->GetInputManager()->IsShiftDown();

                Name commandName;

                switch (event.keyCode)
                {
                case KeyCode::KEY_Z:
                    commandName = isShiftDown ? NAME("EditorCommandRedo") : NAME("EditorCommandUndo");
                    break;
                case KeyCode::KEY_Y:
                    commandName = NAME("EditorCommandRedo");
                    break;
                case KeyCode::KEY_C:
                    commandName = NAME("EditorCommandCopy");
                    break;
                case KeyCode::KEY_V:
                    commandName = NAME("EditorCommandPaste");
                    break;
                case KeyCode::KEY_A:
                    commandName = NAME("EditorCommandSelectAll");
                    break;
                default:
                    break;
                }

                if (commandName.IsValid())
                {
                    ExecuteCommandByName(commandName, String::empty);

                    return UIEventHandlerResult::STOP_BUBBLING;
                }
            }

            if (IsMeshEditDragActive())
            {
                int lockedAxis = -1;

                switch (event.keyCode)
                {
                case KeyCode::KEY_X:
                    lockedAxis = 0;
                    break;
                case KeyCode::KEY_Y:
                    lockedAxis = 1;
                    break;
                case KeyCode::KEY_Z:
                    lockedAxis = 2;
                    break;
                default:
                    break;
                }

                if (lockedAxis != -1)
                {
                    SetMeshEditDragLockedAxis(activeViewport->GetCamera(), event, lockedAxis);

                    return UIEventHandlerResult::STOP_BUBBLING;
                }
            }

            CameraController* controller = activeViewport->GetCamera()->GetCameraController();

            if (controller != nullptr)
            {
                InputHandlerBase* inputHandler = controller->GetInputHandler();

                if (inputHandler != nullptr)
                {
                    if (inputHandler->OnKeyDown(event))
                    {
                        return UIEventHandlerResult::STOP_BUBBLING;
                    }
                }
            }

            return UIEventHandlerResult::OK;
        }));

    backdropPanel->OnKeyUp.RemoveAllFromSet(m_delegateHandlers);
    m_delegateHandlers.Add(backdropPanel->OnKeyUp.Bind(
        backdropPanel.Get(),
        [this](const KeyboardEvent& event)
        {
            EditorViewport* activeViewport = GetActiveViewport();
            if (!activeViewport)
            {
                return UIEventHandlerResult::OK;
            }

            CameraController* controller = activeViewport->GetCamera()->GetCameraController();
            
            if (controller != nullptr)
            {
                InputHandlerBase* inputHandler = controller->GetInputHandler();

                if (inputHandler != nullptr)
                {
                    inputHandler->OnKeyUp(event);
                }
            }

            return UIEventHandlerResult::OK;
        }));

    backdropPanel->OnGainFocus.RemoveAllFromSet(m_delegateHandlers);
    m_delegateHandlers.Add(backdropPanel->OnGainFocus.Bind(
        backdropPanel.Get(),
        [this](const MouseEvent& event)
        {
            m_editorCameraEnabled = true;

            EditorViewport* activeViewport = GetActiveViewport();
            if (!activeViewport)
            {
                return UIEventHandlerResult::OK;
            }

            CameraController* controller = activeViewport->GetCamera()->GetCameraController();
            
            if (controller != nullptr)
            {
                InputHandlerBase* inputHandler = controller->GetInputHandler();

                if (inputHandler != nullptr)
                {
                    if (inputHandler->OnGainFocus(event))
                    {
                        return UIEventHandlerResult::STOP_BUBBLING;
                    }
                }
            }

            return UIEventHandlerResult::OK;
        }));

    backdropPanel->OnLoseFocus.RemoveAllFromSet(m_delegateHandlers);
    m_delegateHandlers.Add(backdropPanel->OnLoseFocus.Bind(
        backdropPanel.Get(),
        [this](const MouseEvent& event)
        {
            m_editorCameraEnabled = false;

            EditorViewport* activeViewport = GetActiveViewport();
            if (!activeViewport)
            {
                return UIEventHandlerResult::OK;
            }

            CameraController* controller = activeViewport->GetCamera()->GetCameraController();
            
            if (controller != nullptr)
            {
                InputHandlerBase* inputHandler = controller->GetInputHandler();

                if (inputHandler != nullptr)
                {
                    inputHandler->OnLoseFocus(event);
                }
            }

            return UIEventHandlerResult::OK;
        }));
}

void EditorSubsystem::SetSelectedBucket(uint32 bucketIndex)
{
    if (m_selectedBucketIndex == bucketIndex)
    {
        return;
    }

    m_selectedBucketIndex = bucketIndex;

    OnSelectedBucketChanged(bucketIndex);
}

void EditorSubsystem::RequestAssetThumbnail(uint32 bucketIndex, Name assetName)
{
    AssertOnThread(g_simThread);

    if (m_thumbnailService)
    {
        m_thumbnailService->Request(bucketIndex, assetName);
    }
}

String EditorSubsystem::GetAssetThumbnailPath(uint32 bucketIndex, Name assetName) const
{
    if (!m_thumbnailService)
    {
        return String::empty;
    }

    return m_thumbnailService->GetCachedThumbnailPath(bucketIndex, assetName);
}

void EditorSubsystem::CancelPendingAssetThumbnails()
{
    AssertOnThread(g_simThread);

    if (m_thumbnailService)
    {
        m_thumbnailService->CancelPending();
    }
}

void EditorSubsystem::BeginMaterialPreview(uint32 bucketIndex, Name assetName)
{
    AssertOnThread(g_simThread);

    if (m_materialPreviewRenderer)
    {
        m_materialPreviewRenderer->SetMaterial(bucketIndex, assetName);
    }
}

void EditorSubsystem::EndMaterialPreview()
{
    AssertOnThread(g_simThread);

    if (m_materialPreviewRenderer)
    {
        m_materialPreviewRenderer->SetMaterial(0, Name());
    }
}

void EditorSubsystem::SetMaterialPreviewLightAngles(float yaw, float pitch)
{
    AssertOnThread(g_simThread);

    if (m_materialPreviewRenderer)
    {
        m_materialPreviewRenderer->SetLightAngles(yaw, pitch);
    }
}

void EditorSubsystem::InvalidateMaterialPreview()
{
    if (m_materialPreviewRenderer)
    {
        m_materialPreviewRenderer->Invalidate();
    }
}

bool EditorSubsystem::ExecuteCommand(const Handle<EditorCommandBase>& command)
{
    if (!command)
    {
        return false;
    }

    if (IsSimulating() && !command->AllowedWhileSimulating())
    {
        HYP_LOG(Editor, Warning, "Cannot execute command '{}' while simulation is active", command->InstanceClass()->GetName());

        return false;
    }

    if (IsOnThread(g_simThread))
    {
        command->Execute(this);
    }
    else
    {
        GetThreadById(g_simThread)->GetScheduler().Enqueue(
            [this, weakThis = MakeWeakRef(this), command = command]()
            {
                Handle<EditorSubsystem> strongThis = weakThis.Lock();
                if (!strongThis)
                {
                    HYP_LOG(Editor, Error, "Failed to lock EditorSubsystem from weak reference in ExecuteCommand");
                    return;
                }

                command->Execute(this);
            },
            TaskEnqueueFlags::FIRE_AND_FORGET);
    }

    return true;
}

bool EditorSubsystem::ExecuteCommandByName(Name name, const String& args)
{
    Handle<EditorCommandBase> command = CreateCommandByName(name);

    if (!command)
    {
        return false;
    }

    command->SetArguments(args.Split(' '));

    return ExecuteCommand(command);
}

Handle<EditorCommandBase> EditorSubsystem::CreateCommandByName(Name name) const
{
    if (!name.IsValid())
    {
        return Handle<EditorCommandBase>::Null();
    }

    const Class* commandClass = ClassRegistry::GetInstance().GetClass(name);
    if (!commandClass || !commandClass->IsDerivedFrom(EditorCommandBase::StaticClass()))
    {
        String nameStr = name.ToString();
        if (!nameStr.EndsWith("Commandlet"))
        {
            // try again with "Commandlet" appended to the name
            String nameWithCommandlet = nameStr + "Commandlet";
            commandClass = ClassRegistry::GetInstance().GetClass(nameWithCommandlet);
        }

        if (!commandClass || !commandClass->IsDerivedFrom(EditorCommandBase::StaticClass()))
        {
            HYP_LOG(Editor, Error, "Invalid command class: {}", name);
            return Handle<EditorCommandBase>::Null();
        }
    }

    BoxedValue instanceData;
    if (!commandClass->CreateInstance(instanceData))
    {
        HYP_LOG(Editor, Error, "Failed to construct command instance: {}", name);
        return Handle<EditorCommandBase>::Null();
    }

    Handle<EditorCommandBase> command = instanceData.Get<Handle<EditorCommandBase>>();
    AssertDebug(command != nullptr);

    return command;
}

void EditorSubsystem::OpenProjectAtPath(const String& projectFilepath)
{
    Handle<EditorCommandBase> command = CreateCommandByName(NAME("EditorCommandOpenProjectAtPath"));

    if (!command)
    {
        return;
    }

    command->SetArguments({ projectFilepath });

    ExecuteCommand(command);
}

void EditorSubsystem::NewProject()
{
    Handle<EditorProject> project = EditorProject::CreateNew();

    Handle<Scene> mainScene = MakeHandle<Scene>();
    mainScene->SetName(NAME("MainScene"));
    mainScene->SetSceneFlags(SceneFlags::DEFAULT & ~SceneFlags::STREAMED);
    project->AddScene(mainScene);

    Handle<DirectionalLight> sun = MakeHandle<DirectionalLight>();
    sun->SetName(NAME("SunLight"));
    sun->SetDirection(Vec3f(0.0f, 0.8f, 0.0f).Normalize());
    sun->SetColor(Color(Vec4f(1.0f, 0.9f, 0.8f, 1.0f)));
    sun->SetIntensity(18.0f);
    InitObject(sun);

    mainScene->GetRoot()->AddChild(sun);

    {
        // The new project isn't open yet, so point asset registration at its registry explicitly
        GlobalContextScope assetRegistryScope { AssetRegistryContext { project->GetGame()->GetAssetRegistry() } };

        EditorPlayerSetup::AddGround(mainScene->GetRoot(), NAME("Ground"));

        EditorThirdPersonPlayer player = EditorPlayerSetup::CreateThirdPersonPlayer(NAME("Player"), NAME("Camera"));
        GetCurrentAssetRegistry()->PutAssetUnique(player.capsuleShape);

        // In edit mode the player's origin is the capsule center, so this rests the capsule on the ground
        player.playerEntity->SetWorldTranslation(Vec3f(0.0f, player.capsuleShape->GetHeight() * 0.5f + player.capsuleShape->GetRadius(), 0.0f));

        mainScene->GetRoot()->AddChild(player.playerEntity);

        EditorPlayerSetup::AttachToScene(player);

        player.playerEntity->AddTag<EntityTag::Player>();
    }

    // Handle<Scene> streamedScene = MakeHandle<Scene>();
    // streamedScene->SetName(NAME("StreamedScene"));
    // streamedScene->SetSceneFlags(SceneFlags::DEFAULT);
    // project->AddScene(streamedScene);

    // add dynamic skybox
    project->GetWorld()->AddSystemT<DynamicSkySystem>();

    OpenProject(project);
}

void EditorSubsystem::CloseProject(bool shutdownWorld)
{
    AssertOnThread(g_simThread);

    if (m_currentProject)
    {
        // Tear the preview scenes down before the world goes away - they hold Scenes inside it.
        if (m_thumbnailService)
        {
            m_thumbnailService->Shutdown();
            m_thumbnailService.Reset();
        }

        if (m_materialPreviewRenderer)
        {
            m_materialPreviewRenderer->Shutdown();
            m_materialPreviewRenderer.Reset();
        }

        ShutdownProjectWorld(m_currentProject, /* shutdownWorld */ shutdownWorld);
        OnProjectClosing(m_currentProject);

        m_currentProject->OnProjectSaved.RemoveAllFromSet(m_delegateHandlers);

        if (const Handle<EditorActionStack>& actionStack = m_currentProject->GetActionStack(); actionStack.IsValid())
        {
            actionStack->OnBeforeActionPush.RemoveAllFromSet(m_delegateHandlers);
            actionStack->OnBeforeActionPop.RemoveAllFromSet(m_delegateHandlers);
            actionStack->OnAfterActionPush.RemoveAllFromSet(m_delegateHandlers);
            actionStack->OnAfterActionPop.RemoveAllFromSet(m_delegateHandlers);
        }

        m_committedEnvProbePlacements.Clear();

        m_currentProject->SetEditorSubsystem(WeakHandle<EditorSubsystem>::Null());
        m_currentProject->Close(/* shutdownWorld */ shutdownWorld);

        m_currentProject.Reset();
    }

    RenderProxyList& pickRpl = g_editorState->GetPickCache().GetRenderProxyList();

    pickRpl.BeginWrite();
    pickRpl.ClearAll();
    pickRpl.EndWrite();

    g_editorState->GetPickCache().Clear();
}

void EditorSubsystem::OpenProject(const Handle<EditorProject>& project)
{
    AssertOnThread(g_simThread);

    if (project == m_currentProject)
    {
        return;
    }

    const bool isSimulationStateChange = IsSimulating();
    const bool isStartSimulation = isSimulationStateChange && project != m_preSimulationProject;

    CloseProject(/* shutdownWorld*/ true);

    if (!project.IsValid())
    {
        return;
    }

    project->SetEditorSubsystem(MakeWeakRef(this));

    m_currentProject = project;

    m_delegateHandlers.Add(m_currentProject->OnProjectSaved.Bind(
        m_currentProject.Get(),
        [](const Handle<EditorProject>& savedProject)
        {
            g_editorState->AddRecentProject(savedProject->GetFilePath());
        }));

    if (const Handle<EditorActionStack>& actionStack = m_currentProject->GetActionStack(); actionStack.IsValid())
    {
        auto captureBeforeAction = [this](EditorActionBase*)
        {
            TrackBakedEnvProbePlacements();
        };

        auto rebakeAfterAction = [this](EditorActionBase*)
        {
            RebakeMovedEnvProbes();
        };

        m_delegateHandlers.Add(actionStack->OnBeforeActionPush.Bind(actionStack.Get(), captureBeforeAction));
        m_delegateHandlers.Add(actionStack->OnBeforeActionPop.Bind(actionStack.Get(), captureBeforeAction));
        m_delegateHandlers.Add(actionStack->OnAfterActionPush.Bind(actionStack.Get(), rebakeAfterAction));
        m_delegateHandlers.Add(actionStack->OnAfterActionPop.Bind(actionStack.Get(), rebakeAfterAction));
    }

    InitializeProjectWorld(m_currentProject, isStartSimulation);

    m_thumbnailService = MakeUnique<AssetThumbnailService>();
    m_thumbnailService->OnThumbnailReady
        .Bind([this](uint32 bucketIndex, Name assetName)
              {
                  OnThumbnailReady(bucketIndex, assetName);
              })
        .Detach();
    m_thumbnailService->Initialize(m_currentProject->GetWorld().Get());

    m_materialPreviewRenderer = MakeUnique<MaterialPreviewRenderer>();
    m_materialPreviewRenderer->OnFrameReady
        .Bind([this]()
              {
                  OnMaterialPreviewUpdated();
              })
        .Detach();
    m_materialPreviewRenderer->Initialize(m_currentProject->GetWorld().Get());

    OnProjectOpened(m_currentProject);

    g_editorState->SetCurrentProject(m_currentProject, isSimulationStateChange);

    if (!isSimulationStateChange && m_currentProject->IsSaved())
    {
        g_editorState->AddRecentProject(m_currentProject->GetFilePath());
    }

    const Vec3f editorCameraDirection = project->GetEditorCameraDirection();

    if (!MathUtil::ApproxEqual(editorCameraDirection, Vec3f::Zero()))
    {
        Camera* editorCamera = nullptr;

        if (EditorViewport* activeViewport = GetActiveViewport())
        {
            editorCamera = activeViewport->GetCamera();
        }

        if (!editorCamera)
        {
            editorCamera = g_editorState->GetEditorCamera();
        }

        if (editorCamera)
        {
            editorCamera->SetWorldTranslation(project->GetEditorCameraPosition());
            editorCamera->SetDirection(editorCameraDirection);
        }
    }
}

void EditorSubsystem::ShowImportContentDialog()
{
    ShowOpenFileDialog(
        "Select the file(s) to import into the project",
        EngineGlobals::GetDataDirectory(),
        { "obj", "fbx", "jpg", "jpeg", "png", "tga", "bmp", "ogre.xml" },
        /* allowMultiple */ true, /* allowDirectories */ false,
        [](TResult<Array<FilePath>>&& result)
        {
            if (result.HasError())
            {
                HYP_LOG(Editor, Error, "Failed to select files to import: {}", result.GetError().GetMessage());

                return;
            }

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
                batch->Add(file.Basename(), file);
            }

            batch->OnComplete
                .Bind([](AssetMap& results)
                      {
                          HYP_LOG(Editor, Verbose, "{} assets loaded.", results.Size());

                          /// \todo Open folder the assets ended up in
                      })
                .Detach();

            batch->LoadAsync();

            // Note: The batch will be destroyed automatically by AssetManager when complete
        });
}

void EditorSubsystem::SetFocusedNode(const Handle<Node>& focusedNode, bool shouldSelectInOutline)
{
    if (focusedNode == m_focusedNode)
    {
        return;
    }

    const Handle<Node> previousFocusedNode = m_focusedNode.Lock();

    m_focusedNode = focusedNode;

    if (m_meshEditState.enabled && focusedNode != m_meshEditState.targetNode)
    {
        Entity* entity = DynamicCast<Entity>(focusedNode.Get());
        MeshComponent* meshComponent = entity ? entity->TryGetComponent<MeshComponent>() : nullptr;

        if (meshComponent && meshComponent->mesh.IsValid())
        {
            EndMeshEditDrag(/* saveEdits */ true);
            CommitMeshEdits();

            m_meshEditState.targetNode = focusedNode.ToWeak();

            m_meshEditState.actionStack = MakeHandle<EditorActionStack>(m_currentProject.ToWeak());

            SetSelectedMeshEditFace({});
            m_meshEditState.hoveredFace.Unset();

            OnMeshEditStateChanged();
        }
        else
        {
            ExitMeshEditMode(/* saveEdits */ true);
        }
    }

    if (Handle<Node> focusedNode = m_focusedNode.Lock(); focusedNode.IsValid())
    {
        if (focusedNode->GetScene() != nullptr)
        {
            if (Entity* entity = DynamicCast<Entity>(focusedNode))
            {
                entity->AddTag<EntityTag::FocusedInEditor>();
            }
        }

        HYP_LOG(Editor, Verbose, "Set focused node: {}\t{}\t is static ? {}", focusedNode->GetName(), focusedNode->GetWorldTranslation(),
                focusedNode->IsStatic());

        if (!m_meshEditState.enabled)
        {
            if (focusedNode->IsA<VolumeBase>() && StaticCast<VolumeBase>(focusedNode)->useVolumeEditTool)
            {
                SetSelectedManipulationMode(EditorManipulationMode::ReshapeVolume);
            }
            else if (GetSelectedManipulationMode() == EditorManipulationMode::None
                     || GetSelectedManipulationMode() == EditorManipulationMode::ReshapeVolume)
            {
                SetSelectedManipulationMode(EditorManipulationMode::Translate);
            }
        }

        EditorGizmoBase* gizmo = GetSelectedGizmo();

        if (gizmo)
        {
            gizmo->SetFocusedNode(focusedNode);
        }
    }

    if (previousFocusedNode != nullptr)
    {
        if (Entity* entity = DynamicCast<Entity>(previousFocusedNode))
        {
            entity->RemoveTag<EntityTag::FocusedInEditor>();
        }
    }

    // So, we now use SelectedNodes for multi-select, but for backwards compatibility and some other stuff that needs a single
    // 'focused' node (i.e where to place the gizmo? or maybe it should use avg position?) 
    // We check if the selection includes the focused node.
    //  - if it does, we do nothing,
    //  - otherwise, clear the selection, and set selection to be *just* the focused node.
    if (!m_selectedNodes.Contains(focusedNode))
    {
        SetSelectedNodes({ focusedNode });
    }

    OnFocusedNodeChanged(focusedNode, previousFocusedNode, shouldSelectInOutline);
}

Handle<Scene> EditorSubsystem::GetActiveScene() const
{
    AssertOnThread(g_simThread);
    return m_activeScene.Lock();
}

String EditorSubsystem::GetCodeEditor() const
{
    return String(g_cvCodeEditor.Get());
}

Array<Name> EditorSubsystem::GetAvailableWorldGridLayerClassNames() const
{
    Array<Name> result;

    auto functor =
        [&result](const Class* cls)
        {
            if (cls == nullptr || cls->IsAbstract() || !cls->IsDerivedFrom(WorldGridLayer::StaticClass()))
            {
                return IterationResult::CONTINUE;
            }

            result.PushBack(cls->GetName());

            return IterationResult::CONTINUE;
        };

    ClassRegistry::GetInstance().ForEachClass(functor);

    return result;
}

Handle<DynamicSkySystem> EditorSubsystem::GetDynamicSkySystem() const
{
    AssertOnThread(g_simThread);

    const Handle<World>& world = GetProjectWorld();

    if (!world.IsValid())
    {
        return Handle<DynamicSkySystem>::empty;
    }

    DynamicSkySystem* dynamicSkySystem = world->GetSystem<DynamicSkySystem>();

    if (!dynamicSkySystem)
    {
        return Handle<DynamicSkySystem>::empty;
    }

    return MakeStrongRef(dynamicSkySystem);
}

Handle<Node> EditorSubsystem::GetFocusedNode() const
{
    AssertOnThread(g_simThread);
    return m_focusedNode.Lock();
}

void EditorSubsystem::AddToSelection(const Handle<Node>& node)
{
    AssertOnThread(g_simThread);

    if (!node.IsValid())
    {
        return;
    }

    auto result = m_selectedNodes.Insert(node);

    if (result.second)
    {
        OnSelectionChanged();
    }
}

void EditorSubsystem::RemoveFromSelection(const Handle<Node>& node)
{
    AssertOnThread(g_simThread);

    if (!node.IsValid())
    {
        return;
    }

    auto it = m_selectedNodes.Find(node);

    if (it != m_selectedNodes.End())
    {
        m_selectedNodes.Erase(it);

        OnSelectionChanged();
    }
}

void EditorSubsystem::ClearSelection()
{
    AssertOnThread(g_simThread);

    if (m_selectedNodes.Empty())
    {
        return;
    }

    m_selectedNodes.Clear();

    OnSelectionChanged();
}

bool EditorSubsystem::IsNodeSelected(const Handle<Node>& node) const
{
    AssertOnThread(g_simThread);

    if (!node.IsValid())
    {
        return false;
    }

    return m_selectedNodes.Find(node) != m_selectedNodes.End();
}

void EditorSubsystem::SetSelectedNodes(const Array<Handle<Node>>& nodes)
{
    AssertOnThread(g_simThread);

    if (nodes.Empty())
    {
        ClearSelection();

        return;
    }

    m_selectedNodes = Set<Handle<Node>, EditorAllocator>(nodes.Begin(), nodes.End());

    OnSelectionChanged();
}

Array<Handle<Node>> EditorSubsystem::GetSelectedNodes() const
{
    AssertOnThread(g_simThread);

    Array<Handle<Node>> result;

    for (const Handle<Node>& node : m_selectedNodes)
    {
        result.PushBack(node);
    }

    return result;
}

Vec3f EditorSubsystem::CalculateSceneInsertionPoint(float desiredDistance, float offsetFromSurface) const
{
    EditorViewport* activeViewport = GetActiveViewport();
    if (activeViewport == nullptr)
    {
        return Vec3f::Zero();
    }

    const Vec3f cameraPosition = activeViewport->GetCamera()->GetWorldTranslation();
    const Vec3f cameraDirection = activeViewport->GetCamera()->GetDirection();

    Vec3f insertionPoint = cameraPosition + cameraDirection * desiredDistance;

    const Ray ray { cameraPosition, cameraDirection };

    RayTestResults results;

    Handle<Scene> activeScene = m_activeScene.Lock();
    if (!activeScene)
    {
        return insertionPoint;
    }

    // raytest using scene's octree
    if ((activeScene->GetSceneFlags() & SceneFlags::HAS_OCTREE) && activeScene->GetOctree().TestRay(ray, results, RayTestFlags::TestBVH))
    {
        const RayHit& closestHit = results.Front();

        if (closestHit.distance < desiredDistance)
        {
            // offset the object slightly to avoid clipping
            insertionPoint = closestHit.hitpoint - cameraDirection * offsetFromSurface;

            const float distanceFromCamera = (insertionPoint - cameraPosition).Length();

            // min 1 world unit
            if (distanceFromCamera < 1.0f)
            {
                insertionPoint = cameraPosition + cameraDirection * 1.0f;
            }
        }
    }

    return insertionPoint;
}

bool EditorSubsystem::TestPickRay(const Ray& ray, RayTestResults& outResults)
{
    bool hasHits = false;

    for (const Handle<EditorViewport>& vp : m_editorViewports)
    {
        if (vp->GetView()->TestRay(ray, outResults, RayTestFlags::TestBVH | RayTestFlags::EditorPick))
        {
            hasHits = true;
        }
    }

    if (const Handle<World>& projectWorld = GetProjectWorld(); projectWorld.IsValid())
    {
        if (EditorSpriteSystem* spriteSystem = projectWorld->GetSystem<EditorSpriteSystem>())
        {
            hasHits |= spriteSystem->TestRay(ray, outResults);
        }
    }

    return hasHits;
}

Node* EditorSubsystem::PickNodeAtViewport(const Vec2f& screenPosition)
{
    EditorViewport* activeViewport = GetActiveViewport();
    if (!activeViewport || !activeViewport->GetCamera())
    {
        return nullptr;
    }

    const Ray ray = activeViewport->GetCamera()->GetPickRay(screenPosition);

    RayTestResults results;
    if (!TestPickRay(ray, results))
    {
        return nullptr;
    }

    for (const RayHit& hit : results)
    {
        if (hit.node != nullptr)
        {
            return hit.node;
        }
    }

    return nullptr;
}

bool EditorSubsystem::IsEntityTargetedAsset(uint32 bucketIndex, Name assetName)
{
    if (bucketIndex == AssetBuckets::None.GetIndex() || bucketIndex >= MaxAssetBuckets)
    {
        return false;
    }

    Handle<AssetObject> asset = GetCurrentAssetRegistry()->GetAsset(*AssetBuckets::AllBuckets[bucketIndex], assetName);

    return EditorEntityAssetDrop::TargetsEntity(asset.Get());
}

bool EditorSubsystem::UpdateViewportAssetDropTarget(uint32 bucketIndex, Name assetName, float screenX, float screenY)
{
    AssertOnThread(g_simThread);

    return SetAssetDropTargetNode(bucketIndex, assetName, PickNodeAtViewport(Vec2f(screenX, screenY)));
}

bool EditorSubsystem::UpdateNodeAssetDropTarget(uint32 bucketIndex, Name assetName, const Handle<Node>& node)
{
    AssertOnThread(g_simThread);

    return SetAssetDropTargetNode(bucketIndex, assetName, node.Get());
}

void EditorSubsystem::ClearAssetDropTarget()
{
    m_assetDropState = AssetDropState {};
}

bool EditorSubsystem::SetAssetDropTargetNode(uint32 bucketIndex, Name assetName, Node* node)
{
    if (m_assetDropState.bucketIndex != bucketIndex || m_assetDropState.assetName != assetName)
    {
        m_assetDropState.bucketIndex = bucketIndex;
        m_assetDropState.assetName = assetName;
        m_assetDropState.asset.Reset();

        if (bucketIndex != AssetBuckets::None.GetIndex() && bucketIndex < MaxAssetBuckets)
        {
            m_assetDropState.asset = GetCurrentAssetRegistry()->GetAsset(*AssetBuckets::AllBuckets[bucketIndex], assetName);
        }
    }

    Entity* entity = DynamicCast<Entity>(node);
    Handle<Scene> activeScene = GetActiveScene();

    if (entity != nullptr
        && activeScene.IsValid()
        && entity->GetScene() == activeScene.Get()
        && EditorEntityAssetDrop::CanApplyToEntity(m_assetDropState.asset.Get(), entity))
    {
        m_assetDropState.targetNode = MakeWeakRef(node);

        return true;
    }

    m_assetDropState.targetNode.Reset();

    return false;
}

void EditorSubsystem::DebugDrawAssetDropTarget(DebugDrawCommandList& debugDrawCommandList)
{
    Handle<Node> targetNode = m_assetDropState.targetNode.Lock();

    if (!targetNode.IsValid())
    {
        return;
    }

    const BoundingBox worldBounds = targetNode->GetWorldBounds();

    if (worldBounds.IsFinite())
    {
        debugDrawCommandList.box(worldBounds.GetCenter(), worldBounds.GetExtent() * 0.5f * 1.02f + Vec3f(0.01f), Color::Yellow());
    }
}

void EditorSubsystem::UpdateNormalizedCubeSpherePreview(uint32 numDivisions)
{
    AssertOnThread(g_simThread);

    numDivisions = MathUtil::Max(numDivisions, 1u);

    Handle<Scene> activeScene = GetActiveScene();
    if (!activeScene.IsValid())
    {
        return;
    }

    Handle<Mesh> mesh = MeshBuilder::NormalizedCubeSphere(numDivisions);
    mesh->SetIsTransient(true);
    mesh->SetName(NAME("NormalizedCubeSphereMesh_Preview"));

    GetCurrentAssetRegistry()->PutAssetUnique(mesh);

    InitObject(mesh);

    if (!m_meshPreviewEntity.IsValid())
    {
        const Vec3f insertionPoint = CalculateSceneInsertionPoint(5.0f, 0.5f);

        MaterialAttributes attributes;
        attributes.shaderName = NAME("GeometryPass");

        m_meshPreviewMaterial = MakeHandle<Material>(NAME("NormalizedCubeSpherePreviewMaterial"), attributes);
        m_meshPreviewMaterial->SetIsTransient(true);
        InitObject(m_meshPreviewMaterial);

        m_meshPreviewEntity = MakeHandle<Entity>();
        m_meshPreviewEntity->SetName(NAME("NormalizedCubeSpherePreviewEntity"));
        m_meshPreviewEntity->SetWorldTranslation(insertionPoint);

        activeScene->GetRoot()->AddChild(m_meshPreviewEntity);

        MeshComponent meshComponent;
        meshComponent.mesh = mesh;
        meshComponent.material = m_meshPreviewMaterial;
        m_meshPreviewEntity->AddComponent<MeshComponent>(meshComponent);
    }
    else if (MeshComponent* meshComponent = m_meshPreviewEntity->TryGetComponent<MeshComponent>())
    {
        meshComponent->mesh = mesh;
        m_meshPreviewEntity->AddTag<EntityTag::UpdateRenderProxy>();
    }

    m_meshPreviewEntity->SetLocalBounds(mesh->GetAABB());
}

void EditorSubsystem::CommitMeshPreview()
{
    AssertOnThread(g_simThread);

    if (!m_meshPreviewEntity.IsValid())
    {
        return;
    }

    Handle<Entity> entity = m_meshPreviewEntity;

    m_meshPreviewEntity->Remove();
    m_meshPreviewEntity.Reset();

    m_meshPreviewMaterial.Reset();

    Handle<EditorProject> currentProject = GetCurrentProject();
    if (!currentProject.IsValid())
    {
        HYP_LOG(Editor, Error, "No project loaded; cannot commit mesh preview!");

        entity->Remove();

        return;
    }

    MeshComponent* meshComponent = entity->TryGetComponent<MeshComponent>();
    if (!meshComponent || !meshComponent->mesh.IsValid())
    {
        entity->Remove();

        return;
    }

    Handle<Mesh> mesh = meshComponent->mesh;

    // Replace the transient preview material with a proper, non-transient one for the committed entity
    MaterialAttributes attributes;
    attributes.shaderName = NAME("GeometryPass");

    Handle<Material> material = MakeHandle<Material>(NAME("NormalizedCubeSphereMaterial"), attributes);
    InitObject(material);

    meshComponent->material = material;

    entity->SetName(NAME("NormalizedCubeSphereEntity"));

    mesh->SetName(NAME("NormalizedCubeSphereMesh"));

    Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
        "Add Normalized Cube Sphere",
        Proc<EditorActionFunctions()>(
            [entity, mesh, material]() -> EditorActionFunctions
            {
                return EditorActionFunctions {
                    .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                        [entity, mesh, material](EditorSubsystem* subsystem, EditorProject*)
                        {
                            GetCurrentAssetRegistry()->PutAsset(mesh);
                            GetCurrentAssetRegistry()->PutAsset(material);

                            Handle<Scene> activeScene = subsystem->GetActiveScene();
                            if (activeScene.IsValid())
                            {
                                activeScene->GetRoot()->AddChild(entity);
                            }
                        }),
                    .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                        [entity, mesh, material](EditorSubsystem*, EditorProject*)
                        {
                            GetCurrentAssetRegistry()->RemoveAsset(mesh);
                            GetCurrentAssetRegistry()->RemoveAsset(material);

                            entity->Remove();
                        })
                };
            }));

    InitObject(action);

    currentProject->GetActionStack()->PushAction(action);
}

void EditorSubsystem::CancelMeshPreview()
{
    AssertOnThread(g_simThread);

    if (m_meshPreviewEntity.IsValid())
    {
        m_meshPreviewEntity->Remove();
        m_meshPreviewEntity = Handle<Entity>::Null();
    }

    m_meshPreviewMaterial = Handle<Material>::Null();
}

void EditorSubsystem::SetHoveredGizmo(
    const MouseEvent& event,
    EditorGizmoBase* gizmo,
    const Handle<Node>& gizmoNode)
{
    m_gizmoController->SetHoveredGizmo(event, gizmo, gizmoNode);
}

void EditorSubsystem::SetActiveScene(const Handle<Scene>& scene)
{
    if (scene == m_activeScene)
    {
        return;
    }

    m_activeScene = scene;

    OnActiveSceneChanged(scene);
}

EditorViewport* EditorSubsystem::GetActiveViewport() const
{
    AssertOnThread(g_simThread);

    return m_editorViewports.Empty() ? nullptr : m_editorViewports[0];
}

void EditorSubsystem::SetActiveViewport(EditorViewport* viewport)
{
    AssertOnThread(g_simThread);

    if (!viewport)
    {
        return;
    }

    auto it = m_editorViewports.Find(viewport);
    if (it != m_editorViewports.End())
    {
        Handle<EditorViewport> viewportStrong = MakeStrongRef(viewport);
        m_editorViewports.PushFront(viewportStrong);
        OnActiveViewportChanged(viewportStrong);
        return;
    }

    const size_t idx = m_editorViewports.IndexOf(it);
    AssertDebug(idx != -1);

    if (idx == 0)
    {
        return; // already active VP
    }

    std::swap(m_editorViewports[0], m_editorViewports[idx]);

    OnActiveViewportChanged(MakeStrongRef(viewport));
}

void EditorSubsystem::AddViewport(const Handle<EditorViewport>& viewport)
{
    AssertOnThread(g_simThread);

    Assert(viewport != nullptr);

    if (!viewport)
    {
        return;
    }

    InitObject(viewport);
    Handle<EditorViewport> viewportStrong = MakeStrongRef(viewport);

    viewport->OnAdded(this);
    m_editorViewports.PushBack(viewportStrong);

    // active VP is always the first one in the array
    // if size == 1 it's because we just added the first one
    if (m_editorViewports.Size() == 1)
    {
        OnActiveViewportChanged(viewportStrong);
    }
}

void EditorSubsystem::RemoveViewport(EditorViewport* viewport)
{
    AssertOnThread(g_simThread);

    Assert(viewport != nullptr);

    if (!viewport)
    {
        return;
    }

    auto it = m_editorViewports.Find(viewport);
    if (it != m_editorViewports.End())
    {
        Handle<EditorViewport> viewportStrong;

        const size_t idx = m_editorViewports.IndexOf(it);

        if (idx == 0)
        {
            viewportStrong = MakeStrongRef(*it);
        }

        m_editorViewports.Erase(it);

        if (viewportStrong)
        {
            OnActiveViewportChanged(viewportStrong);
        }

        viewport->OnRemoved(this);
    }
}

void EditorSubsystem::InitializeProjectWorld(const Handle<EditorProject>& project, bool isStartSimulation)
{
    Assert(project != nullptr);
    InitObject(project);
    
    g_editorState->GetPickCache().Clear();

    InitializeGizmos();

    Game* gameInstance = project->GetGame();
    Assert(gameInstance != nullptr);

    const Handle<AssetRegistry>& assetRegistry = gameInstance->GetAssetRegistry();
    Assert(assetRegistry.IsValid());
    PushAssetRegistry(assetRegistry);

    Handle<World> world;

    if (isStartSimulation)
    {
        // Loads the world
        gameInstance->Initialize();

        world = gameInstance->GetWorld();
    }
    else
    {
        world = project->GetWorld();

        if (!world.IsValid())
        {
            if ((world = gameInstance->LoadWorld(Game::s_nameMainWorld)) && world.IsValid())
            {
                world->SetGame(gameInstance);
            }
        }

        project->SetEditWorld(world);

        // This world was kept alive across the simulation transition, but Game::Shutdown() evicted every
        // asset from the registry cache so we need to re-register all nested asset objects
        if (world.IsValid() && !world->IsTransient())
        {
            assetRegistry->Initialize();
            assetRegistry->PutAssetsDeep(world);

            if (const Handle<WorldGrid>& worldGrid = world->GetWorldGrid(); worldGrid.IsValid())
            {
                for (const Handle<WorldGridLayer>& layer : worldGrid->GetLayers())
                {
                    layer->EnsureStreamingObjectsRegistered();
                }
            }
        }
    }

    Assert(world.IsValid());

    g_engineDriver->AddWorld(world);

    if (!isStartSimulation)
    {
        // sprites go in the editor scene (not saved), and only exist for the edit world so they're hidden while simulating
        world->AddSystem(MakeHandle<EditorSpriteSystem>(m_editorScene));

        // restarts the grid shut down for the simulation - see ShutdownProjectWorld()
        if (const Handle<WorldGrid>& worldGrid = world->GetWorldGrid(); worldGrid.IsValid())
        {
            worldGrid->Restart();
        }
    }

    Handle<Scene> activeScene;

    for (const Handle<Scene>& scene : world->GetScenes())
    {
        Assert(scene != nullptr);

        HYP_LOG(Editor, Verbose, "Found scene '{}' in project '{}' with flags: {}", *scene->GetName(), *project->GetName(),
                EnumToString(scene->GetSceneFlags()));

        if ((scene->GetSceneFlags() & (SceneFlags::FOREGROUND | SceneFlags::UI | SceneFlags::DETACHED)) != SceneFlags::FOREGROUND)
        {
            continue;
        }

        if (!activeScene.IsValid())
        {
            activeScene = scene;
        }
    }

    if (!activeScene.IsValid())
    {
        HYP_LOG(Editor, Warning, "No foreground scenes found in project {}!", *project->GetName());
    }

    if (!isStartSimulation)
    {
        for (const Handle<EditorViewport>& vp : m_editorViewports)
        {
            vp->OnAdded(this);
        }
    }

    m_delegateHandlers.Add(world->OnSceneAdded.Bind(
        world.Get(),
        [this, projectWeak = project.ToWeak(), isStartSimulation](World*, const Handle<Scene>& scene)
        {
            Assert(scene != nullptr);
            Assert(scene != m_editorScene);

            if ((scene->GetSceneFlags() & (SceneFlags::FOREGROUND | SceneFlags::UI | SceneFlags::DETACHED)) != SceneFlags::FOREGROUND)
            {
                return;
            }

            Handle<EditorProject> project = projectWeak.Lock();
            Assert(project != nullptr);

            if (!isStartSimulation)
            {
                // Add scene to all editor views
                for (const Handle<EditorViewport>& vp : m_editorViewports)
                {
                    vp->OnSceneAdded(scene);
                }
            }

            if (!m_activeScene)
            {
                SetActiveScene(scene);
            }
        }));

    m_delegateHandlers.Add(world->OnSceneRemoved.Bind(
        world.Get(),
        [this, projectWeak = project.ToWeak(), isStartSimulation](World*, Scene* scene)
        {
            Assert(scene != nullptr);
            Assert(scene != m_editorScene);

            Handle<EditorProject> project = projectWeak.Lock();
            Assert(project != nullptr);

            scene->OnRootNodeChanged.RemoveAllFromSet(m_delegateHandlers);

            if (!isStartSimulation)
            {
                // remove from all editor views
                for (const Handle<EditorViewport>& vp : m_editorViewports)
                {
                    vp->OnSceneRemoved(scene);
                }
            }

            // StopWatchingNode(scene->GetRoot());

            // GetWorld()->RemoveScene(scene);

            // // reinitialize scene selector on scene remove
            // InitActiveSceneSelection();
        }));

    BakerSubsystem* bakerSubsystem = world->GetSubsystem<BakerSubsystem>();

    if (!bakerSubsystem)
    {
        bakerSubsystem = world->AddSubsystem<BakerSubsystem>();
    }

    SetActiveScene(activeScene);
}

void EditorSubsystem::UpdateBakeStatus()
{
    if (IsSimulating())
    {
        return;
    }

    if (m_bakeStatusUpdateTimer.Waiting())
    {
        return;
    }

    m_bakeStatusUpdateTimer.NextTick();

    static const Name s_bakeStatusMessageKey = NAME("BakeStatus");

    if (!m_messagesOverlay.IsValid())
    {
        return;
    }

    if (!m_currentProject.IsValid())
    {
        m_messagesOverlay->ClearMessage(s_bakeStatusMessageKey);

        return;
    }

    const Handle<World>& world = m_currentProject->GetWorld();

    if (!world.IsValid())
    {
        m_messagesOverlay->ClearMessage(s_bakeStatusMessageKey);

        return;
    }

    // Epochs are computed from the live scene, which only reflects the active swatch's overrides,
    // so other swatches' stored epochs can never match here
    const Handle<Swatch> activeSwatch = world->TryGetSwatch(world->GetActiveSwatchName());

    Array<String, EditorAllocator> lightmapVolumeNames;
    Array<String, EditorAllocator> reflectionProbeNames;
    Array<String, EditorAllocator> irradianceProbeNames;
    Array<String, EditorAllocator> fogVolumeNames;

    for (const Handle<Scene>& scene : world->GetScenes())
    {
        if (!scene)
        {
            continue;
        }

        for (auto [volume] : scene->GetEntityManager()->GetEntitySet<EntityType<LightmapVolume>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            if (!volume->GetAtlasTexture(0, LightmapVolume::IrradianceTexture).IsValid())
            {
                lightmapVolumeNames.PushBack(*volume->GetName());

                continue;
            }

            if (!activeSwatch.IsValid())
            {
                continue;
            }

            Baking::BakeLayer& bakeLayer = activeSwatch->bakeLayer;

            uint64 storedEpoch;

            // not tracked yet counts as out of date. bake it to track it
            if (!bakeLayer.TryGetAssetEpoch<Baking::BakeLayerCategory::LightReceiver>(*volume, storedEpoch)
                || storedEpoch != Baking::BakeEpoch::ComputeEpoch(*volume, bakeLayer))
            {
                lightmapVolumeNames.PushBack(*volume->GetName());
            }
        }

        for (auto [probe] : scene->GetEntityManager()->GetEntitySet<EntityType<EnvProbe>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            if (!probe->IsBaked())
            {
                // Fine. no realtime probes are considered.
                continue;
            }

            Array<String, EditorAllocator>* outNames;

            if (DynamicCast<ReflectionProbe>(probe))
            {
                outNames = &reflectionProbeNames;
            }
            else if (DynamicCast<IrradianceProbe>(probe))
            {
                outNames = &irradianceProbeNames;
            }
            else
            {
                continue;
            }

            if (!activeSwatch.IsValid())
            {
                continue;
            }

            Baking::BakeLayer& bakeLayer = activeSwatch->bakeLayer;

            uint64 storedEpoch;

            // not tracked yet counts as out of date. bake it to track it
            if (!bakeLayer.TryGetAssetEpoch<Baking::BakeLayerCategory::LightReceiver>(*probe, storedEpoch)
                || storedEpoch != Baking::BakeEpoch::ComputeEpoch(*probe, bakeLayer))
            {
                outNames->PushBack(*probe->GetName());
            }
        }

        for (auto [volume] : scene->GetEntityManager()->GetEntitySet<EntityType<FogVolume>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            if (!volume->GetVolumeTexture().IsValid())
            {
                // Not baked yet, but we consider it 'out of date', so we dont have FogVolumes left unbaked in the scene.
                fogVolumeNames.PushBack(*volume->GetName());

                continue;
            }

            if (!activeSwatch.IsValid())
            {
                continue;
            }

            Baking::BakeLayer& bakeLayer = activeSwatch->bakeLayer;

            uint64 storedEpoch;

            // not tracked yet counts as out of date. bake it to track it
            if (!bakeLayer.TryGetAssetEpoch<Baking::BakeLayerCategory::LightReceiver>(*volume, storedEpoch)
                || storedEpoch != Baking::BakeEpoch::ComputeEpoch(*volume, bakeLayer))
            {
                fogVolumeNames.PushBack(*volume->GetName());
            }
        }
    }

    if (lightmapVolumeNames.Empty() && reflectionProbeNames.Empty() && irradianceProbeNames.Empty() && fogVolumeNames.Empty())
    {
        m_messagesOverlay->ClearMessage(s_bakeStatusMessageKey);

        return;
    }

    String text;

    auto appendSection = [&text](ANSIStringView label, Span<const String> names)
    {
        static constexpr uint32 MaxToShow = 5;

        if (names.Size() == 0)
        {
            return;
        }

        if (text.Any())
        {
            text += "\n\n";
        }

        text += HYP_FORMAT("{} {} need a rebake", names.Size(), label);

        const uint32 numToShow = MathUtil::Min(uint32(names.Size()), MaxToShow);

        for (uint32 i = 0; i < numToShow; i++)
        {
            text += "\n";
            text += names[i];
        }

        if (uint32(names.Size()) > numToShow)
        {
            text += HYP_FORMAT("\n... and {} more", uint32(names.Size()) - numToShow);
        }
    };

    appendSection("LightmapVolumes", lightmapVolumeNames);
    appendSection("ReflectionProbes", reflectionProbeNames);
    appendSection("IrradianceProbes", irradianceProbeNames);
    appendSection("FogVolumes", fogVolumeNames);

    m_messagesOverlay->PutMessage(MessageEntry {
        s_bakeStatusMessageKey,
        text,
        Color(1.0f, 0.7f, 0.1f, 1.0f)
    });
}

template <class Function>
static void ForEachBakedEnvProbe(World* world, Function&& function)
{
    for (const Handle<Scene>& scene : world->GetScenes())
    {
        if (!scene)
        {
            continue;
        }

        for (auto [probe] : scene->GetEntityManager()->GetEntitySet<EntityType<EnvProbe>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            // realtime probes rerender on their own
            if (probe->IsBaked())
            {
                function(probe);
            }
        }
    }
}

void EditorSubsystem::TrackBakedEnvProbePlacements()
{
    AssertOnThread(g_simThread);

    const Handle<World>& world = GetProjectWorld();

    if (IsSimulating() || !world.IsValid())
    {
        return;
    }

    ForEachBakedEnvProbe(world.Get(), [this](EnvProbe* probe)
        {
            auto committedIt = m_committedEnvProbePlacements.FindIf([probe](const EnvProbePlacement& placement)
                {
                    return placement.probe.GetUnsafe() == probe;
                });

            if (committedIt == m_committedEnvProbePlacements.End())
            {
                m_committedEnvProbePlacements.PushBack(EnvProbePlacement { MakeWeakRef(probe), probe->GetWorldTranslation(), probe->GetWorldBounds() });
            }
        });
}

void EditorSubsystem::RebakeMovedEnvProbes()
{
    AssertOnThread(g_simThread);

    const Handle<World>& world = GetProjectWorld();

    if (IsSimulating() || !world.IsValid())
    {
        return;
    }

    Array<EnvProbePlacement, EditorAllocator> currentPlacements;
    Array<Handle<ObjectBase>, EditorAllocator> movedProbes;

    ForEachBakedEnvProbe(world.Get(), [&](EnvProbe* probe)
        {
            const EnvProbePlacement currentPlacement { MakeWeakRef(probe), probe->GetWorldTranslation(), probe->GetWorldBounds() };

            auto committedIt = m_committedEnvProbePlacements.FindIf([probe](const EnvProbePlacement& placement)
                {
                    return placement.probe.GetUnsafe() == probe;
                });

            if (committedIt != m_committedEnvProbePlacements.End()
                && (committedIt->worldTranslation != currentPlacement.worldTranslation || committedIt->worldBounds != currentPlacement.worldBounds))
            {
                movedProbes.PushBack(MakeStrongRef(probe));
            }

            currentPlacements.PushBack(currentPlacement);
        });

    m_committedEnvProbePlacements = std::move(currentPlacements);

    if (movedProbes.Empty())
    {
        return;
    }

    const Handle<Scene> activeScene = GetActiveScene();

    if (!activeScene.IsValid())
    {
        return;
    }

    if (BakerSubsystem* bakerSubsystem = world->GetSubsystem<BakerSubsystem>())
    {
        for (const Handle<ObjectBase>& probe : movedProbes)
        {
            bakerSubsystem->CancelBake(probe.Get());
        }
    }

    Handle<GenerateLightmapsEditorTask> editorTask = MakeHandle<GenerateLightmapsEditorTask>(movedProbes);
    editorTask->SetIsForegroundTask(true);
    InitObject(editorTask);

    editorTask->SetScene(activeScene);
    editorTask->SetWorld(world);

    g_editorState->AddTask(editorTask);
}

void EditorSubsystem::ShutdownProjectWorld(const Handle<EditorProject>& project, bool shutdownWorld)
{
    Assert(project.IsValid());

    Game* gameInstance = project->GetGame();
    Assert(gameInstance != nullptr);

    const Handle<World>& world = project->GetWorld();
    Assert(world.IsValid());

    g_editorState->GetPickCache().Clear();

    // Shutdown to reinitialize gizmos after project is opened
    ShutdownGizmos();

    m_focusedNode.Reset();
    m_selectedNodes.Clear();

    ClearAssetDropTarget();

    if (m_highlightNode.IsValid())
    {
        m_highlightNode->Remove();
    }

    SetActiveScene(Handle<Scene>::Null());

    // Must run before RemoveWorld() below -- if shutdownWorld is true, World::Shutdown() moves
    // m_scenes out from under the World, so world->GetScenes() would come back empty afterward.
    for (const Handle<Scene>& scene : world->GetScenes())
    {
        if (!scene.IsValid())
        {
            continue;
        }

        scene->OnRootNodeChanged.RemoveAllFromSet(m_delegateHandlers);
    }

    const bool isSimulationProject = IsSimulating() && project != m_preSimulationProject;

    if (!isSimulationProject)
    {
        for (const Handle<EditorViewport>& vp : m_editorViewports)
        {
            vp->OnRemoved(this);
        }
    }


    BakerSubsystem* bakerSubsystem = world->GetSubsystem<BakerSubsystem>();
    if (bakerSubsystem)
    {
        //bakerSubsystem->CancelAllBakes();
    }

    world->OnSceneAdded.RemoveAllFromSet(m_delegateHandlers);
    world->OnSceneRemoved.RemoveAllFromSet(m_delegateHandlers);

    gameInstance->OnGameStateChange.RemoveAllFromSet(m_delegateHandlers);

    // a world kept alive for the simulation would otherwise keep streaming its layers, which store generated data
    // (eg. terrain cells) into the simulation's asset registry
    if (!shutdownWorld)
    {
        if (const Handle<WorldGrid>& worldGrid = world->GetWorldGrid(); worldGrid.IsValid())
        {
            worldGrid->Shutdown();
        }
    }

    if (EditorSpriteSystem* spriteSystem = world->GetSystem<EditorSpriteSystem>())
    {
        world->RemoveSystem(spriteSystem);
    }

    g_engineDriver->RemoveWorld(world, shutdownWorld);

    PopAssetRegistry(gameInstance->GetAssetRegistry().Get());
}

#endif

#pragma endregion EditorSubsystem

} // namespace Hyperion
