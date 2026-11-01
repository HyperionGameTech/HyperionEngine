/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <CsgEditor/EditorCsgState.hpp>
#include <CsgEditor/CsgEditorExtension.hpp>
#include <Editor/Terrain/EditorTerrainState.hpp>
#include <Editor/EditorSubsystem.hpp>
#include <Editor/EditorViewport.hpp>
#include <Editor/EditorProject.hpp>
#include <Editor/EditorAction.hpp>
#include <Editor/EditorActionStack.hpp>
#include <Editor/EditorConfig.hpp>

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
#include <Rendering/Material.hpp>
#include <Rendering/DebugDrawer.hpp>
#include <Rendering/Util/MeshBuilder.hpp>

#include <Core/Math/MathUtil.hpp>
#include <Core/Math/Quat4f.hpp>
#include <Core/Math/Ray.hpp>

#include <Core/Logging/Logger.hpp>

#include <EditorCsgState.generated.inl>

namespace Hyperion {

EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);

namespace {

HYP_FORCE_INLINE constexpr MeshBooleanOperation ToMeshBooleanOperation(CsgOperation operation)
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

HYP_FORCE_INLINE constexpr const char* GetOperationName(CsgOperation operation)
{
    return (uint32(operation) < NumCsgOperations) ? CsgOperationNames[uint32(operation)] : "";
}

HYP_FORCE_INLINE constexpr const char* GetBrushShapeName(CsgBrushShape shape)
{
    return (uint32(shape) < NumCsgBrushShapes) ? CsgBrushShapeNames[uint32(shape)] : "";
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
    materialAttributes.flags = MaterialAttributeFlags::None;

    return attributes;
}

EditorCsgState* FindCsgState(EditorSubsystem* editorSubsystem)
{
    Handle<CsgEditorExtension> extension = editorSubsystem->GetExtension<CsgEditorExtension>();

    if (!extension.IsValid())
    {
        return nullptr;
    }

    return extension->GetCsgState().Get();
}

MeshComponent* GetMeshComponent(Node* node)
{
    Entity* entity = TryCast<Entity>(node);

    return entity ? entity->TryGetComponent<MeshComponent>() : nullptr;
}

void AssignMesh(const Handle<Node>& node, const Handle<Mesh>& mesh)
{
    Entity* entity = TryCast<Entity>(node.Get());
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

Handle<Mesh> BuildResultMesh(const Mesh& sourceMesh, const MeshBooleanResult& result)
{
    VertexArrayView vertices {};
    vertices.floatData = result.vertexData.Data();
    vertices.vertexCount = result.meshDesc.lods[0].numVertices;
    vertices.layoutDesc = result.meshDesc.meshAttributes.inputLayout;

    MeshDataView meshData {};
    meshData.vertices[0] = vertices;
    meshData.indices[0] = result.indices.ToByteView();

    Handle<Mesh> mesh = MakeHandle<Mesh>();
    mesh->SetName(sourceMesh.GetName());
    mesh->SetMeshData(result.meshDesc, meshData);
    mesh->SetFlags(sourceMesh.GetFlags());

    MeshLodGenerationSettings lodGenerationSettings = sourceMesh.GetLodGenerationSettings();
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

Transform MakePlacementTransform(const Vec3f& position, const Vec3f& normal, const Vec3f& halfExtent, bool alignToSurface)
{
    Transform transform;

    if (alignToSurface)
    {
        transform.SetRotation(RotationFromUpTo(normal));
    }

    transform.SetScale(halfExtent);
    transform.SetTranslation(position);

    return transform;
}

Vec3f SnapAcrossSurface(const Vec3f& position, const Vec3f& normal, float gridSize)
{
    const Vec3f absoluteNormal = Vec3f::Abs(normal);

    int normalAxis = 0;

    if (absoluteNormal.y > absoluteNormal[normalAxis])
    {
        normalAxis = 1;
    }

    if (absoluteNormal.z > absoluteNormal[normalAxis])
    {
        normalAxis = 2;
    }

    Vec3f snapped = position;

    for (int axis = 0; axis < 3; axis++)
    {
        if (axis != normalAxis)
        {
            snapped[axis] = float(MathUtil::Round(position[axis] / gridSize)) * gridSize;
        }
    }

    return snapped;
}

bool IsEditableScene(const Scene* scene)
{
    if (!scene || scene->IsTransient())
    {
        return false;
    }

    const EnumFlags<SceneFlags> flags = scene->GetSceneFlags();

    return (flags & SceneFlags::FOREGROUND)
        && !(flags & (SceneFlags::BACKDROP | SceneFlags::EDITOR | SceneFlags::UI | SceneFlags::DETACHED));
}

MeshComponent* GetCandidateMeshComponent(Node* node)
{
    if (!node || !node->IsStatic() || !IsEditableScene(node->GetScene()))
    {
        return nullptr;
    }

    for (const Node* parent = node->GetParent(); parent != nullptr; parent = parent->GetParent())
    {
        if (!parent->IsStatic())
        {
            return nullptr;
        }
    }

    MeshComponent* meshComponent = GetMeshComponent(node);

    if (!meshComponent
        || !meshComponent->mesh.IsValid()
        || meshComponent->skeleton.IsValid()
        || meshComponent->numInstances != 0
        || meshComponent->mesh->IsDynamicMesh())
    {
        return nullptr;
    }

    return meshComponent;
}

void CollectOverlappingMeshEntities(const Node* node, const BoundingBox& bounds, Array<Entity*>& outEntities)
{
    for (const Handle<Node>& child : node->GetChildren())
    {
        if (!child.IsValid())
        {
            continue;
        }

        Entity* entity = TryCast<Entity>(child.Get());

        if (entity && GetCandidateMeshComponent(entity) && entity->GetWorldBounds().Overlaps(bounds))
        {
            outEntities.PushBack(entity);
        }

        CollectOverlappingMeshEntities(child.Get(), bounds, outEntities);
    }
}

const char* GetTargetIssueText(MeshBooleanError error)
{
    switch (error)
    {
    case MeshBooleanError::None:
        return "";
    case MeshBooleanError::TargetNotClosed:
        return "open surface";
    case MeshBooleanError::UnsupportedLayout:
        return "unsupported vertex layout";
    case MeshBooleanError::MissingData:
        return "no mesh data";
    default:
        return MeshBoolean::GetErrorMessage(error);
    }
}

bool FindSurfaceHit(EditorSubsystem* subsystem, const Vec2f& relativePos, Vec3f& outPosition, Vec3f& outNormal, Node*& outNode)
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

        if (!scene->GetOctree().TestRay(
            ray,
            results,
            RayTestFlags::TestBVH | RayTestFlags::EditorPick))
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
                outNode = hit.node;
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
static constexpr uint32 RegatherIntervalFrames = 30;

static constexpr const char* AlignToSurfaceConfigKey = "CsgTools.AlignToSurface";
static constexpr const char* ShowOutlinesConfigKey = "CsgTools.ShowOutlines";

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

    EditorConfig config;

    if (!config.Load())
    {
        return;
    }

    if (const ConfigValue& alignValue = config.Get(AlignToSurfaceConfigKey); alignValue.IsBool())
    {
        m_alignToSurface = alignValue.ToBool();
    }

    if (const ConfigValue& outlinesValue = config.Get(ShowOutlinesConfigKey); outlinesValue.IsBool())
    {
        m_outlinesShown = outlinesValue.ToBool();
    }
}

void EditorCsgState::SaveSettings() const
{
    EditorConfig config;
    config.Load();

    config.Set(AlignToSurfaceConfigKey, ConfigValue(m_alignToSurface));
    config.Set(ShowOutlinesConfigKey, ConfigValue(m_outlinesShown));

    if (!config.Save())
    {
        HYP_LOG(Editor, Warning, "Failed to save CSG tool settings");
    }
}

bool EditorCsgState::IsEnabled() const
{
    return m_enabled;
}

bool EditorCsgState::CanEnter() const
{
    AssertOnThread(g_simThread);

    if (m_enabled)
    {
        return true;
    }

    return MeshBoolean::IsSupported()
        && m_subsystem->GetCurrentProject().IsValid()
        && !m_subsystem->IsSimulating()
        && !m_subsystem->IsInIsolatedEditMode();
}

void EditorCsgState::Enter()
{
    AssertOnThread(g_simThread);

    if (m_enabled || !CanEnter())
    {
        return;
    }

    const Handle<Node> focusedNode = m_subsystem->GetFocusedNode();

    m_subsystem->ExitMeshEditMode(/* saveEdits */ true);
    m_subsystem->GetTerrainState()->SetEnabled(false);
    m_subsystem->DisableSurfacePainters();

    m_sessionId = CsgSessionId(uint32(m_sessionId) + 1);
    m_brushActions.Clear();
    m_applyActions.Clear();

    m_targets.Clear();
    m_listedTargets.Clear();
    m_validatedMeshes.Clear();
    m_placedOnNode.Reset();
    m_primaryTarget.Reset();
    m_targetsPinned = false;
    m_targetsHash = HashCode();
    m_hasGathered = false;

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

    m_statusText = String::empty;
    m_manipulationModeBeforeCsg = m_subsystem->GetSelectedManipulationMode();

    m_enabled = true;
    m_brushManipulationMode = EditorManipulationMode::Translate;

    m_subsystem->SetSelectedManipulationMode(EditorManipulationMode::None);

    if (GetCandidateMeshComponent(focusedNode.Get()))
    {
        CsgTarget& target = AddTarget(focusedNode);
        target.pinned = true;

        m_placedOnNode = focusedNode.ToWeak();
    }

    BeginPlacement(m_brushShape);

    RefreshTargets();

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

        FinalizeMeshes();
    }
    else
    {
        RemoveSessionActions(/* includeApplyActions */ true);

        for (const CsgTarget& target : m_targets)
        {
            Handle<Node> node = target.node.Lock();

            if (!node.IsValid())
            {
                continue;
            }

            if (target.created)
            {
                if (node->GetParent() != nullptr)
                {
                    node->Remove();
                }

                continue;
            }

            const MeshComponent* meshComponent = GetMeshComponent(node.Get());

            if (meshComponent && target.baselineMesh.IsValid() && meshComponent->mesh != target.baselineMesh)
            {
                AssignMesh(node, target.baselineMesh);
                m_subsystem->SyncBoxPhysicsShapeToLocalBounds(TryCast<Entity>(node.Get()));
            }
        }
    }

    m_enabled = false;

    m_targets.Clear();
    m_listedTargets.Clear();
    m_validatedMeshes.Clear();
    m_placedOnNode.Reset();
    m_primaryTarget.Reset();
    m_targetsPinned = false;
    m_targetsHash = HashCode();
    m_hasGathered = false;

    m_brushNode.Reset();

    m_brushActions.Clear();
    m_applyActions.Clear();

    m_statusText = String::empty;

    m_subsystem->SetSelectedManipulationMode(m_manipulationModeBeforeCsg);

    OnStateChanged();
}

uint32 EditorCsgState::GetTargetCount() const
{
    return m_enabled ? uint32(m_listedTargets.Size()) : 0;
}

Node* EditorCsgState::GetTargetNodeAt(uint32 index) const
{
    const CsgTarget* target = GetListedTarget(index);

    if (!target)
    {
        return nullptr;
    }

    return target->node.Lock().Get();
}

String EditorCsgState::GetTargetName(uint32 index) const
{
    const CsgTarget* target = GetListedTarget(index);

    if (!target)
    {
        return String::empty;
    }

    Handle<Node> node = target->node.Lock();

    if (!node.IsValid())
    {
        return String::empty;
    }

    return HYP_FORMAT("{}", node->GetName());
}

String EditorCsgState::GetTargetIssue(uint32 index) const
{
    const CsgTarget* target = GetListedTarget(index);

    if (!target)
    {
        return String::empty;
    }

    return String(GetTargetIssueText(target->error));
}

bool EditorCsgState::IsTargetIncluded(uint32 index) const
{
    const CsgTarget* target = GetListedTarget(index);

    return target != nullptr && target->included && target->error == MeshBooleanError::None;
}

void EditorCsgState::SetTargetIncluded(uint32 index, bool included)
{
    AssertOnThread(g_simThread);

    if (!m_enabled || index >= m_listedTargets.Size())
    {
        return;
    }

    CsgTarget& target = m_targets[m_listedTargets[index]];

    if (target.error != MeshBooleanError::None)
    {
        return;
    }

    target.included = included;
    target.includedByUser = true;

    m_statusText = String::empty;

    RefreshTargets();

    OnStateChanged();
}

bool EditorCsgState::AreTargetsPinned() const
{
    return m_enabled && m_targetsPinned;
}

void EditorCsgState::SetTargetsPinned(bool pinned)
{
    AssertOnThread(g_simThread);

    if (!m_enabled || pinned == m_targetsPinned)
    {
        return;
    }

    if (pinned)
    {
        for (uint32 targetIndex : m_listedTargets)
        {
            m_targets[targetIndex].pinned = true;
        }
    }
    else
    {
        for (CsgTarget& target : m_targets)
        {
            target.pinned = target.created;
        }
    }

    m_targetsPinned = pinned;

    RefreshTargets();

    OnStateChanged();
}

bool EditorCsgState::HasBrush() const
{
    return m_brushNode.IsValid()
        && m_brushNode->GetScene() != nullptr
        && m_brushNode->GetScene() == m_subsystem->GetEditorScene().Get();
}

bool EditorCsgState::HasPendingEdits() const
{
    if (!m_enabled)
    {
        return false;
    }

    for (const CsgTarget& target : m_targets)
    {
        Handle<Node> node = target.node.Lock();

        if (!node.IsValid())
        {
            continue;
        }

        if (target.created)
        {
            if (node->GetParent() != nullptr)
            {
                return true;
            }

            continue;
        }

        const MeshComponent* meshComponent = GetMeshComponent(node.Get());

        if (meshComponent && meshComponent->mesh != target.baselineMesh)
        {
            return true;
        }
    }

    return false;
}

bool EditorCsgState::CanApply() const
{
    if (!m_enabled || m_placement.active || !HasBrush())
    {
        return false;
    }

    if (m_operation == CsgOperation::Union)
    {
        return true;
    }

    for (uint32 targetIndex : m_listedTargets)
    {
        const CsgTarget& target = m_targets[targetIndex];

        if (target.included && target.error == MeshBooleanError::None)
        {
            return true;
        }
    }

    return false;
}

String EditorCsgState::GetStatusText() const
{
    if (!m_enabled)
    {
        return String::empty;
    }

    return m_statusText;
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
    m_statusText = String::empty;

    UpdateDefaultInclusion();

    OnStateChanged();
}

bool EditorCsgState::IsSessionActive(CsgSessionId sessionId) const
{
    return m_enabled && m_sessionId == sessionId;
}

bool EditorCsgState::IsSessionNode(const Node* node) const
{
    if (!m_enabled || node == nullptr)
    {
        return false;
    }

    if (node == m_brushNode.Get())
    {
        return true;
    }

    for (const CsgTarget& target : m_targets)
    {
        if (target.node.GetUnsafe() == node)
        {
            return true;
        }
    }

    return false;
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

EditorCsgState::CsgTarget* EditorCsgState::FindTarget(const Node* node)
{
    if (!node)
    {
        return nullptr;
    }

    for (CsgTarget& target : m_targets)
    {
        if (target.node.GetUnsafe() == node)
        {
            return &target;
        }
    }

    return nullptr;
}

EditorCsgState::CsgTarget& EditorCsgState::AddTarget(const Handle<Node>& node)
{
    if (CsgTarget* existingTarget = FindTarget(node.Get()))
    {
        return *existingTarget;
    }

    CsgTarget& target = m_targets.EmplaceBack();
    target.node = node.ToWeak();

    if (MeshComponent* meshComponent = GetMeshComponent(node.Get()))
    {
        target.baselineMesh = meshComponent->mesh;
    }

    ValidateTarget(target);

    return target;
}

const EditorCsgState::CsgTarget* EditorCsgState::GetListedTarget(uint32 index) const
{
    if (!m_enabled || index >= m_listedTargets.Size())
    {
        return nullptr;
    }

    return &m_targets[m_listedTargets[index]];
}

bool EditorCsgState::IsTargetListed(const CsgTarget& target) const
{
    if (!target.overlapping && !target.pinned)
    {
        return false;
    }

    Handle<Node> node = target.node.Lock();

    return node.IsValid() && node->GetScene() != nullptr && GetMeshComponent(node.Get()) != nullptr;
}

MeshBooleanError EditorCsgState::ValidateMesh(const Handle<Mesh>& mesh)
{
    if (!mesh.IsValid())
    {
        return MeshBooleanError::MissingData;
    }

    for (const ValidatedMesh& validatedMesh : m_validatedMeshes)
    {
        if (validatedMesh.mesh == mesh)
        {
            return validatedMesh.error;
        }
    }

    const MeshBooleanError error = MeshBoolean::ValidateSolid(mesh.Get());

    if (error != MeshBooleanError::None)
    {
        HYP_LOG(Editor, Info, "CSG: {} can't be a target - {}", mesh->GetName(), MeshBoolean::GetErrorMessage(error));
    }

    m_validatedMeshes.PushBack(ValidatedMesh { mesh, error });

    return error;
}

void EditorCsgState::ValidateTarget(CsgTarget& target)
{
    Handle<Node> node = target.node.Lock();
    MeshComponent* meshComponent = GetMeshComponent(node.Get());

    Handle<Mesh> mesh;

    if (meshComponent)
    {
        mesh = meshComponent->mesh;
    }

    if (mesh.IsValid() && target.validatedMesh == mesh)
    {
        return;
    }

    target.validatedMesh = mesh;
    target.error = ValidateMesh(mesh);
}

Transform EditorCsgState::GetPlacementTransform() const
{
    return MakePlacementTransform(m_placement.hoverPosition, m_placement.hoverNormal, m_placement.halfExtent, m_alignToSurface);
}

bool EditorCsgState::GetActiveBrushTransform(CsgBrushShape& outShape, Transform& outTransform) const
{
    if (m_placement.active)
    {
        if (!m_placement.hasHover)
        {
            return false;
        }

        outShape = m_placement.shape;
        outTransform = GetPlacementTransform();

        return true;
    }

    if (!HasBrush())
    {
        return false;
    }

    outShape = m_brushShape;
    outTransform = m_brushNode->GetLocalTransform();

    return true;
}

void EditorCsgState::RefreshTargets()
{
    CsgBrushShape shape = m_brushShape;
    Transform brushTransform;

    const Handle<EditorProject>& project = m_subsystem->GetCurrentProject();

    const bool hasBrush = !m_targetsPinned && project.IsValid() && project->GetWorld().IsValid() && GetActiveBrushTransform(shape, brushTransform);

    Mat4f brushMatrix;

    if (hasBrush)
    {
        brushMatrix = brushTransform.GetMatrix();
    }

    const bool regather = !hasBrush
        || !m_hasGathered
        || shape != m_gatheredShape
        || brushMatrix != m_gatheredBrushMatrix
        || ++m_framesSinceGather >= RegatherIntervalFrames;

    if (regather)
    {
        for (CsgTarget& target : m_targets)
        {
            target.overlapping = false;
            target.overlap = 0.0f;
        }

        m_hasGathered = hasBrush;
        m_gatheredShape = shape;
        m_gatheredBrushMatrix = brushMatrix;
        m_framesSinceGather = 0;
    }

    if (regather && hasBrush)
    {
        const BoundingBox brushLocalBounds = GetBrushMesh(shape)->GetAABB();
        const Mat4f inverseBrushMatrix = brushMatrix.Inverse();
        const BoundingBox brushWorldBounds = brushMatrix * brushLocalBounds;

        Array<Entity*> candidates;

        for (const Handle<Scene>& scene : project->GetWorld()->GetScenes())
        {
            if (!IsEditableScene(scene.Get()))
            {
                continue;
            }

            CollectOverlappingMeshEntities(scene->GetRoot().Get(), brushWorldBounds, candidates);
        }

        for (Entity* entity : candidates)
        {
            MeshComponent* meshComponent = GetCandidateMeshComponent(entity);

            if (!meshComponent)
            {
                continue;
            }

            const BoundingBox boundsInBrush = (inverseBrushMatrix * entity->GetWorldMatrix()) * meshComponent->mesh->GetAABB();

            if (!boundsInBrush.Overlaps(brushLocalBounds))
            {
                continue;
            }

            const Vec3f overlapExtent = boundsInBrush.Intersection(brushLocalBounds).GetExtent();

            CsgTarget& target = AddTarget(MakeStrongRef(static_cast<Node*>(entity)));
            target.overlapping = true;
            target.overlap = overlapExtent.x * overlapExtent.y * overlapExtent.z;
        }
    }

    for (size_t targetIndex = 0; targetIndex < m_targets.Size();)
    {
        CsgTarget& target = m_targets[targetIndex];
        Handle<Node> node = target.node.Lock();

        bool keep = node.IsValid() && (target.overlapping || target.pinned || target.created);

        if (node.IsValid() && !keep)
        {
            const MeshComponent* meshComponent = GetMeshComponent(node.Get());

            keep = meshComponent != nullptr && meshComponent->mesh != target.baselineMesh;
        }

        if (!keep)
        {
            m_targets.EraseAt(targetIndex);

            continue;
        }

        ValidateTarget(target);

        targetIndex++;
    }

    m_listedTargets.Clear();

    for (uint32 targetIndex = 0; targetIndex < uint32(m_targets.Size()); targetIndex++)
    {
        if (IsTargetListed(m_targets[targetIndex]))
        {
            m_listedTargets.PushBack(targetIndex);
        }
    }

    Handle<Node> placedOn;

    if (m_placement.active)
    {
        placedOn = m_placement.hoverNode.Lock();
    }
    else
    {
        placedOn = m_placedOnNode.Lock();
    }

    const CsgTarget* primaryTarget = FindTarget(placedOn.Get());

    if (!primaryTarget || !IsTargetListed(*primaryTarget) || primaryTarget->error != MeshBooleanError::None)
    {
        primaryTarget = nullptr;

        for (uint32 targetIndex : m_listedTargets)
        {
            const CsgTarget& target = m_targets[targetIndex];

            if (target.error == MeshBooleanError::None && (!primaryTarget || target.overlap > primaryTarget->overlap))
            {
                primaryTarget = &target;
            }
        }
    }

    if (primaryTarget)
    {
        m_primaryTarget = primaryTarget->node;
    }
    else
    {
        m_primaryTarget.Reset();
    }

    UpdateDefaultInclusion();

    HashCode targetsHash;

    for (uint32 targetIndex : m_listedTargets)
    {
        const CsgTarget& target = m_targets[targetIndex];

        targetsHash.Add(target.node.GetHashCode().Value());
        targetsHash.Add(target.included);
        targetsHash.Add(uint32(target.error));
    }

    if (targetsHash != m_targetsHash)
    {
        m_targetsHash = targetsHash;

        OnStateChanged();
    }
}

void EditorCsgState::UpdateDefaultInclusion()
{
    const Handle<Node> primaryTarget = m_primaryTarget.Lock();

    for (CsgTarget& target : m_targets)
    {
        if (target.includedByUser)
        {
            continue;
        }

        target.included = m_operation == CsgOperation::Subtract
            || (primaryTarget.IsValid() && target.node == primaryTarget);
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

Transform EditorCsgState::MakeDefaultBrushTransform() const
{
    const Vec3f halfExtent = GetBrushHalfExtent();

    Handle<Node> anchor = m_placedOnNode.Lock();

    if (!anchor.IsValid())
    {
        anchor = m_primaryTarget.Lock();
    }

    if (!anchor.IsValid())
    {
        return Transform(m_subsystem->CalculateSceneInsertionPoint(), halfExtent, Quat4f::Identity());
    }

    const BoundingBox anchorBounds = anchor->GetWorldBounds();

    if (!anchorBounds.IsValid() || !anchorBounds.IsFinite())
    {
        return Transform(anchor->GetWorldTranslation(), halfExtent, Quat4f::Identity());
    }

    return Transform(anchorBounds.GetCenter() + Vec3f(0.0f, anchorBounds.GetExtent().y * 0.5f, 0.0f), halfExtent, Quat4f::Identity());
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
    m_statusText = String::empty;

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
        m_subsystem->SetFocusedNode(Handle<Node>::Null(), /* shouldSelectInOutline */ false);
    }

    OnStateChanged();
}

void EditorCsgState::PlaceBrush(CsgBrushShape shape, const Transform& transform)
{
    if (!m_enabled || uint32(shape) >= NumCsgBrushShapes)
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

                        if (EditorCsgState* csgState = FindCsgState(editorSubsystem))
                        {
                            csgState->AttachBrush(brushNode, shape);
                        }
                    },
                    [brushNode, previousShape, previousTransform](EditorSubsystem* editorSubsystem, EditorProject* editorProject)
                    {
                        brushNode->SetLocalTransform(previousTransform);

                        if (EditorCsgState* csgState = FindCsgState(editorSubsystem))
                        {
                            csgState->AttachBrush(brushNode, previousShape);
                        }
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
                    if (EditorCsgState* csgState = FindCsgState(editorSubsystem))
                    {
                        csgState->AttachBrush(brushNode, shape);
                    }
                },
                [brushNode](EditorSubsystem* editorSubsystem, EditorProject* editorProject)
                {
                    if (EditorCsgState* csgState = FindCsgState(editorSubsystem))
                    {
                        csgState->DetachBrush(brushNode);
                    }
                }
            };
        }));
}

void EditorCsgState::AddBrush(CsgBrushShape shape)
{
    AssertOnThread(g_simThread);

    if (!m_enabled)
    {
        return;
    }

    PlaceBrush(shape, MakeDefaultBrushTransform());
}

void EditorCsgState::SelectBrushShape(CsgBrushShape shape)
{
    AssertOnThread(g_simThread);

    BeginPlacement(shape);
}

void EditorCsgState::BeginPlacement(CsgBrushShape shape)
{
    if (!m_enabled || uint32(shape) >= NumCsgBrushShapes)
    {
        return;
    }

    m_placement = PlacementState {};
    m_placement.active = true;
    m_placement.shape = shape;
    m_placement.halfExtent = NormalizeHalfExtents(shape, GetBrushHalfExtent());

    m_statusText = String::empty;

    if (IsBrushSelected())
    {
        m_subsystem->SetFocusedNode(Handle<Node>::Null(), /* shouldSelectInOutline */ false);
    }

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

    Node* hoverNode = nullptr;

    m_placement.hasHover = FindSurfaceHit(m_subsystem, relativePos, m_placement.hoverPosition, m_placement.hoverNormal, hoverNode);

    if (m_placement.hasHover && m_subsystem->IsSnapToGridEnabled())
    {
        m_placement.hoverPosition = SnapAcrossSurface(m_placement.hoverPosition, m_placement.hoverNormal, MathUtil::Max(m_subsystem->GetGridSize(), MinimumBrushSize));
    }

    if (m_placement.hasHover && hoverNode != nullptr)
    {
        m_placement.hoverNode = MakeWeakRef(hoverNode);
    }
    else
    {
        m_placement.hoverNode.Reset();
    }
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
    const Transform transform = GetPlacementTransform();

    m_lastBrushHalfExtent = m_placement.halfExtent;
    m_placedOnNode = m_placement.hoverNode;

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

    if (!m_enabled || !HasBrush())
    {
        return;
    }

    SetBrushTransformUndoable(m_brushNode->GetLocalTransform(), MakeDefaultBrushTransform(), "Reset CSG Brush");

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

                    if (EditorCsgState* csgState = FindCsgState(editorSubsystem))
                    {
                        csgState->OnStateChanged();
                    }
                },
                [brushNode, previousTransform](EditorSubsystem* editorSubsystem, EditorProject* editorProject)
                {
                    brushNode->SetLocalTransform(previousTransform);

                    if (EditorCsgState* csgState = FindCsgState(editorSubsystem))
                    {
                        csgState->OnStateChanged();
                    }
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
    static const RenderableAttributeSet faceOutlineAttributes = BrushOverlayAttributes(FillMode::Line);

    FixedArray<SizeHandle, MaxSizeHandles> handles;
    const uint32 numHandles = GetSizeHandles(m_brushShape, handles);

    const Transform& brushTransform = m_brushNode->GetLocalTransform();
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

            if (!handle.isRadius)
            {
                Vec3f faceHalfExtents = brushTransform.GetScale();
                faceHalfExtents[int(handle.axis)] = MinimumBrushSize * 0.1f;

                const Transform faceTransform(handlePosition, faceHalfExtents, brushTransform.GetRotation());

                if (m_brushShape == CsgBrushShape::Cylinder)
                {
                    debugDrawCommandList.cylinder(faceTransform, Color(1.0f, 0.85f, 0.2f, 0.25f), handleAttributes);
                    debugDrawCommandList.cylinder(faceTransform, Color(1.0f, 0.85f, 0.2f, 0.9f), faceOutlineAttributes);
                }
                else
                {
                    debugDrawCommandList.box(faceTransform, Color(1.0f, 0.85f, 0.2f, 0.25f), handleAttributes);
                    debugDrawCommandList.box(faceTransform, Color(1.0f, 0.85f, 0.2f, 0.9f), faceOutlineAttributes);
                }
            }
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

bool EditorCsgState::IsAlignToSurface() const
{
    return m_alignToSurface;
}

void EditorCsgState::SetAlignToSurface(bool alignToSurface)
{
    AssertOnThread(g_simThread);

    if (alignToSurface == m_alignToSurface)
    {
        return;
    }

    m_alignToSurface = alignToSurface;

    SaveSettings();

    OnStateChanged();
}

bool EditorCsgState::AreOutlinesShown() const
{
    return m_outlinesShown;
}

void EditorCsgState::SetOutlinesShown(bool outlinesShown)
{
    AssertOnThread(g_simThread);

    if (outlinesShown == m_outlinesShown)
    {
        return;
    }

    m_outlinesShown = outlinesShown;

    SaveSettings();

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
                    if (EditorCsgState* csgState = FindCsgState(editorSubsystem))
                    {
                        csgState->DetachBrush(brushNode);
                    }
                },
                [brushNode, shape](EditorSubsystem* editorSubsystem, EditorProject* editorProject)
                {
                    if (EditorCsgState* csgState = FindCsgState(editorSubsystem))
                    {
                        csgState->AttachBrush(brushNode, shape);
                    }
                }
            };
        }));
}

namespace {

struct CsgMeshChange
{
    WeakHandle<Node> node;
    Handle<Mesh> previousMesh;
    Handle<Mesh> resultMesh;
};

bool IsUnchangedResult(const Mesh& sourceMesh, const MeshBooleanResult& result)
{
    if (result.meshDesc.lods[0].numIndices != sourceMesh.GetMeshDesc().lods[0].numIndices)
    {
        return false;
    }

    const size_t stride = result.meshDesc.meshAttributes.inputLayout.VertexSize() / sizeof(float);

    if (stride == 0)
    {
        return false;
    }

    BoundingBox resultBounds = BoundingBox::Empty();

    for (size_t offset = 0; offset + 2 < result.vertexData.Size(); offset += stride)
    {
        resultBounds = resultBounds.Union(Vec3f(result.vertexData[offset], result.vertexData[offset + 1], result.vertexData[offset + 2]));
    }

    const BoundingBox& sourceBounds = sourceMesh.GetAABB();

    const Vec3f extent = sourceBounds.GetExtent();
    const float tolerance = MathUtil::Max(extent.x, MathUtil::Max(extent.y, extent.z)) * 0.0001f;

    const Vec3f minDelta = Vec3f::Abs(resultBounds.GetMin() - sourceBounds.GetMin());
    const Vec3f maxDelta = Vec3f::Abs(resultBounds.GetMax() - sourceBounds.GetMax());

    return MathUtil::Max(minDelta.x, MathUtil::Max(minDelta.y, minDelta.z)) <= tolerance
        && MathUtil::Max(maxDelta.x, MathUtil::Max(maxDelta.y, maxDelta.z)) <= tolerance;
}

} // namespace

void EditorCsgState::RefreshBrushAfterApply(EditorSubsystem* editorSubsystem, const Handle<Node>& brushNode, bool keepBrush, CsgSessionId sessionId)
{
    EditorCsgState* csgState = FindCsgState(editorSubsystem);

    if (!csgState || !csgState->IsSessionActive(sessionId))
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
}

void EditorCsgState::RestoreBrushAfterUndo(EditorSubsystem* editorSubsystem, const Handle<Node>& brushNode, CsgBrushShape shape, CsgSessionId sessionId)
{
    EditorCsgState* csgState = FindCsgState(editorSubsystem);

    if (csgState && csgState->IsSessionActive(sessionId))
    {
        csgState->AttachBrush(brushNode, shape);
    }
}

void EditorCsgState::ApplyBrush()
{
    AssertOnThread(g_simThread);

    if (!CanApply())
    {
        return;
    }

    RefreshTargets();

    const Handle<Node> brushNode = m_brushNode;
    const CsgBrushShape shape = m_brushShape;
    const CsgOperation operation = m_operation;
    const bool keepBrush = m_keepBrushAfterApply;

    bool hasIncludedTarget = false;

    for (uint32 targetIndex : m_listedTargets)
    {
        const CsgTarget& target = m_targets[targetIndex];

        if (target.included && target.error == MeshBooleanError::None)
        {
            hasIncludedTarget = true;

            break;
        }
    }

    if (hasIncludedTarget)
    {
        ApplyToTargets(brushNode, shape, operation, keepBrush);
    }
    else if (operation == CsgOperation::Union)
    {
        CreateUnionMesh(brushNode, shape, keepBrush);
    }

    if (!keepBrush && !HasBrush())
    {
        const String statusText = m_statusText;

        BeginPlacement(shape);

        m_statusText = statusText;
    }
}

void EditorCsgState::ApplyToTargets(const Handle<Node>& brushNode, CsgBrushShape shape, CsgOperation operation, bool keepBrush)
{
    const Mat4f brushWorldMatrix = brushNode->GetWorldMatrix();

    Array<CsgMeshChange> changes;

    uint32 numAttempted = 0;
    uint32 numFailed = 0;

    MeshBooleanError firstError = MeshBooleanError::None;
    String firstFailedName;

    for (uint32 targetIndex : m_listedTargets)
    {
        CsgTarget& target = m_targets[targetIndex];

        if (!target.included || target.error != MeshBooleanError::None)
        {
            continue;
        }

        Handle<Node> node = target.node.Lock();
        MeshComponent* meshComponent = GetMeshComponent(node.Get());

        if (!meshComponent || !meshComponent->mesh.IsValid())
        {
            continue;
        }

        numAttempted++;

        const Handle<Mesh> previousMesh = meshComponent->mesh;

        const MeshBooleanOperand targetOperand { previousMesh.Get(), Mat4f::Identity() };
        const MeshBooleanOperand brushOperand { GetBrushMesh(shape).Get(), node->GetWorldMatrix().Inverse() * brushWorldMatrix };

        const MeshBooleanResult result = MeshBoolean::Apply(targetOperand, brushOperand, ToMeshBooleanOperation(operation));

        if (result.error != MeshBooleanError::None)
        {
            HYP_LOG(Editor, Warning, "CSG {} on {} failed: {}", GetOperationName(operation), node->GetName(), MeshBoolean::GetErrorMessage(result.error));

            numFailed++;

            if (firstError == MeshBooleanError::None)
            {
                firstError = result.error;
                firstFailedName = HYP_FORMAT("{}", node->GetName());
            }

            continue;
        }

        if (IsUnchangedResult(*previousMesh, result))
        {
            continue;
        }

        Handle<Mesh> resultMesh = BuildResultMesh(*previousMesh, result);

        target.validatedMesh = resultMesh;
        target.error = MeshBooleanError::None;

        m_validatedMeshes.PushBack(ValidatedMesh { resultMesh, MeshBooleanError::None });

        changes.PushBack(CsgMeshChange { node.ToWeak(), previousMesh, resultMesh });
    }

    m_hasGathered = false;

    if (changes.Empty())
    {
        if (numFailed != 0)
        {
            m_statusText = HYP_FORMAT("{}: {}", firstFailedName, MeshBoolean::GetErrorMessage(firstError));
        }
        else
        {
            m_statusText = "The brush doesn't change the included meshes";
        }

        OnStateChanged();

        return;
    }

    if (numFailed != 0)
    {
        m_statusText = HYP_FORMAT("Applied to {} of {} meshes. {}: {}", changes.Size(), numAttempted, firstFailedName, MeshBoolean::GetErrorMessage(firstError));
    }
    else
    {
        m_statusText = String::empty;
    }

    PushApplyAction(MakeHandle<FunctionalEditorAction>(
        HYP_FORMAT("CSG {} ({})", GetOperationName(operation), GetBrushShapeName(shape)),
        [changes = std::move(changes), brushNode, shape, keepBrush, sessionId = m_sessionId]() -> EditorActionFunctions
        {
            return {
                [changes, brushNode, keepBrush, sessionId](EditorSubsystem* editorSubsystem, EditorProject* editorProject)
                {
                    for (const CsgMeshChange& change : changes)
                    {
                        Handle<Node> node = change.node.Lock();

                        if (!node.IsValid())
                        {
                            continue;
                        }

                        AssignMesh(node, change.resultMesh);
                        editorSubsystem->SyncBoxPhysicsShapeToLocalBounds(TryCast<Entity>(node.Get()));
                    }

                    RefreshBrushAfterApply(editorSubsystem, brushNode, keepBrush, sessionId);
                },
                [changes, brushNode, shape, sessionId](EditorSubsystem* editorSubsystem, EditorProject* editorProject)
                {
                    for (const CsgMeshChange& change : changes)
                    {
                        Handle<Node> node = change.node.Lock();

                        if (!node.IsValid())
                        {
                            continue;
                        }

                        AssignMesh(node, change.previousMesh);
                        editorSubsystem->SyncBoxPhysicsShapeToLocalBounds(TryCast<Entity>(node.Get()));
                    }

                    RestoreBrushAfterUndo(editorSubsystem, brushNode, shape, sessionId);
                }
            };
        }));
}

void EditorCsgState::CreateUnionMesh(const Handle<Node>& brushNode, CsgBrushShape shape, bool keepBrush)
{
    const Handle<Scene>& scene = m_subsystem->GetActiveScene();

    if (!scene.IsValid())
    {
        return;
    }

    const Transform brushTransform = brushNode->GetLocalTransform();

    Handle<Mesh> mesh = MeshBuilder::ApplyTransform(GetBrushMesh(shape).Get(), Transform(Vec3f(0.0f), brushTransform.GetScale(), Quat4f::Identity()));
    mesh->SetName(NAME("CsgUnionMesh"));

    BVHNode bvh;
    mesh->BuildBVH(bvh);

    {
        auto writeScope = mesh->GetWriteScope();
        mesh->SetBVH(std::move(bvh));
    }

    mesh->UploadGpuData();

    MaterialAttributes materialAttributes;
    materialAttributes.shaderName = NAME("GeometryPass");

    Handle<Material> material = MakeHandle<Material>(NAME("CsgUnionMaterial"), materialAttributes);

    Handle<Entity> entity = MakeHandle<Entity>();
    entity->SetName(NAME("CsgUnion"));
    entity->SetLocalTransform(Transform(brushTransform.GetTranslation(), Vec3f(1.0f), brushTransform.GetRotation()));

    m_statusText = String::empty;

    PushApplyAction(MakeHandle<FunctionalEditorAction>(
        HYP_FORMAT("CSG Union ({})", GetBrushShapeName(shape)),
        [scene, entity, mesh, material, brushNode, shape, keepBrush, sessionId = m_sessionId]() -> EditorActionFunctions
        {
            return {
                [scene, entity, mesh, material, brushNode, keepBrush, sessionId](EditorSubsystem* editorSubsystem, EditorProject* editorProject)
                {
                    scene->GetRoot()->AddChild(entity);

                    if (MeshComponent* meshComponent = entity->TryGetComponent<MeshComponent>())
                    {
                        meshComponent->mesh = mesh;
                        meshComponent->material = material;
                    }
                    else
                    {
                        MeshComponent newMeshComponent;
                        newMeshComponent.mesh = mesh;
                        newMeshComponent.material = material;

                        entity->AddComponent<MeshComponent>(newMeshComponent);
                    }

                    entity->SetLocalBounds(mesh->GetAABB());
                    entity->SetNeedsRenderProxyUpdate();

                    RefreshBrushAfterApply(editorSubsystem, brushNode, keepBrush, sessionId);
                },
                [entity, brushNode, shape, sessionId](EditorSubsystem* editorSubsystem, EditorProject* editorProject)
                {
                    entity->Remove();

                    RestoreBrushAfterUndo(editorSubsystem, brushNode, shape, sessionId);
                }
            };
        }));

    CsgTarget& target = AddTarget(entity);
    target.baselineMesh.Reset();
    target.createdMaterial = material;
    target.created = true;
    target.pinned = true;

    m_placedOnNode = entity.ToWeak();
    m_hasGathered = false;

    RefreshTargets();
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

void EditorCsgState::FinalizeMeshes()
{
    for (const CsgTarget& target : m_targets)
    {
        Handle<Node> node = target.node.Lock();

        if (!node.IsValid() || node->GetScene() == nullptr)
        {
            continue;
        }

        const MeshComponent* meshComponent = GetMeshComponent(node.Get());

        if (!meshComponent || !meshComponent->mesh.IsValid())
        {
            continue;
        }

        const Handle<Mesh> finalMesh = meshComponent->mesh;

        if (!target.created && finalMesh == target.baselineMesh)
        {
            continue;
        }

        if (target.baselineMesh.IsValid() && target.baselineMesh->GetMeshDesc().GetNumLods() > 1 && finalMesh->CanGenerateLods())
        {
            finalMesh->GenerateLods();
        }

        if (!finalMesh->IsRegistered())
        {
            GetCurrentAssetRegistry()->PutAssetUnique(finalMesh);
        }

        if (target.createdMaterial.IsValid() && !target.createdMaterial->IsRegistered())
        {
            GetCurrentAssetRegistry()->PutAssetUnique(target.createdMaterial);
        }

        if (Entity* entity = TryCast<Entity>(node.Get()))
        {
            RigidBodyComponent* rigidBodyComponent = entity->TryGetComponent<RigidBodyComponent>();

            if (rigidBodyComponent && rigidBodyComponent->shape.IsValid() && rigidBodyComponent->shape->GetType() == PhysicsShapeType::Compound)
            {
                const CompoundPhysicsShape* compoundShape = static_cast<const CompoundPhysicsShape*>(rigidBodyComponent->shape.Get());

                m_subsystem->GenerateConvexCollisionWithSettings(node.Get(), compoundShape->GetDecompositionSettings());
            }
        }
    }
}

void EditorCsgState::OnFocusedNodeChanged(const Handle<Node>& focusedNode)
{
    if (!m_enabled)
    {
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

    RefreshTargets();
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
    static const RenderableAttributeSet excludedAttributes = BrushOverlayAttributes(FillMode::Line);
    static const uint32 requiredLayoutMask = VT_Position | VT_Normal | VT_UV0;

    for (uint32 targetIndex : m_listedTargets)
    {
        const CsgTarget& target = m_targets[targetIndex];

        const Handle<Node> targetNode = target.node.Lock();
        const MeshComponent* meshComponent = GetMeshComponent(targetNode.Get());

        if (!meshComponent || !meshComponent->mesh.IsValid())
        {
            continue;
        }

        const Mesh& mesh = *meshComponent->mesh;

        if (!target.included || target.error != MeshBooleanError::None)
        {
            const BoundingBox bounds = targetNode->GetWorldBounds();

            if (m_outlinesShown && bounds.IsValid() && bounds.IsFinite())
            {
                debugDrawCommandList.box(Transform(bounds.GetCenter(), bounds.GetExtent() * 0.5f, Quat4f::Identity()), Color(0.7f, 0.7f, 0.7f, 0.35f), excludedAttributes);
            }

            continue;
        }

        if ((mesh.GetMeshAttributes().inputLayout.mask & requiredLayoutMask) != requiredLayoutMask)
        {
            continue;
        }

        debugDrawCommandList.mesh(mesh, targetNode->GetWorldMatrix(), Color(1.0f, 0.72f, 0.25f, 0.12f), highlightAttributes);
    }
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
            const Transform placementTransform = GetPlacementTransform();

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
