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

// a 2x2x2 cube centered on the entity
Handle<Entity> AddCube(Name name)
{
    EditorSubsystem* editorSubsystem = GetEditorSubsystem();

    Handle<Mesh> mesh = MeshBuilder::Cube();

    MaterialAttributes materialAttributes;
    materialAttributes.shaderName = NAME("GeometryPass");

    Handle<Entity> entity = MakeHandle<Entity>();
    entity->SetName(name);

    editorSubsystem->GetActiveScene()->GetRoot()->AddChild(entity);

    MeshComponent meshComponent;
    meshComponent.mesh = mesh;
    meshComponent.material = MakeHandle<Material>(NAME("CsgTestMaterial"), materialAttributes);

    entity->AddComponent<MeshComponent>(meshComponent);
    entity->SetLocalBounds(mesh->GetAABB());

    return entity;
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
    Handle<EditorCsgState> csgState = GetCsgState();
    HYP_TEST_REQUIRE(csgState.IsValid());

    Handle<Entity> cube = AddCube(NAME("CsgEnterCube"));

    editorSubsystem->SetFocusedNode(Handle<Node>::Null(), /* shouldSelectInOutline */ false);
    HYP_TEST_CHECK(!csgState->CanEnter());

    editorSubsystem->SetFocusedNode(cube, /* shouldSelectInOutline */ false);
    HYP_TEST_CHECK(csgState->CanEnter());

    csgState->Enter();

    HYP_TEST_CHECK(csgState->IsEnabled());
    HYP_TEST_CHECK(csgState->GetTargetNode() == cube.Get());
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

HYP_EDITOR_TEST(CsgTools, CannotEnterWithoutMesh)
{
    Handle<EditorCsgState> csgState = GetCsgState();
    HYP_TEST_REQUIRE(csgState.IsValid());

    Handle<Entity> entity = MakeHandle<Entity>();
    entity->SetName(NAME("CsgNoMesh"));
    GetEditorSubsystem()->GetActiveScene()->GetRoot()->AddChild(entity);

    GetEditorSubsystem()->SetFocusedNode(entity, /* shouldSelectInOutline */ false);

    HYP_TEST_CHECK(!csgState->CanEnter());

    csgState->Enter();

    HYP_TEST_CHECK(!csgState->IsEnabled());

    RemoveNode(entity);
}

HYP_EDITOR_TEST(CsgTools, OperationsChangeVolume)
{
    Handle<EditorCsgState> csgState = GetCsgState();
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
            csgState->ApplyBrush();

            const Handle<Mesh> resultMesh = GetMesh(cube);
            const double volume = GetMeshVolume(resultMesh);

            HYP_TEST_CHECK_MSG(csgState->GetStatusText().Empty(), "{}: {}", operationName, csgState->GetStatusText());
            HYP_TEST_CHECK_MSG(resultMesh != originalMesh, "{}: the mesh wasn't replaced", operationName);
            HYP_TEST_CHECK_MSG(MathUtil::Abs(volume - testCase.expectedVolume) <= VolumeTolerance,
                "{}: volume {} expected {}", operationName, volume, testCase.expectedVolume);
            HYP_TEST_CHECK_MSG(csgState->HasPendingEdits(), "{}: no pending edits", operationName);
            HYP_TEST_CHECK_MSG(!csgState->HasBrush(), "{}: the brush stayed after applying", operationName);

            csgState->Exit(/* saveEdits */ true);

            HYP_TEST_CHECK_MSG(GetMesh(cube) == resultMesh, "{}: saving the edits changed the mesh", operationName);
            HYP_TEST_CHECK_MSG(resultMesh->IsRegistered(), "{}: the result mesh isn't a registered asset", operationName);
        }

        RemoveNode(cube);
    }
}

HYP_EDITOR_TEST(CsgTools, KeepBrushAfterApply)
{
    Handle<EditorCsgState> csgState = GetCsgState();
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
    Handle<EditorCsgState> csgState = GetCsgState();
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
    Handle<EditorCsgState> csgState = GetCsgState();
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
    Handle<EditorCsgState> csgState = GetCsgState();
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
    Handle<EditorCsgState> csgState = GetCsgState();
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
    Handle<EditorCsgState> csgState = GetCsgState();
    HYP_TEST_REQUIRE(csgState.IsValid());

    Handle<Entity> cube = AddCube(NAME("CsgToolsCube"));
    Handle<Entity> otherCube = AddCube(NAME("CsgOtherCube"));

    if (EnterWithBoxBrush(context, cube, CsgOperation::Subtract))
    {
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

    if (EnterWithBoxBrush(context, cube, CsgOperation::Subtract))
    {
        editorSubsystem->SetFocusedNode(otherCube, /* shouldSelectInOutline */ false);

        HYP_TEST_CHECK(!csgState->IsEnabled());
    }

    RemoveNode(otherCube);
    RemoveNode(cube);
}

} // namespace csg
} // namespace tests
} // namespace Hyperion

#endif // HYP_TESTS
