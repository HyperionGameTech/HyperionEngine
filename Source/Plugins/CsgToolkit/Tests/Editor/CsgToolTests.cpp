/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#ifdef HYP_TESTS

#include <Framework/Testing/EngineTest.hpp>

#include <CsgEditor/CsgEditorExtension.hpp>
#include <CsgEditor/EditorCsgState.hpp>
#include <Csg/MeshBoolean.hpp>

#include <Editor/EditorState.hpp>
#include <Editor/EditorSubsystem.hpp>
#include <Editor/EditorProject.hpp>
#include <Editor/EditorActionStack.hpp>
#include <Editor/Decal/EditorDecalPainterState.hpp>

#include <Scene/Scene.hpp>
#include <Scene/Entity.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/Components/MeshComponent.hpp>

#include <Rendering/Mesh.hpp>
#include <Rendering/Material.hpp>
#include <Rendering/Util/MeshBuilder.hpp>

#include <Input/Keyboard.hpp>

#include <Core/Math/MathUtil.hpp>

namespace Hyperion {
namespace tests {
namespace csg {

namespace {

constexpr double CubeVolume = 8.0;
constexpr double BrushVolumeInsideCube = 0.5;
constexpr double VolumeTolerance = 0.001;

// above the new project's ground and player
constexpr float TestHeight = 100.0f;

EditorSubsystem* GetEditorSubsystem()
{
    return EditorState::GetInstance()->GetEditorSubsystem().Get();
}

Handle<CsgEditorExtension> GetCsgExtension()
{
    return GetEditorSubsystem()->GetExtension<CsgEditorExtension>();
}

Handle<EditorCsgState> GetCsgState()
{
    Handle<CsgEditorExtension> extension = GetCsgExtension();

    if (!extension.IsValid())
    {
        return Handle<EditorCsgState>::empty;
    }

    return extension->GetCsgState();
}

// a test that stopped early can leave a session open, and opening the tool again does nothing then
Handle<EditorCsgState> GetClosedCsgState()
{
    Handle<EditorCsgState> csgState = GetCsgState();

    if (csgState.IsValid())
    {
        csgState->Exit(/* saveEdits */ false);
    }

    return csgState;
}

Handle<Entity> AddMeshEntity(Name name, const Handle<Mesh>& mesh, const Vec3f& position)
{
    EditorSubsystem* editorSubsystem = GetEditorSubsystem();

    MaterialAttributes materialAttributes;
    materialAttributes.shaderName = NAME("GeometryPass");

    Handle<Entity> entity = MakeHandle<Entity>();
    entity->SetName(name);
    entity->SetLocalTranslation(position);

    editorSubsystem->GetActiveScene()->GetRoot()->AddChild(entity);

    MeshComponent meshComponent;
    meshComponent.mesh = mesh;
    meshComponent.material = MakeHandle<Material>(NAME("CsgTestMaterial"), materialAttributes);

    entity->AddComponent<MeshComponent>(meshComponent);
    entity->SetLocalBounds(mesh->GetAABB());

    return entity;
}

// a 2x2x2 cube centered on the entity
Handle<Entity> AddCube(Name name, const Vec3f& position = Vec3f(0.0f, TestHeight, 0.0f))
{
    return AddMeshEntity(name, MeshBuilder::Cube(), position);
}

void RemoveNode(const Handle<Entity>& entity)
{
    EditorSubsystem* editorSubsystem = GetEditorSubsystem();

    if (Handle<EditorCsgState> csgState = GetCsgState(); csgState.IsValid())
    {
        csgState->Exit(/* saveEdits */ false);
    }

    editorSubsystem->ExitMeshEditMode(/* saveEdits */ false);
    editorSubsystem->SetFocusedNode(Handle<Node>::Null(), /* shouldSelectInOutline */ false);

    entity->Remove();
}

Handle<Mesh> GetMesh(const Handle<Entity>& entity)
{
    const MeshComponent* meshComponent = entity->TryGetComponent<MeshComponent>();

    if (!meshComponent)
    {
        return Handle<Mesh>::empty;
    }

    return meshComponent->mesh;
}

double GetMeshVolume(const Handle<Mesh>& mesh)
{
    if (!mesh.IsValid())
    {
        return 0.0;
    }

    auto readScope = mesh->GetReadScope();

    const VertexArrayView vertices = mesh->GetVertexData(0);
    const Span<const ubyte> indexBytes = mesh->GetIndexData(0);

    if (!vertices.floatData || !indexBytes.Data())
    {
        return 0.0;
    }

    const size_t stride = vertices.layoutDesc.VertexSize() / sizeof(float);
    const uint32* indices = reinterpret_cast<const uint32*>(indexBytes.Data());
    const size_t numIndices = indexBytes.Size() / sizeof(uint32);

    double volume = 0.0;

    for (size_t triangleStart = 0; triangleStart + 2 < numIndices; triangleStart += 3)
    {
        const float* p0 = vertices.floatData + indices[triangleStart + 0] * stride;
        const float* p1 = vertices.floatData + indices[triangleStart + 1] * stride;
        const float* p2 = vertices.floatData + indices[triangleStart + 2] * stride;

        volume += double(p0[0]) * (double(p1[1]) * double(p2[2]) - double(p1[2]) * double(p2[1]))
            - double(p0[1]) * (double(p1[0]) * double(p2[2]) - double(p1[2]) * double(p2[0]))
            + double(p0[2]) * (double(p1[0]) * double(p2[1]) - double(p1[1]) * double(p2[0]));
    }

    return MathUtil::Abs(volume / 6.0);
}

// focuses the cube, enters the tool and places a 1x1x1 box brush, which lands centered on the cube's top face, so half of it is inside
bool EnterWithBoxBrush(EngineTestContext& context, const Handle<Entity>& cube, CsgOperation operation)
{
    Handle<EditorCsgState> csgState = GetCsgState();

    GetEditorSubsystem()->SetFocusedNode(cube, /* shouldSelectInOutline */ false);

    csgState->Enter();

    if (!HYP_TEST_CHECK(csgState->IsEnabled()))
    {
        return false;
    }

    csgState->SetOperation(operation);
    csgState->AddBrush(CsgBrushShape::Box);
    csgState->SetSizingHalfExtents(Vec3f(0.5f));

    return HYP_TEST_CHECK(csgState->CanApply());
}

KeyboardEvent MakeKeyEvent(KeyCode keyCode)
{
    KeyboardEvent event;
    event.keyCode = keyCode;

    return event;
}

// enters the tool with nothing selected and puts a box brush at center
void EnterWithBrushAt(const Handle<EditorCsgState>& csgState, CsgOperation operation, const Vec3f& center, const Vec3f& halfExtent)
{
    csgState->Enter();
    csgState->SetOperation(operation);
    csgState->AddBrush(CsgBrushShape::Box);

    if (Handle<Node> brushNode = GetEditorSubsystem()->GetFocusedNode(); brushNode.IsValid())
    {
        brushNode->SetLocalTransform(Transform(center, halfExtent, Quat4f::Identity()));
    }

    csgState->Update();
}

String DescribeTargets(const Handle<EditorCsgState>& csgState)
{
    String description;

    for (uint32 index = 0; index < csgState->GetTargetCount(); index++)
    {
        description += HYP_FORMAT("{}{}{} ", csgState->GetTargetName(index), csgState->IsTargetIncluded(index) ? "+" : "-", csgState->GetTargetIssue(index));
    }

    return description;
}

int32 FindTargetIndex(const Handle<EditorCsgState>& csgState, const Handle<Entity>& entity)
{
    for (uint32 index = 0; index < csgState->GetTargetCount(); index++)
    {
        if (csgState->GetTargetNodeAt(index) == entity.Get())
        {
            return int32(index);
        }
    }

    return -1;
}

} // namespace

HYP_EDITOR_TEST(CsgTools, ExtensionIsAttached)
{
    HYP_TEST_REQUIRE(GetCsgExtension().IsValid());
    HYP_TEST_CHECK(GetCsgState().IsValid());
    HYP_TEST_CHECK(MeshBoolean::IsSupported());
}

HYP_EDITOR_TEST(CsgTools, EnterTargetsFocusedMesh)
{
    EditorSubsystem* editorSubsystem = GetEditorSubsystem();
    Handle<EditorCsgState> csgState = GetClosedCsgState();
    HYP_TEST_REQUIRE(csgState.IsValid());

    Handle<Entity> cube = AddCube(NAME("CsgEnterCube"));

    editorSubsystem->SetFocusedNode(cube, /* shouldSelectInOutline */ false);
    HYP_TEST_CHECK(csgState->CanEnter());

    csgState->Enter();

    HYP_TEST_CHECK(csgState->IsEnabled());
    HYP_TEST_CHECK(csgState->GetTargetCount() == 1);
    HYP_TEST_CHECK(csgState->GetTargetNodeAt(0) == cube.Get());
    HYP_TEST_CHECK(csgState->IsPlacing());
    HYP_TEST_CHECK(editorSubsystem->IsExtensionToolActive());
    HYP_TEST_CHECK(!editorSubsystem->CanUseVolumeEditTool());
    HYP_TEST_CHECK(editorSubsystem->GetSelectedManipulationMode() == EditorManipulationMode::None);
    HYP_TEST_CHECK(editorSubsystem->GetGizmoTargetNodes().Empty());

    csgState->Exit(/* saveEdits */ true);

    HYP_TEST_CHECK(!csgState->IsEnabled());
    HYP_TEST_CHECK(!editorSubsystem->IsExtensionToolActive());

    RemoveNode(cube);
}

HYP_EDITOR_TEST(CsgTools, EntersWithoutMeshSelected)
{
    Handle<EditorCsgState> csgState = GetClosedCsgState();
    HYP_TEST_REQUIRE(csgState.IsValid());

    Handle<Entity> entity = MakeHandle<Entity>();
    entity->SetName(NAME("CsgNoMesh"));
    GetEditorSubsystem()->GetActiveScene()->GetRoot()->AddChild(entity);

    GetEditorSubsystem()->SetFocusedNode(entity, /* shouldSelectInOutline */ false);

    HYP_TEST_CHECK(csgState->CanEnter());

    csgState->Enter();

    HYP_TEST_CHECK(csgState->IsEnabled());
    HYP_TEST_CHECK(csgState->IsPlacing());
    HYP_TEST_CHECK(csgState->GetTargetCount() == 0);

    RemoveNode(entity);

    HYP_TEST_CHECK(!csgState->IsEnabled());
}

HYP_EDITOR_TEST(CsgTools, BrushTargetsOverlappedMeshes)
{
    Handle<EditorCsgState> csgState = GetClosedCsgState();
    HYP_TEST_REQUIRE(csgState.IsValid());

    const Handle<EditorActionStack>& actionStack = GetEditorSubsystem()->GetCurrentProject()->GetActionStack();
    HYP_TEST_REQUIRE(actionStack.IsValid());

    Handle<Entity> leftCube = AddCube(NAME("CsgLeftCube"));
    Handle<Entity> rightCube = AddCube(NAME("CsgRightCube"), Vec3f(3.0f, TestHeight, 0.0f));
    Handle<Entity> farCube = AddCube(NAME("CsgFarCube"), Vec3f(30.0f, TestHeight, 0.0f));

    const Handle<Mesh> leftMesh = GetMesh(leftCube);
    const Handle<Mesh> rightMesh = GetMesh(rightCube);
    const Handle<Mesh> farMesh = GetMesh(farCube);

    GetEditorSubsystem()->SetFocusedNode(Handle<Node>::Null(), /* shouldSelectInOutline */ false);

    // spans x 0.5 to 2.5, so half a unit of each near cube
    EnterWithBrushAt(csgState, CsgOperation::Subtract, Vec3f(1.5f, TestHeight, 0.0f), Vec3f(1.0f, 0.5f, 0.5f));

    HYP_TEST_CHECK_MSG(csgState->GetTargetCount() == 2, "targets: {}", DescribeTargets(csgState));
    HYP_TEST_CHECK(FindTargetIndex(csgState, leftCube) >= 0);
    HYP_TEST_CHECK(FindTargetIndex(csgState, rightCube) >= 0);
    HYP_TEST_CHECK(FindTargetIndex(csgState, farCube) < 0);

    for (uint32 index = 0; index < csgState->GetTargetCount(); index++)
    {
        HYP_TEST_CHECK(csgState->IsTargetIncluded(index));
    }

    HYP_TEST_REQUIRE(csgState->CanApply());

    csgState->ApplyBrush();

    HYP_TEST_CHECK_MSG(csgState->GetStatusText().Empty(), "{}", csgState->GetStatusText());
    HYP_TEST_CHECK(MathUtil::Abs(GetMeshVolume(GetMesh(leftCube)) - (CubeVolume - 0.5)) <= VolumeTolerance);
    HYP_TEST_CHECK(MathUtil::Abs(GetMeshVolume(GetMesh(rightCube)) - (CubeVolume - 0.5)) <= VolumeTolerance);
    HYP_TEST_CHECK(GetMesh(farCube) == farMesh);

    HYP_TEST_REQUIRE(actionStack->CanUndo());
    actionStack->Undo();

    HYP_TEST_CHECK(GetMesh(leftCube) == leftMesh);
    HYP_TEST_CHECK(GetMesh(rightCube) == rightMesh);

    HYP_TEST_REQUIRE(actionStack->CanRedo());
    actionStack->Redo();

    HYP_TEST_CHECK(GetMesh(leftCube) != leftMesh);
    HYP_TEST_CHECK(GetMesh(rightCube) != rightMesh);

    csgState->Exit(/* saveEdits */ false);

    HYP_TEST_CHECK(GetMesh(leftCube) == leftMesh);
    HYP_TEST_CHECK(GetMesh(rightCube) == rightMesh);

    RemoveNode(farCube);
    RemoveNode(rightCube);
    RemoveNode(leftCube);
}

HYP_EDITOR_TEST(CsgTools, UnionDefaultsToPrimaryTarget)
{
    Handle<EditorCsgState> csgState = GetClosedCsgState();
    HYP_TEST_REQUIRE(csgState.IsValid());

    Handle<Entity> leftCube = AddCube(NAME("CsgPrimaryCube"));
    Handle<Entity> rightCube = AddCube(NAME("CsgSecondaryCube"), Vec3f(3.0f, TestHeight, 0.0f));

    GetEditorSubsystem()->SetFocusedNode(leftCube, /* shouldSelectInOutline */ false);

    EnterWithBrushAt(csgState, CsgOperation::Union, Vec3f(1.5f, TestHeight, 0.0f), Vec3f(1.0f, 0.5f, 0.5f));

    const int32 leftIndex = FindTargetIndex(csgState, leftCube);
    const int32 rightIndex = FindTargetIndex(csgState, rightCube);

    HYP_TEST_REQUIRE(leftIndex >= 0 && rightIndex >= 0);

    HYP_TEST_CHECK(csgState->IsTargetIncluded(uint32(leftIndex)));
    HYP_TEST_CHECK(!csgState->IsTargetIncluded(uint32(rightIndex)));

    csgState->SetOperation(CsgOperation::Subtract);

    HYP_TEST_CHECK(csgState->IsTargetIncluded(uint32(leftIndex)));
    HYP_TEST_CHECK(csgState->IsTargetIncluded(uint32(rightIndex)));

    csgState->SetOperation(CsgOperation::Union);
    csgState->SetTargetIncluded(uint32(rightIndex), true);

    HYP_TEST_CHECK(csgState->IsTargetIncluded(uint32(FindTargetIndex(csgState, rightCube))));

    RemoveNode(rightCube);
    RemoveNode(leftCube);
}

HYP_EDITOR_TEST(CsgTools, OpenSurfaceIsListedButSkipped)
{
    Handle<EditorCsgState> csgState = GetClosedCsgState();
    HYP_TEST_REQUIRE(csgState.IsValid());

    Handle<Entity> cube = AddCube(NAME("CsgSolidCube"));
    Handle<Entity> quad = AddMeshEntity(NAME("CsgOpenQuad"), MeshBuilder::Quad(), Vec3f(1.5f, TestHeight, 0.0f));

    const Handle<Mesh> quadMesh = GetMesh(quad);

    GetEditorSubsystem()->SetFocusedNode(Handle<Node>::Null(), /* shouldSelectInOutline */ false);

    EnterWithBrushAt(csgState, CsgOperation::Subtract, Vec3f(1.5f, TestHeight, 0.0f), Vec3f(1.0f, 0.5f, 0.5f));

    const int32 cubeIndex = FindTargetIndex(csgState, cube);
    const int32 quadIndex = FindTargetIndex(csgState, quad);

    HYP_TEST_REQUIRE(cubeIndex >= 0 && quadIndex >= 0);

    HYP_TEST_CHECK(csgState->GetTargetIssue(uint32(cubeIndex)).Empty());
    HYP_TEST_CHECK(csgState->IsTargetIncluded(uint32(cubeIndex)));

    HYP_TEST_CHECK(csgState->GetTargetIssue(uint32(quadIndex)).Any());
    HYP_TEST_CHECK(!csgState->IsTargetIncluded(uint32(quadIndex)));

    csgState->ApplyBrush();

    HYP_TEST_CHECK(MathUtil::Abs(GetMeshVolume(GetMesh(cube)) - (CubeVolume - 0.5)) <= VolumeTolerance);
    HYP_TEST_CHECK(GetMesh(quad) == quadMesh);

    RemoveNode(quad);
    RemoveNode(cube);
}

HYP_EDITOR_TEST(CsgTools, UnionInEmptySpaceCreatesMesh)
{
    Handle<EditorCsgState> csgState = GetClosedCsgState();
    HYP_TEST_REQUIRE(csgState.IsValid());

    const Handle<EditorActionStack>& actionStack = GetEditorSubsystem()->GetCurrentProject()->GetActionStack();
    HYP_TEST_REQUIRE(actionStack.IsValid());

    GetEditorSubsystem()->SetFocusedNode(Handle<Node>::Null(), /* shouldSelectInOutline */ false);

    EnterWithBrushAt(csgState, CsgOperation::Union, Vec3f(200.0f, 200.0f, 200.0f), Vec3f(0.5f));

    HYP_TEST_CHECK(csgState->GetTargetCount() == 0);
    HYP_TEST_REQUIRE(csgState->CanApply());

    csgState->ApplyBrush();

    HYP_TEST_REQUIRE(csgState->GetTargetCount() == 1);

    Node* createdNodePtr = csgState->GetTargetNodeAt(0);
    HYP_TEST_REQUIRE(createdNodePtr != nullptr);

    Handle<Node> createdNode = MakeStrongRef(createdNodePtr);

    Entity* createdEntity = TryCast<Entity>(createdNode.Get());
    const MeshComponent* meshComponent = createdEntity ? createdEntity->TryGetComponent<MeshComponent>() : nullptr;

    HYP_TEST_REQUIRE(meshComponent != nullptr);
    HYP_TEST_CHECK(createdNode->GetParent() != nullptr);
    HYP_TEST_CHECK(MathUtil::Abs(GetMeshVolume(meshComponent->mesh) - 1.0) <= VolumeTolerance);
    HYP_TEST_CHECK(csgState->HasPendingEdits());

    HYP_TEST_REQUIRE(actionStack->CanUndo());
    actionStack->Undo();

    HYP_TEST_CHECK(createdNode->GetParent() == nullptr);

    HYP_TEST_REQUIRE(actionStack->CanRedo());
    actionStack->Redo();

    HYP_TEST_CHECK(createdNode->GetParent() != nullptr);

    csgState->Exit(/* saveEdits */ false);

    HYP_TEST_CHECK(createdNode->GetParent() == nullptr);
}

HYP_EDITOR_TEST(CsgTools, PinnedTargetsStayWhenBrushMoves)
{
    Handle<EditorCsgState> csgState = GetClosedCsgState();
    HYP_TEST_REQUIRE(csgState.IsValid());

    Handle<Entity> cube = AddCube(NAME("CsgPinnedCube"));

    GetEditorSubsystem()->SetFocusedNode(Handle<Node>::Null(), /* shouldSelectInOutline */ false);

    EnterWithBrushAt(csgState, CsgOperation::Subtract, Vec3f(0.0f, TestHeight + 1.0f, 0.0f), Vec3f(0.5f));

    HYP_TEST_REQUIRE(csgState->GetTargetCount() == 1);

    csgState->SetTargetsPinned(true);

    Handle<Node> brushNode = GetEditorSubsystem()->GetFocusedNode();
    HYP_TEST_REQUIRE(brushNode.IsValid());

    brushNode->SetLocalTransform(Transform(Vec3f(50.0f, TestHeight, 0.0f), Vec3f(0.5f), Quat4f::Identity()));
    csgState->Update();

    HYP_TEST_CHECK_MSG(csgState->GetTargetCount() == 1, "targets: {}", DescribeTargets(csgState));

    csgState->SetTargetsPinned(false);
    csgState->Update();

    HYP_TEST_CHECK(csgState->GetTargetCount() == 0);

    RemoveNode(cube);
}

HYP_EDITOR_TEST(CsgTools, OperationsChangeVolume)
{
    Handle<EditorCsgState> csgState = GetClosedCsgState();
    HYP_TEST_REQUIRE(csgState.IsValid());

    const struct
    {
        CsgOperation operation;
        double expectedVolume;
    } cases[] = {
        { CsgOperation::Union, CubeVolume + BrushVolumeInsideCube },
        { CsgOperation::Subtract, CubeVolume - BrushVolumeInsideCube },
        { CsgOperation::Intersect, BrushVolumeInsideCube }
    };

    for (const auto& testCase : cases)
    {
        const char* operationName = CsgOperationNames[uint32(testCase.operation)];

        Handle<Entity> cube = AddCube(NAME("CsgOperationCube"));
        const Handle<Mesh> originalMesh = GetMesh(cube);

        if (EnterWithBoxBrush(context, cube, testCase.operation))
        {
            const String targetsBefore = DescribeTargets(csgState);

            csgState->ApplyBrush();

            const Handle<Mesh> resultMesh = GetMesh(cube);
            const double volume = GetMeshVolume(resultMesh);

            HYP_TEST_CHECK_MSG(csgState->GetStatusText().Empty(), "{}: {}", operationName, csgState->GetStatusText());
            HYP_TEST_CHECK_MSG(resultMesh != originalMesh, "{}: the mesh wasn't replaced, targets: {}", operationName, targetsBefore);
            HYP_TEST_CHECK_MSG(MathUtil::Abs(volume - testCase.expectedVolume) <= VolumeTolerance,
                "{}: volume {} expected {}", operationName, volume, testCase.expectedVolume);
            HYP_TEST_CHECK_MSG(csgState->HasPendingEdits(), "{}: no pending edits", operationName);
            HYP_TEST_CHECK_MSG(!csgState->HasBrush(), "{}: the brush stayed after applying", operationName);
            HYP_TEST_CHECK_MSG(csgState->IsPlacing(), "{}: placement didn't restart after applying", operationName);

            csgState->Exit(/* saveEdits */ true);

            HYP_TEST_CHECK_MSG(GetMesh(cube) == resultMesh, "{}: saving the edits changed the mesh", operationName);
            HYP_TEST_CHECK_MSG(resultMesh->IsRegistered(), "{}: the result mesh isn't a registered asset", operationName);
        }

        RemoveNode(cube);
    }
}

HYP_EDITOR_TEST(CsgTools, KeepBrushAfterApply)
{
    Handle<EditorCsgState> csgState = GetClosedCsgState();
    HYP_TEST_REQUIRE(csgState.IsValid());

    Handle<Entity> cube = AddCube(NAME("CsgKeepBrushCube"));

    if (EnterWithBoxBrush(context, cube, CsgOperation::Subtract))
    {
        csgState->SetKeepBrushAfterApply(true);
        csgState->ApplyBrush();

        HYP_TEST_CHECK(csgState->HasBrush());
        HYP_TEST_CHECK(csgState->CanApply());

        csgState->SetKeepBrushAfterApply(false);
    }

    RemoveNode(cube);
}

HYP_EDITOR_TEST(CsgTools, DiscardRestoresMesh)
{
    Handle<EditorCsgState> csgState = GetClosedCsgState();
    HYP_TEST_REQUIRE(csgState.IsValid());

    Handle<Entity> cube = AddCube(NAME("CsgDiscardCube"));
    const Handle<Mesh> originalMesh = GetMesh(cube);

    if (EnterWithBoxBrush(context, cube, CsgOperation::Subtract))
    {
        csgState->ApplyBrush();
        HYP_TEST_CHECK(GetMesh(cube) != originalMesh);

        csgState->Exit(/* saveEdits */ false);

        HYP_TEST_CHECK(GetMesh(cube) == originalMesh);
        HYP_TEST_CHECK(MathUtil::Abs(GetMeshVolume(GetMesh(cube)) - CubeVolume) <= VolumeTolerance);
    }

    RemoveNode(cube);
}

HYP_EDITOR_TEST(CsgTools, UndoRedoApply)
{
    EditorSubsystem* editorSubsystem = GetEditorSubsystem();
    Handle<EditorCsgState> csgState = GetClosedCsgState();
    HYP_TEST_REQUIRE(csgState.IsValid());

    const Handle<EditorActionStack>& actionStack = editorSubsystem->GetCurrentProject()->GetActionStack();
    HYP_TEST_REQUIRE(actionStack.IsValid());

    Handle<Entity> cube = AddCube(NAME("CsgUndoCube"));
    const Handle<Mesh> originalMesh = GetMesh(cube);

    if (EnterWithBoxBrush(context, cube, CsgOperation::Subtract))
    {
        csgState->ApplyBrush();

        const Handle<Mesh> resultMesh = GetMesh(cube);

        HYP_TEST_REQUIRE(actionStack->CanUndo());
        actionStack->Undo();

        HYP_TEST_CHECK(GetMesh(cube) == originalMesh);
        HYP_TEST_CHECK(csgState->HasBrush());

        HYP_TEST_REQUIRE(actionStack->CanRedo());
        actionStack->Redo();

        HYP_TEST_CHECK(GetMesh(cube) == resultMesh);
        HYP_TEST_CHECK(!csgState->HasBrush());
        HYP_TEST_CHECK(MathUtil::Abs(GetMeshVolume(resultMesh) - (CubeVolume - BrushVolumeInsideCube)) <= VolumeTolerance);
    }

    RemoveNode(cube);
}

HYP_EDITOR_TEST(CsgTools, BrushSelectionDrivesGizmo)
{
    EditorSubsystem* editorSubsystem = GetEditorSubsystem();
    Handle<EditorCsgState> csgState = GetClosedCsgState();
    HYP_TEST_REQUIRE(csgState.IsValid());

    Handle<Entity> cube = AddCube(NAME("CsgGizmoCube"));

    if (EnterWithBoxBrush(context, cube, CsgOperation::Subtract))
    {
        HYP_TEST_CHECK(csgState->IsBrushSelected());

        Array<Handle<Node>> targetNodes = editorSubsystem->GetGizmoTargetNodes();

        HYP_TEST_CHECK(targetNodes.Size() == 1 && targetNodes[0].Get() != cube.Get());
        HYP_TEST_CHECK(editorSubsystem->GetSelectedManipulationMode() != EditorManipulationMode::None);

        // the target is part of the session, so focusing it keeps the tool on, with the gizmo hidden
        editorSubsystem->SetFocusedNode(cube, /* shouldSelectInOutline */ false);

        HYP_TEST_CHECK(csgState->IsEnabled());
        HYP_TEST_CHECK(!csgState->IsBrushSelected());
        HYP_TEST_CHECK(editorSubsystem->GetGizmoTargetNodes().Empty());
        HYP_TEST_CHECK(editorSubsystem->GetSelectedManipulationMode() == EditorManipulationMode::None);

        csgState->SelectBrush();

        HYP_TEST_CHECK(csgState->IsBrushSelected());
        HYP_TEST_CHECK(editorSubsystem->GetGizmoTargetNodes().Size() == 1);
    }

    RemoveNode(cube);
}

HYP_EDITOR_TEST(CsgTools, KeysDriveTool)
{
    Handle<CsgEditorExtension> extension = GetCsgExtension();
    Handle<EditorCsgState> csgState = GetClosedCsgState();
    HYP_TEST_REQUIRE(csgState.IsValid());

    Handle<Entity> cube = AddCube(NAME("CsgKeysCube"));

    if (EnterWithBoxBrush(context, cube, CsgOperation::Subtract))
    {
        HYP_TEST_CHECK(extension->OnViewportKeyDown(MakeKeyEvent(KeyCode::KEY_RETURN)));
        HYP_TEST_CHECK(MathUtil::Abs(GetMeshVolume(GetMesh(cube)) - (CubeVolume - BrushVolumeInsideCube)) <= VolumeTolerance);

        csgState->AddBrush(CsgBrushShape::Box);
        HYP_TEST_REQUIRE(csgState->HasBrush());

        HYP_TEST_CHECK(extension->OnViewportKeyDown(MakeKeyEvent(KeyCode::KEY_ESCAPE)));
        HYP_TEST_CHECK(!csgState->HasBrush());
        HYP_TEST_CHECK(csgState->IsEnabled());

        HYP_TEST_CHECK(extension->OnViewportKeyDown(MakeKeyEvent(KeyCode::KEY_ESCAPE)));
        HYP_TEST_CHECK(!csgState->IsEnabled());

        HYP_TEST_CHECK(!extension->OnViewportKeyDown(MakeKeyEvent(KeyCode::KEY_ESCAPE)));
    }

    RemoveNode(cube);
}

HYP_EDITOR_TEST(CsgTools, OtherToolsExitIt)
{
    EditorSubsystem* editorSubsystem = GetEditorSubsystem();
    Handle<EditorCsgState> csgState = GetClosedCsgState();
    HYP_TEST_REQUIRE(csgState.IsValid());

    Handle<Entity> cube = AddCube(NAME("CsgToolsCube"));
    Handle<Entity> otherCube = AddCube(NAME("CsgOtherCube"));

    if (EnterWithBoxBrush(context, cube, CsgOperation::Subtract))
    {
        editorSubsystem->SetFocusedNode(cube, /* shouldSelectInOutline */ false);
        editorSubsystem->EnterMeshEditMode();

        HYP_TEST_CHECK(editorSubsystem->IsMeshEditModeEnabled());
        HYP_TEST_CHECK(!csgState->IsEnabled());

        editorSubsystem->ExitMeshEditMode(/* saveEdits */ true);
    }

    if (EnterWithBoxBrush(context, cube, CsgOperation::Subtract))
    {
        editorSubsystem->GetDecalPainterState()->SetEnabled(true);

        HYP_TEST_CHECK(editorSubsystem->GetDecalPainterState()->IsEnabled());
        HYP_TEST_CHECK(!csgState->IsEnabled());

        editorSubsystem->GetDecalPainterState()->SetEnabled(false);
    }

    // selecting another node leaves the tool on; the brush decides the targets
    if (EnterWithBoxBrush(context, cube, CsgOperation::Subtract))
    {
        editorSubsystem->SetFocusedNode(otherCube, /* shouldSelectInOutline */ false);

        HYP_TEST_CHECK(csgState->IsEnabled());

        csgState->Exit(/* saveEdits */ true);
    }

    RemoveNode(otherCube);
    RemoveNode(cube);
}

} // namespace csg
} // namespace tests
} // namespace Hyperion

#endif // HYP_TESTS
