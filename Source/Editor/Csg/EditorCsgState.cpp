/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <Editor/Csg/EditorCsgState.hpp>
#include <Editor/Terrain/EditorTerrainState.hpp>
#include <Editor/Decal/EditorDecalPainterState.hpp>
#include <Editor/EditorSubsystem.hpp>
#include <Editor/EditorViewport.hpp>
#include <Editor/EditorProject.hpp>
#include <Editor/EditorAction.hpp>
#include <Editor/EditorActionStack.hpp>

#include <Scene/Scene.hpp>
#include <Scene/World.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/Entity.hpp>

#include <Scene/Camera/Camera.hpp>

#include <Scene/Components/MeshComponent.hpp>
#include <Scene/Components/RigidBodyComponent.hpp>

#include <Physics/PhysicsShape.hpp>

#include <Asset/AssetRegistry.hpp>

#include <Rendering/Mesh.hpp>
#include <Rendering/DebugDrawer.hpp>
#include <Rendering/Util/MeshBuilder.hpp>

#include <Core/Math/MathUtil.hpp>
#include <Core/Math/Quat4f.hpp>
#include <Core/Math/Ray.hpp>

#include <Core/Logging/Logger.hpp>

#include <EditorCsgState.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

namespace {

MeshBooleanOperation ToMeshBooleanOperation(CsgOperation operation)
{
    switch (operation)
    {
    case CsgOperation::Union:
        return MeshBooleanOperation::Union;
    case CsgOperation::Subtract:
        return MeshBooleanOperation::Subtract;
    case CsgOperation::Intersect:
        return MeshBooleanOperation::Intersect;
    }

    return MeshBooleanOperation::Subtract;
}

const char* GetOperationName(CsgOperation operation)
{
    switch (operation)
    {
    case CsgOperation::Union:
        return "Union";
    case CsgOperation::Subtract:
        return "Subtract";
    case CsgOperation::Intersect:
        return "Intersect";
    }

    return "";
}

const char* GetBrushShapeName(CsgBrushShape shape)
{
    switch (shape)
    {
    case CsgBrushShape::Box:
        return "Box";
    case CsgBrushShape::Sphere:
        return "Sphere";
    case CsgBrushShape::Cylinder:
        return "Cylinder";
    default:
        return "";
    }
}

Color GetOperationColor(CsgOperation operation, float alpha)
{
    switch (operation)
    {
    case CsgOperation::Union:
        return Color(0.35f, 0.9f, 0.45f, alpha);
    case CsgOperation::Subtract:
        return Color(1.0f, 0.4f, 0.3f, alpha);
    case CsgOperation::Intersect:
        return Color(0.4f, 0.65f, 1.0f, alpha);
    }

    return Color(1.0f, 1.0f, 1.0f, alpha);
}

RenderableAttributeSet BrushOverlayAttributes(FillMode fillMode)
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

MeshComponent* GetMeshComponent(Node* node)
{
    Entity* entity = DynamicCast<Entity>(node);

    return entity ? entity->TryGetComponent<MeshComponent>() : nullptr;
}

void AssignMesh(const Handle<Node>& node, const Handle<Mesh>& mesh)
{
    Entity* entity = DynamicCast<Entity>(node.Get());
    MeshComponent* meshComponent = entity ? entity->TryGetComponent<MeshComponent>() : nullptr;

    if (!meshComponent || !mesh.IsValid())
    {
        return;
    }

    meshComponent->mesh = mesh;

    entity->SetLocalBounds(mesh->GetAABB());
    entity->AddTag<EntityTag::UpdateRenderProxy>();

    if (Scene* scene = entity->GetScene())
    {
        scene->MarkStaticRenderResourcesChanged();
    }
}

Handle<Mesh> BuildResultMesh(const Mesh* sourceMesh, const MeshBooleanResult& result)
{
    VertexArrayView vertices {};
    vertices.floatData = result.vertexData.Data();
    vertices.vertexCount = result.meshDesc.lods[0].numVertices;
    vertices.layoutDesc = result.meshDesc.meshAttributes.inputLayout;

    MeshDataView meshData {};
    meshData.vertices[0] = vertices;
    meshData.indices[0] = result.indices.ToByteView();

    Handle<Mesh> mesh = MakeHandle<Mesh>();
    mesh->SetName(sourceMesh->GetName());
    mesh->SetMeshData(result.meshDesc, meshData);
    mesh->SetFlags(sourceMesh->GetFlags());

    MeshLodGenerationSettings lodGenerationSettings = sourceMesh->GetLodGenerationSettings();
    lodGenerationSettings.sourceDataHash = 0;
    mesh->SetLodGenerationSettings(lodGenerationSettings);

    BVHNode bvh;
    mesh->BuildBVH(bvh);

    {
        auto writeScope = mesh->GetWriteScope();
        mesh->SetBVH(std::move(bvh));
    }

    mesh->UploadGpuData();

    return mesh;
}

Quat4f RotationFromUpTo(const Vec3f& direction)
{
    const Vec3f up = Vec3f::UnitY();
    const float cosAngle = MathUtil::Clamp(up.Dot(direction), -1.0f, 1.0f);

    Vec3f axis = up.Cross(direction);

    if (axis.Length() < 0.0001f)
    {
        return cosAngle > 0.0f ? Quat4f::Identity() : Quat4f::AxisAngles(Vec3f::UnitX(), MathUtil::pi<float>);
    }

    return Quat4f::AxisAngles(axis.Normalized(), MathUtil::Arccos(cosAngle));
}

Transform MakePlacementTransform(const Vec3f& position, const Vec3f& normal, const Vec3f& halfExtent)
{
    Transform transform;
    transform.SetRotation(RotationFromUpTo(normal).Inverse());
    transform.SetScale(halfExtent);
    transform.SetTranslation(position);

    return transform;
}

bool FindSurfaceHit(EditorSubsystem* subsystem, const Vec2f& relativePos, Vec3f& outPosition, Vec3f& outNormal)
{
    EditorViewport* activeViewport = subsystem->GetActiveViewport();

    if (!activeViewport || !subsystem->GetCurrentProject().IsValid())
    {
        return false;
    }

    const Handle<World>& world = subsystem->GetCurrentProject()->GetWorld();

    if (!world.IsValid())
    {
        return false;
    }

    const Ray ray = activeViewport->GetCamera()->GetPickRay(relativePos);

    bool hasHit = false;
    float closestDistance = MathUtil::MaxSafeValue<float>();

    for (const Handle<Scene>& scene : world->GetScenes())
    {
        if (!scene.IsValid() || !(scene->GetSceneFlags() & SceneFlags::HAS_OCTREE) || (scene->GetSceneFlags() & SceneFlags::EDITOR))
        {
            continue;
        }

        RayTestResults results;

        if (!scene->GetOctree().TestRay(ray, results, RayTestFlags::TestBVH))
        {
            continue;
        }

        for (const RayHit& hit : results)
        {
            if (hit.isApproximate)
            {
                continue;
            }

            if (hit.distance < closestDistance)
            {
                closestDistance = hit.distance;

                outPosition = hit.hitpoint;
                outNormal = hit.normal.LengthSquared() > 0.0001f ? hit.normal.Normalized() : -ray.direction;

                if (outNormal.Dot(ray.direction) > 0.0f)
                {
                    outNormal = -outNormal;
                }

                hasHit = true;
            }

            break;
        }
    }

    return hasHit;
}

static constexpr float HandleScreenRadius = 0.009f;
static constexpr float HandlePickScale = 1.75f;
static constexpr float MinimumBrushSize = 0.01f;
static constexpr uint32 MaxSizeHandles = 6;

struct SizeHandle
{
    Vec3f localDirection;
    uint32 axis = 0;
    bool isRadius = false;
};

uint32 GetSizeHandles(CsgBrushShape shape, FixedArray<SizeHandle, MaxSizeHandles>& outHandles)
{
    uint32 numHandles = 0;

    for (uint32 axis = 0; axis < 3; axis++)
    {
        for (float sign : { 1.0f, -1.0f })
        {
            SizeHandle& handle = outHandles[numHandles++];
            handle.localDirection = Vec3f(0.0f);
            handle.localDirection[int(axis)] = sign;
            handle.axis = axis;

            switch (shape)
            {
            case CsgBrushShape::Sphere:
                handle.isRadius = true;
                break;
            case CsgBrushShape::Cylinder:
                handle.isRadius = axis != 1;
                break;
            default:
                handle.isRadius = false;
                break;
            }
        }
    }

    return numHandles;
}

Vec3f NormalizeHalfExtents(CsgBrushShape shape, const Vec3f& halfExtents)
{
    const Vec3f clamped = Vec3f::Max(halfExtents, Vec3f(MinimumBrushSize * 0.5f));

    switch (shape)
    {
    case CsgBrushShape::Sphere:
        return Vec3f(MathUtil::Max(clamped.x, MathUtil::Max(clamped.y, clamped.z)));
    case CsgBrushShape::Cylinder:
    {
        const float radius = MathUtil::Max(clamped.x, clamped.z);

        return Vec3f(radius, clamped.y, radius);
    }
    default:
        return clamped;
    }
}

Color GetHandleColor(const SizeHandle& handle)
{
    if (handle.isRadius)
    {
        return Color(0.85f, 0.95f, 1.0f, 0.95f);
    }

    switch (handle.axis)
    {
    case 0:
        return Color(0.95f, 0.3f, 0.3f, 0.95f);
    case 1:
        return Color(0.4f, 0.9f, 0.4f, 0.95f);
    default:
        return Color(0.35f, 0.55f, 1.0f, 0.95f);
    }
}

bool RayHitsSphere(const Ray& ray, const Vec3f& center, float radius, float& outDistance)
{
    const Vec3f offset = ray.position - center;

    const float a = ray.direction.Dot(ray.direction);
    const float b = 2.0f * offset.Dot(ray.direction);
    const float c = offset.Dot(offset) - radius * radius;

    const float discriminant = b * b - 4.0f * a * c;

    if (discriminant < 0.0f || a <= MathUtil::epsilonF)
    {
        return false;
    }

    const float root = MathUtil::Sqrt(discriminant);

    float distance = (-b - root) / (2.0f * a);

    if (distance < 0.0f)
    {
        distance = (-b + root) / (2.0f * a);
    }

    if (distance < 0.0f)
    {
        return false;
    }

    outDistance = distance;

    return true;
}

bool RayHitsUnitSphere(const Ray& localRay)
{
    const Vec3f origin = localRay.position;
    const Vec3f direction = localRay.direction;

    const float a = direction.Dot(direction);
    const float b = 2.0f * origin.Dot(direction);
    const float c = origin.Dot(origin) - 1.0f;

    const float discriminant = b * b - 4.0f * a * c;

    if (discriminant < 0.0f || a <= MathUtil::epsilonF)
    {
        return false;
    }

    const float farthestHit = (-b + MathUtil::Sqrt(discriminant)) / (2.0f * a);

    return farthestHit >= 0.0f;
}

} // namespace

#pragma region EditorCsgState

EditorCsgState::EditorCsgState() = default;

EditorCsgState::~EditorCsgState() = default;

void EditorCsgState::Initialize(EditorSubsystem* subsystem)
{
    AssertDebug(subsystem != nullptr);

    m_subsystem = subsystem;
}

bool EditorCsgState::IsEnabled() const
{
    return m_enabled;
}

Node* EditorCsgState::ResolveTarget() const
{
    Handle<Node> node = m_enabled
        ? m_targetNode.Lock()
        : m_subsystem->GetFocusedNode();

    MeshComponent* meshComponent = GetMeshComponent(node.Get());

    if (!meshComponent || !meshComponent->mesh.IsValid())
    {
        return nullptr;
    }

    return node.Get();
}

Handle<Mesh> EditorCsgState::GetTargetMesh() const
{
    MeshComponent* meshComponent = GetMeshComponent(m_targetNode.Lock().Get());

    return meshComponent ? meshComponent->mesh : Handle<Mesh>();
}

bool EditorCsgState::CanEnter() const
{
    AssertOnThread(g_simThread);

    if (m_enabled)
    {
        return true;
    }

    if (!MeshBoolean::IsSupported() || !m_subsystem->GetCurrentProject().IsValid() || m_subsystem->IsSimulating())
    {
        return false;
    }

    Node* target = ResolveTarget();

    if (!target)
    {
        return false;
    }

    const Mesh* mesh = GetMeshComponent(target)->mesh.Get();

    return !mesh->IsDynamicMesh()
        && MeshBoolean::IsLayoutSupported(mesh->GetMeshDesc().meshAttributes.inputLayout);
}

void EditorCsgState::Enter()
{
    AssertOnThread(g_simThread);

    if (m_enabled || !CanEnter())
    {
        return;
    }

    Node* target = ResolveTarget();

    m_subsystem->ExitMeshEditMode(/* saveEdits */ true);
    m_subsystem->GetTerrainState()->SetEnabled(false);
    m_subsystem->GetDecalPainterState()->SetEnabled(false);

    m_targetNode = MakeWeakRef(target);
    m_baselineMesh = GetMeshComponent(target)->mesh;

    m_sessionId++;
    m_brushActions.Clear();
    m_applyActions.Clear();

    if (EditorActionStack* actionStack = GetProjectActionStack())
    {
        m_actionAddedHandler = actionStack->OnActionAdded.Bind([this](EditorActionBase* action)
            {
                if (m_enabled && action != nullptr && IsBrushSelected() && !IsSessionAction(action))
                {
                    m_brushActions.PushBack(MakeStrongRef(action));
                }
            });
    }

    m_lastApplyError = MeshBooleanError::None;
    m_manipulationModeBeforeCsg = m_subsystem->GetSelectedManipulationMode();

    m_enabled = true;
    m_brushManipulationMode = EditorManipulationMode::Translate;

    m_subsystem->SetSelectedManipulationMode(EditorManipulationMode::None);

    ValidateTarget();

    BeginPlacement(m_brushShape);

    OnStateChanged();
}

void EditorCsgState::Exit(bool saveEdits)
{
    AssertOnThread(g_simThread);

    if (!m_enabled)
    {
        return;
    }

    m_placement = PlacementState {};
    m_handleDrag = HandleDragState {};
    m_hoveredHandle = -1;

    if (Handle<Node> brushNode = m_brushNode; brushNode.IsValid())
    {
        DetachBrush(brushNode);
    }

    m_actionAddedHandler.Reset();

    if (saveEdits)
    {
        RemoveSessionActions(/* includeApplyActions */ false);

        FinalizeMesh();
    }
    else
    {
        RemoveSessionActions(/* includeApplyActions */ true);

        Handle<Node> target = m_targetNode.Lock();

        if (target.IsValid() && m_baselineMesh.IsValid() && GetTargetMesh() != m_baselineMesh)
        {
            AssignMesh(target, m_baselineMesh);
            m_subsystem->SyncBoxPhysicsShapeToLocalBounds(DynamicCast<Entity>(target.Get()));
        }
    }

    m_enabled = false;

    m_targetNode.Reset();
    m_baselineMesh.Reset();
    m_brushNode.Reset();
    m_validatedMesh.Reset();

    m_brushActions.Clear();
    m_applyActions.Clear();

    m_targetError = MeshBooleanError::None;
    m_lastApplyError = MeshBooleanError::None;

    m_subsystem->SetSelectedManipulationMode(m_manipulationModeBeforeCsg);

    OnStateChanged();
}

Node* EditorCsgState::GetTargetNode() const
{
    if (!m_enabled)
    {
        return nullptr;
    }

    return m_targetNode.Lock().Get();
}

bool EditorCsgState::HasBrush() const
{
    return m_brushNode.IsValid()
        && m_brushNode->GetScene() != nullptr
        && m_brushNode->GetScene() == m_subsystem->GetEditorScene().Get();
}

bool EditorCsgState::HasPendingEdits() const
{
    return m_enabled && GetTargetMesh() != m_baselineMesh;
}

bool EditorCsgState::CanApply() const
{
    return m_enabled
        && !m_placement.active
        && HasBrush()
        && m_targetError == MeshBooleanError::None
        && m_targetNode.Lock().IsValid();
}

String EditorCsgState::GetStatusText() const
{
    if (!m_enabled)
    {
        return String::empty;
    }

    if (m_targetError != MeshBooleanError::None)
    {
        return MeshBoolean::GetErrorMessage(m_targetError);
    }

    if (m_lastApplyError != MeshBooleanError::None)
    {
        return MeshBoolean::GetErrorMessage(m_lastApplyError);
    }

    return String::empty;
}

CsgBrushShape EditorCsgState::GetBrushShape() const
{
    return m_brushShape;
}

CsgOperation EditorCsgState::GetOperation() const
{
    return m_operation;
}

void EditorCsgState::SetOperation(CsgOperation operation)
{
    AssertOnThread(g_simThread);

    if (operation == m_operation)
    {
        return;
    }

    m_operation = operation;
    m_lastApplyError = MeshBooleanError::None;

    OnStateChanged();
}

bool EditorCsgState::IsSessionActive(uint32 sessionId) const
{
    return m_enabled && m_sessionId == sessionId;
}

bool EditorCsgState::IsSessionNode(const Node* node) const
{
    if (!m_enabled || node == nullptr)
    {
        return false;
    }

    return node == m_brushNode.Get() || node == m_targetNode.Lock().Get();
}

const Handle<Mesh>& EditorCsgState::GetBrushMesh(CsgBrushShape shape)
{
    Handle<Mesh>& brushMesh = m_brushMeshes[uint32(shape)];

    if (brushMesh.IsValid())
    {
        return brushMesh;
    }

    switch (shape)
    {
    case CsgBrushShape::Box:
        brushMesh = MeshBuilder::Cube();
        break;
    case CsgBrushShape::Sphere:
        brushMesh = MeshBuilder::NormalizedCubeSphere(12);
        break;
    case CsgBrushShape::Cylinder:
        brushMesh = MeshBuilder::Cylinder(1.0f, 2.0f, 32);
        break;
    default:
        break;
    }

    return brushMesh;
}

void EditorCsgState::ValidateTarget()
{
    Handle<Mesh> mesh = GetTargetMesh();

    if (m_validatedMesh == mesh)
    {
        return;
    }

    m_validatedMesh = mesh.ToWeak();
    m_targetError = mesh.IsValid() ? MeshBoolean::ValidateSolid(mesh.Get()) : MeshBooleanError::MissingData;

    if (m_targetError != MeshBooleanError::None && mesh.IsValid())
    {
        HYP_LOG(Editor, Warning, "CSG: {} can't be used as a target - {}", mesh->GetName(), MeshBoolean::GetErrorMessage(m_targetError));
    }
}

Vec3f EditorCsgState::GetBrushHalfExtent() const
{
    if (HasBrush())
    {
        return m_brushNode->GetLocalTransform().GetScale();
    }

    if (m_lastBrushHalfExtent.HasValue())
    {
        return *m_lastBrushHalfExtent;
    }

    return Vec3f(MathUtil::Max(m_subsystem->GetGridSize(), 0.01f) * 0.5f);
}

Transform EditorCsgState::MakeDefaultBrushTransform(Node* target) const
{
    const Vec3f halfExtent = GetBrushHalfExtent();

    const BoundingBox targetBounds = target->GetWorldBounds();

    if (!targetBounds.IsValid() || !targetBounds.IsFinite())
    {
        return Transform(target->GetWorldTranslation(), halfExtent, Quat4f::Identity());
    }

    return Transform(targetBounds.GetCenter() + Vec3f(0.0f, targetBounds.GetExtent().y * 0.5f, 0.0f), halfExtent, Quat4f::Identity());
}

void EditorCsgState::AttachBrush(const Handle<Node>& brushNode, CsgBrushShape shape)
{
    if (!brushNode.IsValid())
    {
        return;
    }

    if (m_brushNode.IsValid() && m_brushNode != brushNode)
    {
        DetachBrush(m_brushNode);
    }

    m_placement = PlacementState {};

    m_brushNode = brushNode;
    m_brushShape = shape;
    m_lastApplyError = MeshBooleanError::None;

    if (brushNode->GetScene() != m_subsystem->GetEditorScene().Get())
    {
        m_subsystem->GetEditorScene()->GetRoot()->AddChild(brushNode);
    }

    SelectBrush();

    OnStateChanged();
}

void EditorCsgState::DetachBrush(const Handle<Node>& brushNode)
{
    if (!brushNode.IsValid())
    {
        return;
    }

    const bool wasFocused = m_subsystem->GetFocusedNode() == brushNode;

    if (brushNode->GetParent() != nullptr)
    {
        brushNode->Remove(/* moveToDetached */ false);
    }

    if (m_brushNode == brushNode)
    {
        m_lastBrushHalfExtent = brushNode->GetLocalTransform().GetScale();

        m_brushNode.Reset();

        m_handleDrag = HandleDragState {};
        m_hoveredHandle = -1;
    }

    if (wasFocused)
    {
        if (Handle<Node> target = m_targetNode.Lock(); target.IsValid())
        {
            m_subsystem->SetFocusedNode(target, /* shouldSelectInOutline */ true);
        }
    }

    OnStateChanged();
}

void EditorCsgState::PlaceBrush(CsgBrushShape shape, const Transform& transform)
{
    Handle<Node> target = m_targetNode.Lock();

    if (!m_enabled || !target.IsValid() || uint32(shape) >= NumCsgBrushShapes)
    {
        return;
    }

    if (HasBrush())
    {
        Handle<Node> brushNode = m_brushNode;

        const Transform previousTransform = brushNode->GetLocalTransform();
        const CsgBrushShape previousShape = m_brushShape;

        PushBrushAction(MakeHandle<FunctionalEditorAction>(
            HYP_FORMAT("Place CSG {} Brush", GetBrushShapeName(shape)),
            [brushNode, shape, transform, previousShape, previousTransform]() -> EditorActionFunctions
            {
                return {
                    [brushNode, shape, transform](EditorSubsystem* editorSubsystem, EditorProject* editorProject)
                    {
                        brushNode->SetLocalTransform(transform);

                        editorSubsystem->GetCsgState()->AttachBrush(brushNode, shape);
                    },
                    [brushNode, previousShape, previousTransform](EditorSubsystem* editorSubsystem, EditorProject* editorProject)
                    {
                        brushNode->SetLocalTransform(previousTransform);

                        editorSubsystem->GetCsgState()->AttachBrush(brushNode, previousShape);
                    }
                };
            }));

        return;
    }

    Handle<Node> brushNode = MakeHandle<Node>();
    brushNode->SetName(NAME("CSG Brush"));
    brushNode->SetNodeFlags(brushNode->GetNodeFlags() | NodeFlags::HideInSceneOutline);
    brushNode->SetLocalTransform(transform);

    PushBrushAction(MakeHandle<FunctionalEditorAction>(
        HYP_FORMAT("Add CSG {} Brush", GetBrushShapeName(shape)),
        [brushNode, shape]() -> EditorActionFunctions
        {
            return {
                [brushNode, shape](EditorSubsystem* editorSubsystem, EditorProject* editorProject)
                {
                    editorSubsystem->GetCsgState()->AttachBrush(brushNode, shape);
                },
                [brushNode](EditorSubsystem* editorSubsystem, EditorProject* editorProject)
                {
                    editorSubsystem->GetCsgState()->DetachBrush(brushNode);
                }
            };
        }));
}

void EditorCsgState::AddBrush(CsgBrushShape shape)
{
    AssertOnThread(g_simThread);

    Handle<Node> target = m_targetNode.Lock();

    if (!m_enabled || !target.IsValid())
    {
        return;
    }

    PlaceBrush(shape, MakeDefaultBrushTransform(target.Get()));
}

void EditorCsgState::SelectBrushShape(CsgBrushShape shape)
{
    AssertOnThread(g_simThread);

    BeginPlacement(shape);
}

void EditorCsgState::BeginPlacement(CsgBrushShape shape)
{
    Handle<Node> target = m_targetNode.Lock();

    if (!m_enabled || !target.IsValid() || uint32(shape) >= NumCsgBrushShapes)
    {
        return;
    }

    m_placement = PlacementState {};
    m_placement.active = true;
    m_placement.shape = shape;
    m_placement.halfExtent = NormalizeHalfExtents(shape, GetBrushHalfExtent());

    m_lastApplyError = MeshBooleanError::None;

    m_subsystem->SetFocusedNode(target, /* shouldSelectInOutline */ true);
    m_subsystem->SetSelectedManipulationMode(EditorManipulationMode::None);

    OnStateChanged();
}

bool EditorCsgState::IsPlacing() const
{
    return m_enabled && m_placement.active;
}

CsgBrushShape EditorCsgState::GetPlacementShape() const
{
    return m_placement.shape;
}

void EditorCsgState::CancelPlacement()
{
    AssertOnThread(g_simThread);

    if (!IsPlacing())
    {
        return;
    }

    m_placement = PlacementState {};

    if (HasBrush())
    {
        SelectBrush();
    }

    OnStateChanged();
}

void EditorCsgState::UpdatePlacementHover(const Vec2f& relativePos)
{
    if (!IsPlacing())
    {
        return;
    }

    m_placement.hasHover = FindSurfaceHit(m_subsystem, relativePos, m_placement.hoverPosition, m_placement.hoverNormal);
}

bool EditorCsgState::CommitPlacement()
{
    if (!IsPlacing())
    {
        return false;
    }

    if (!m_placement.hasHover)
    {
        return true;
    }

    const CsgBrushShape shape = m_placement.shape;
    const Transform transform = MakePlacementTransform(m_placement.hoverPosition, m_placement.hoverNormal, m_placement.halfExtent);

    m_lastBrushHalfExtent = m_placement.halfExtent;

    m_placement = PlacementState {};

    PlaceBrush(shape, transform);

    return true;
}

void EditorCsgState::ScalePlacement(float factor)
{
    if (!IsPlacing())
    {
        return;
    }

    m_placement.halfExtent = Vec3f::Max(m_placement.halfExtent * factor, Vec3f(0.01f));

    OnStateChanged();
}

bool EditorCsgState::IsBrushSelected() const
{
    return HasBrush() && m_subsystem->GetFocusedNode() == m_brushNode;
}

void EditorCsgState::SelectBrush()
{
    AssertOnThread(g_simThread);

    if (!m_enabled || !HasBrush())
    {
        return;
    }

    m_subsystem->SetFocusedNode(m_brushNode, /* shouldSelectInOutline */ false);

    OnStateChanged();
}

EditorManipulationMode EditorCsgState::ResolveManipulationMode(EditorManipulationMode requestedMode)
{
    if (!m_enabled || requestedMode == EditorManipulationMode::None)
    {
        return requestedMode;
    }

    if (m_placement.active || !HasBrush())
    {
        return EditorManipulationMode::None;
    }

    if (requestedMode == EditorManipulationMode::Scale)
    {
        requestedMode = m_brushManipulationMode;
    }

    m_brushManipulationMode = requestedMode;

    if (!IsBrushSelected())
    {
        SelectBrush();
    }

    return requestedMode;
}

void EditorCsgState::ResetBrush()
{
    AssertOnThread(g_simThread);

    Handle<Node> target = m_targetNode.Lock();

    if (!m_enabled || !HasBrush() || !target.IsValid())
    {
        return;
    }

    SetBrushTransformUndoable(m_brushNode->GetLocalTransform(), MakeDefaultBrushTransform(target.Get()), "Reset CSG Brush");

    SelectBrush();
}

void EditorCsgState::SetBrushTransformUndoable(const Transform& previousTransform, const Transform& newTransform, const char* actionText)
{
    if (!HasBrush())
    {
        return;
    }

    Handle<Node> brushNode = m_brushNode;

    PushBrushAction(MakeHandle<FunctionalEditorAction>(
        actionText,
        [brushNode, previousTransform, newTransform]() -> EditorActionFunctions
        {
            return {
                [brushNode, newTransform](EditorSubsystem* editorSubsystem, EditorProject* editorProject)
                {
                    brushNode->SetLocalTransform(newTransform);

                    editorSubsystem->GetCsgState()->OnStateChanged();
                },
                [brushNode, previousTransform](EditorSubsystem* editorSubsystem, EditorProject* editorProject)
                {
                    brushNode->SetLocalTransform(previousTransform);

                    editorSubsystem->GetCsgState()->OnStateChanged();
                }
            };
        }));
}

CsgBrushShape EditorCsgState::GetSizingShape() const
{
    return m_placement.active ? m_placement.shape : m_brushShape;
}

Vec3f EditorCsgState::GetSizingHalfExtents() const
{
    if (m_placement.active)
    {
        return m_placement.halfExtent;
    }

    return NormalizeHalfExtents(m_brushShape, GetBrushHalfExtent());
}

void EditorCsgState::SetSizingHalfExtents(Vec3f halfExtents)
{
    AssertOnThread(g_simThread);

    if (!m_enabled)
    {
        return;
    }

    const Vec3f normalizedHalfExtents = NormalizeHalfExtents(GetSizingShape(), halfExtents);

    if (m_placement.active)
    {
        m_placement.halfExtent = normalizedHalfExtents;
        m_lastBrushHalfExtent = normalizedHalfExtents;

        OnStateChanged();

        return;
    }

    if (!HasBrush())
    {
        return;
    }

    const Transform previousTransform = m_brushNode->GetLocalTransform();

    if (previousTransform.GetScale() == normalizedHalfExtents)
    {
        return;
    }

    m_lastBrushHalfExtent = normalizedHalfExtents;

    SetBrushTransformUndoable(
        previousTransform,
        Transform(previousTransform.GetTranslation(), normalizedHalfExtents, previousTransform.GetRotation()),
        "Resize CSG Brush");
}

bool EditorCsgState::CanUseSizeHandles() const
{
    return m_enabled && !m_placement.active && HasBrush();
}

float EditorCsgState::GetHandleRadius(const Vec3f& worldPosition) const
{
    EditorViewport* activeViewport = m_subsystem->GetActiveViewport();

    if (!activeViewport || !activeViewport->GetCamera().IsValid())
    {
        return 0.05f;
    }

    const Handle<Camera>& camera = activeViewport->GetCamera();
    const Mat4f& projection = camera->GetProjectionMatrix();

    if (projection[1][1] <= MathUtil::epsilonF)
    {
        return 0.05f;
    }

    float viewDepth = 1.0f;

    if (projection[3][3] == 0.0f)
    {
        viewDepth = MathUtil::Max((worldPosition - camera->GetWorldTranslation()).Dot(camera->GetDirection()), camera->GetNearClip());
    }

    return (2.0f * viewDepth / projection[1][1]) * HandleScreenRadius;
}

int32 EditorCsgState::PickSizeHandle(const Vec2f& relativePos) const
{
    EditorViewport* activeViewport = m_subsystem->GetActiveViewport();

    if (!CanUseSizeHandles() || !activeViewport || !activeViewport->GetCamera().IsValid())
    {
        return -1;
    }

    const Ray ray = activeViewport->GetCamera()->GetPickRay(relativePos);

    FixedArray<SizeHandle, MaxSizeHandles> handles;
    const uint32 numHandles = GetSizeHandles(m_brushShape, handles);

    const Mat4f& worldMatrix = m_brushNode->GetWorldMatrix();

    int32 closestHandle = -1;
    float closestDistance = MathUtil::MaxSafeValue<float>();

    for (uint32 handleIndex = 0; handleIndex < numHandles; handleIndex++)
    {
        const Vec3f handlePosition = worldMatrix.TransformVector(Vec4f(handles[handleIndex].localDirection, 1.0f)).GetXYZ();

        float distance = 0.0f;

        if (RayHitsSphere(ray, handlePosition, GetHandleRadius(handlePosition) * HandlePickScale, distance) && distance < closestDistance)
        {
            closestDistance = distance;
            closestHandle = int32(handleIndex);
        }
    }

    return closestHandle;
}

void EditorCsgState::UpdateHandleHover(const Vec2f& relativePos)
{
    if (m_handleDrag.active)
    {
        return;
    }

    m_hoveredHandle = PickSizeHandle(relativePos);
}

bool EditorCsgState::IsHandleHovered() const
{
    return m_hoveredHandle >= 0 && CanUseSizeHandles();
}

bool EditorCsgState::IsHandleDragActive() const
{
    return m_handleDrag.active;
}

bool EditorCsgState::BeginHandleDrag(const Vec2f& relativePos)
{
    const int32 handleIndex = PickSizeHandle(relativePos);

    if (handleIndex < 0)
    {
        return false;
    }

    const Handle<Camera>& camera = m_subsystem->GetActiveViewport()->GetCamera();

    FixedArray<SizeHandle, MaxSizeHandles> handles;
    GetSizeHandles(m_brushShape, handles);

    const SizeHandle& handle = handles[handleIndex];
    const Mat4f& worldMatrix = m_brushNode->GetWorldMatrix();

    const Vec3f handlePosition = worldMatrix.TransformVector(Vec4f(handle.localDirection, 1.0f)).GetXYZ();
    Vec3f worldAxis = worldMatrix.TransformVector(Vec4f(handle.localDirection, 0.0f)).GetXYZ();

    if (worldAxis.LengthSquared() <= MathUtil::epsilonF)
    {
        return false;
    }

    worldAxis = worldAxis.Normalized();

    const Vec3f toCamera = (camera->GetWorldTranslation() - handlePosition).Normalized();

    Vec3f planeNormal = toCamera - worldAxis * toCamera.Dot(worldAxis);

    if (planeNormal.LengthSquared() < 0.000001f)
    {
        planeNormal = worldAxis.Cross(camera->GetUpVector());
    }

    planeNormal = planeNormal.Normalized();

    const Optional<RayHit> planeHit = camera->GetPickRay(relativePos).TestPlane(handlePosition, planeNormal);

    if (!planeHit.HasValue())
    {
        return false;
    }

    m_handleDrag = HandleDragState {};
    m_handleDrag.active = true;
    m_handleDrag.handleIndex = handleIndex;
    m_handleDrag.startTransform = m_brushNode->GetLocalTransform();
    m_handleDrag.worldAxis = worldAxis;
    m_handleDrag.planePoint = handlePosition;
    m_handleDrag.planeNormal = planeNormal;
    m_handleDrag.startProjection = (planeHit->hitpoint - handlePosition).Dot(worldAxis);

    m_hoveredHandle = handleIndex;

    if (!IsBrushSelected())
    {
        SelectBrush();
    }

    return true;
}

void EditorCsgState::UpdateHandleDrag(const Vec2f& relativePos)
{
    EditorViewport* activeViewport = m_subsystem->GetActiveViewport();

    if (!m_handleDrag.active || !HasBrush() || !activeViewport)
    {
        return;
    }

    const Optional<RayHit> planeHit = activeViewport->GetCamera()->GetPickRay(relativePos).TestPlane(m_handleDrag.planePoint, m_handleDrag.planeNormal);

    if (!planeHit.HasValue())
    {
        return;
    }

    const float delta = (planeHit->hitpoint - m_handleDrag.planePoint).Dot(m_handleDrag.worldAxis) - m_handleDrag.startProjection;

    FixedArray<SizeHandle, MaxSizeHandles> handles;
    GetSizeHandles(m_brushShape, handles);

    const SizeHandle& handle = handles[m_handleDrag.handleIndex];

    const Vec3f startHalfExtent = m_handleDrag.startTransform.GetScale();

    const bool snapToGrid = m_subsystem->IsSnapToGridEnabled();
    const float gridSize = MathUtil::Max(m_subsystem->GetGridSize(), MinimumBrushSize);

    auto snapSize = [snapToGrid, gridSize](float size) -> float
    {
        size = MathUtil::Max(size, MinimumBrushSize);

        if (snapToGrid)
        {
            size = MathUtil::Max(float(MathUtil::Round(size / gridSize)) * gridSize, gridSize);
        }

        return size;
    };

    Vec3f newHalfExtent = startHalfExtent;
    Vec3f newTranslation = m_handleDrag.startTransform.GetTranslation();

    if (handle.isRadius)
    {
        const float radius = snapSize((startHalfExtent[int(handle.axis)] + delta) * 2.0f) * 0.5f;

        newHalfExtent = m_brushShape == CsgBrushShape::Sphere
            ? Vec3f(radius)
            : Vec3f(radius, startHalfExtent.y, radius);
    }
    else
    {
        const float startSize = startHalfExtent[int(handle.axis)] * 2.0f;
        const float newSize = snapSize(startSize + delta);

        newHalfExtent[int(handle.axis)] = newSize * 0.5f;
        newTranslation = newTranslation + m_handleDrag.worldAxis * ((newSize - startSize) * 0.5f);
    }

    m_brushNode->SetLocalTransform(Transform(newTranslation, newHalfExtent, m_handleDrag.startTransform.GetRotation()));
}

void EditorCsgState::EndHandleDrag()
{
    if (!m_handleDrag.active)
    {
        return;
    }

    const Transform startTransform = m_handleDrag.startTransform;

    m_handleDrag = HandleDragState {};

    if (!HasBrush())
    {
        return;
    }

    const Transform endTransform = m_brushNode->GetLocalTransform();

    if (endTransform.GetScale() == startTransform.GetScale() && endTransform.GetTranslation() == startTransform.GetTranslation())
    {
        return;
    }

    m_lastBrushHalfExtent = endTransform.GetScale();

    SetBrushTransformUndoable(startTransform, endTransform, "Resize CSG Brush");
}

void EditorCsgState::DrawSizeHandles(DebugDrawCommandList& debugDrawCommandList) const
{
    if (!CanUseSizeHandles())
    {
        return;
    }

    static const RenderableAttributeSet handleAttributes = BrushOverlayAttributes(FillMode::Fill);

    FixedArray<SizeHandle, MaxSizeHandles> handles;
    const uint32 numHandles = GetSizeHandles(m_brushShape, handles);

    const Mat4f& worldMatrix = m_brushNode->GetWorldMatrix();

    const int32 highlightedHandle = m_handleDrag.active ? m_handleDrag.handleIndex : m_hoveredHandle;

    for (uint32 handleIndex = 0; handleIndex < numHandles; handleIndex++)
    {
        const SizeHandle& handle = handles[handleIndex];
        const Vec3f handlePosition = worldMatrix.TransformVector(Vec4f(handle.localDirection, 1.0f)).GetXYZ();

        Color color = GetHandleColor(handle);

        if (int32(handleIndex) == highlightedHandle)
        {
            color = Color(1.0f, 0.85f, 0.2f, 1.0f);
        }

        debugDrawCommandList.sphere(handlePosition, GetHandleRadius(handlePosition), color, handleAttributes);
    }
}

bool EditorCsgState::IsKeepBrushAfterApply() const
{
    return m_keepBrushAfterApply;
}

void EditorCsgState::SetKeepBrushAfterApply(bool keepBrushAfterApply)
{
    AssertOnThread(g_simThread);

    if (keepBrushAfterApply == m_keepBrushAfterApply)
    {
        return;
    }

    m_keepBrushAfterApply = keepBrushAfterApply;

    OnStateChanged();
}

void EditorCsgState::CancelBrush()
{
    AssertOnThread(g_simThread);

    if (!m_enabled || !HasBrush())
    {
        return;
    }

    Handle<Node> brushNode = m_brushNode;
    const CsgBrushShape shape = m_brushShape;

    PushBrushAction(MakeHandle<FunctionalEditorAction>(
        "Remove CSG Brush",
        [brushNode, shape]() -> EditorActionFunctions
        {
            return {
                [brushNode](EditorSubsystem* editorSubsystem, EditorProject* editorProject)
                {
                    editorSubsystem->GetCsgState()->DetachBrush(brushNode);
                },
                [brushNode, shape](EditorSubsystem* editorSubsystem, EditorProject* editorProject)
                {
                    editorSubsystem->GetCsgState()->AttachBrush(brushNode, shape);
                }
            };
        }));
}

void EditorCsgState::ApplyBrush()
{
    AssertOnThread(g_simThread);

    if (!CanApply())
    {
        return;
    }

    Handle<Node> target = m_targetNode.Lock();
    Handle<Mesh> previousMesh = GetTargetMesh();
    Handle<Node> brushNode = m_brushNode;
    const CsgBrushShape shape = m_brushShape;
    const CsgOperation operation = m_operation;
    const bool keepBrush = m_keepBrushAfterApply;

    if (!previousMesh.IsValid())
    {
        return;
    }

    const MeshBooleanOperand targetOperand { previousMesh.Get(), Mat4f::Identity() };
    const MeshBooleanOperand brushOperand { GetBrushMesh(shape).Get(), target->GetWorldMatrix().Inverse() * brushNode->GetWorldMatrix() };

    const MeshBooleanResult result = MeshBoolean::Apply(targetOperand, brushOperand, ToMeshBooleanOperation(operation));

    if (result.error != MeshBooleanError::None)
    {
        HYP_LOG(Editor, Warning, "CSG {} on {} failed: {}", GetOperationName(operation), target->GetName(), MeshBoolean::GetErrorMessage(result.error));

        m_lastApplyError = result.error;

        OnStateChanged();

        return;
    }

    Handle<Mesh> resultMesh = BuildResultMesh(previousMesh.Get(), result);

    m_validatedMesh = resultMesh.ToWeak();
    m_targetError = MeshBooleanError::None;
    m_lastApplyError = MeshBooleanError::None;

    PushApplyAction(MakeHandle<FunctionalEditorAction>(
        HYP_FORMAT("CSG {} ({})", GetOperationName(operation), GetBrushShapeName(shape)),
        [targetWeak = target.ToWeak(), previousMesh, resultMesh, brushNode, shape, keepBrush, sessionId = m_sessionId]() -> EditorActionFunctions
        {
            return {
                [targetWeak, resultMesh, brushNode, keepBrush, sessionId](EditorSubsystem* editorSubsystem, EditorProject* editorProject)
                {
                    Handle<Node> target = targetWeak.Lock();

                    if (!target.IsValid())
                    {
                        return;
                    }

                    AssignMesh(target, resultMesh);
                    editorSubsystem->SyncBoxPhysicsShapeToLocalBounds(DynamicCast<Entity>(target.Get()));

                    const Handle<EditorCsgState>& csgState = editorSubsystem->GetCsgState();

                    if (!csgState->IsSessionActive(sessionId))
                    {
                        return;
                    }

                    if (keepBrush)
                    {
                        csgState->OnStateChanged();
                    }
                    else
                    {
                        csgState->DetachBrush(brushNode);
                    }
                },
                [targetWeak, previousMesh, brushNode, shape, sessionId](EditorSubsystem* editorSubsystem, EditorProject* editorProject)
                {
                    Handle<Node> target = targetWeak.Lock();

                    if (!target.IsValid())
                    {
                        return;
                    }

                    AssignMesh(target, previousMesh);
                    editorSubsystem->SyncBoxPhysicsShapeToLocalBounds(DynamicCast<Entity>(target.Get()));

                    const Handle<EditorCsgState>& csgState = editorSubsystem->GetCsgState();

                    if (csgState->IsSessionActive(sessionId))
                    {
                        csgState->AttachBrush(brushNode, shape);
                    }
                }
            };
        }));
}

EditorActionStack* EditorCsgState::GetProjectActionStack() const
{
    const Handle<EditorProject>& project = m_subsystem->GetCurrentProject();

    return project.IsValid() ? project->GetActionStack().Get() : nullptr;
}

void EditorCsgState::PushBrushAction(const Handle<EditorActionBase>& action)
{
    EditorActionStack* actionStack = GetProjectActionStack();

    if (!actionStack)
    {
        return;
    }

    m_brushActions.PushBack(action);

    actionStack->PushAction(action);
}

void EditorCsgState::PushApplyAction(const Handle<EditorActionBase>& action)
{
    EditorActionStack* actionStack = GetProjectActionStack();

    if (!actionStack)
    {
        return;
    }

    m_applyActions.PushBack(action);

    actionStack->PushAction(action);
}

bool EditorCsgState::IsSessionAction(const EditorActionBase* action) const
{
    for (const Handle<EditorActionBase>& brushAction : m_brushActions)
    {
        if (brushAction.Get() == action)
        {
            return true;
        }
    }

    for (const Handle<EditorActionBase>& applyAction : m_applyActions)
    {
        if (applyAction.Get() == action)
        {
            return true;
        }
    }

    return false;
}

void EditorCsgState::RemoveSessionActions(bool includeApplyActions)
{
    EditorActionStack* actionStack = GetProjectActionStack();

    if (!actionStack)
    {
        return;
    }

    actionStack->RemoveActions([this, includeApplyActions](EditorActionBase* action)
        {
            for (const Handle<EditorActionBase>& brushAction : m_brushActions)
            {
                if (brushAction.Get() == action)
                {
                    return true;
                }
            }

            if (!includeApplyActions)
            {
                return false;
            }

            for (const Handle<EditorActionBase>& applyAction : m_applyActions)
            {
                if (applyAction.Get() == action)
                {
                    return true;
                }
            }

            return false;
        });
}

void EditorCsgState::FinalizeMesh()
{
    Handle<Node> target = m_targetNode.Lock();
    Handle<Mesh> finalMesh = GetTargetMesh();

    if (!target.IsValid() || !finalMesh.IsValid() || !m_baselineMesh.IsValid() || finalMesh == m_baselineMesh)
    {
        return;
    }

    if (m_baselineMesh->GetMeshDesc().GetNumLods() > 1 && finalMesh->CanGenerateLods())
    {
        finalMesh->GenerateLods();
    }

    if (!finalMesh->IsRegistered())
    {
        GetCurrentAssetRegistry()->PutAssetUnique(finalMesh);
    }

    if (Entity* entity = DynamicCast<Entity>(target.Get()))
    {
        RigidBodyComponent* rigidBodyComponent = entity->TryGetComponent<RigidBodyComponent>();

        if (rigidBodyComponent && rigidBodyComponent->shape.IsValid() && rigidBodyComponent->shape->GetType() == PhysicsShapeType::Compound)
        {
            m_subsystem->GenerateConvexCollision(target.Get());
        }
    }
}

void EditorCsgState::OnFocusedNodeChanged(const Handle<Node>& focusedNode)
{
    if (!m_enabled)
    {
        return;
    }

    if (focusedNode.IsValid() && !IsSessionNode(focusedNode.Get()))
    {
        Exit(/* saveEdits */ true);

        return;
    }

    if (HasBrush() && focusedNode == m_brushNode && !m_placement.active)
    {
        m_subsystem->SetSelectedManipulationMode(m_brushManipulationMode);
    }
    else
    {
        const EditorManipulationMode currentMode = m_subsystem->GetSelectedManipulationMode();

        if (currentMode != EditorManipulationMode::None)
        {
            m_brushManipulationMode = currentMode;
        }

        m_subsystem->SetSelectedManipulationMode(EditorManipulationMode::None);
    }

    OnStateChanged();
}

bool EditorCsgState::TryPickBrush(const Ray& ray)
{
    if (!m_enabled || m_placement.active || !HasBrush())
    {
        return false;
    }

    const Ray localRay = m_brushNode->GetWorldMatrix().Inverse() * ray;

    const bool hit = m_brushShape == CsgBrushShape::Sphere
        ? RayHitsUnitSphere(localRay)
        : localRay.TestAABB(BoundingBox(Vec3f(-1.0f), Vec3f(1.0f))).HasValue();

    if (!hit)
    {
        return false;
    }

    SelectBrush();

    return true;
}

bool EditorCsgState::BackOut()
{
    if (!m_enabled)
    {
        return false;
    }

    if (m_placement.active)
    {
        CancelPlacement();

        return true;
    }

    if (HasBrush())
    {
        CancelBrush();

        return true;
    }

    Exit(/* saveEdits */ true);

    return true;
}

void EditorCsgState::Update()
{
    if (!m_enabled)
    {
        return;
    }

    if (!m_targetNode.Lock().IsValid())
    {
        Exit(/* saveEdits */ false);

        return;
    }

    ValidateTarget();
}

void EditorCsgState::DrawBrush(DebugDrawCommandList& debugDrawCommandList, CsgBrushShape shape, const Transform& transform, float fillAlpha, float outlineAlpha) const
{
    static const RenderableAttributeSet fillAttributes = BrushOverlayAttributes(FillMode::Fill);
    static const RenderableAttributeSet outlineAttributes = BrushOverlayAttributes(FillMode::Line);

    const Color fillColor = GetOperationColor(m_operation, fillAlpha);
    const Color outlineColor = GetOperationColor(m_operation, outlineAlpha);

    switch (shape)
    {
    case CsgBrushShape::Box:
        debugDrawCommandList.box(transform, fillColor, fillAttributes);
        debugDrawCommandList.box(transform, outlineColor, outlineAttributes);
        break;
    case CsgBrushShape::Sphere:
        debugDrawCommandList.sphere(transform, fillColor, fillAttributes);
        debugDrawCommandList.sphere(transform, outlineColor, outlineAttributes);
        break;
    case CsgBrushShape::Cylinder:
    {
        const Transform cylinderTransform(
            transform.GetTranslation(),
            transform.GetScale() * Vec3f(1.0f, 2.0f, 1.0f),
            transform.GetRotation());

        debugDrawCommandList.cylinder(cylinderTransform, fillColor, fillAttributes);
        debugDrawCommandList.cylinder(cylinderTransform, outlineColor, outlineAttributes);

        break;
    }
    default:
        break;
    }
}

void EditorCsgState::DrawTargetHighlight(DebugDrawCommandList& debugDrawCommandList) const
{
    static const RenderableAttributeSet highlightAttributes = BrushOverlayAttributes(FillMode::Fill);
    static const uint32 requiredLayoutMask = VT_Position | VT_Normal | VT_UV0;

    const Handle<Node> targetNode = m_targetNode.Lock();
    const MeshComponent* meshComponent = GetMeshComponent(targetNode.Get());

    if (!meshComponent || !meshComponent->mesh.IsValid())
    {
        return;
    }

    const Mesh& mesh = *meshComponent->mesh;

    if ((mesh.GetMeshAttributes().inputLayout.mask & requiredLayoutMask) != requiredLayoutMask)
    {
        return;
    }

    debugDrawCommandList.mesh(mesh, targetNode->GetWorldMatrix(), Color(1.0f, 0.72f, 0.25f, 0.12f), highlightAttributes);
}

void EditorCsgState::DebugDraw(DebugDrawCommandList& debugDrawCommandList)
{
    if (!m_enabled)
    {
        return;
    }

    DrawTargetHighlight(debugDrawCommandList);

    if (m_placement.active)
    {
        if (m_placement.hasHover)
        {
            const Transform placementTransform = MakePlacementTransform(m_placement.hoverPosition, m_placement.hoverNormal, m_placement.halfExtent);

            DrawBrush(debugDrawCommandList, m_placement.shape, placementTransform, 0.22f, 0.9f);
        }

        return;
    }

    if (!HasBrush())
    {
        return;
    }

    const bool isSelected = IsBrushSelected();

    DrawBrush(debugDrawCommandList, m_brushShape, m_brushNode->GetLocalTransform(), isSelected ? 0.2f : 0.08f, isSelected ? 0.85f : 0.4f);

    DrawSizeHandles(debugDrawCommandList);
}

#pragma endregion EditorCsgState

} // namespace Hyperion
