/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <Editor/Decal/EditorDecalPainterState.hpp>
#include <Editor/Terrain/EditorTerrainState.hpp>
#include <Editor/EditorSubsystem.hpp>
#include <Editor/EditorViewport.hpp>
#include <Editor/EditorProject.hpp>
#include <Editor/EditorAction.hpp>
#include <Editor/EditorActionStack.hpp>

#include <Scene/Scene.hpp>
#include <Scene/World.hpp>
#include <Scene/Decal/Decal.hpp>
#include <Scene/Decal/DecalProxy.hpp>
#include <Scene/EntityManager.hpp>

#include <Scene/Camera/Camera.hpp>

#include <Asset/AssetRegistry.hpp>

#include <Rendering/DebugDrawer.hpp>

#include <Framework/CVarManager.hpp>

#include <Core/Math/MathUtil.hpp>
#include <Core/Math/Quat4f.hpp>
#include <Core/Math/Ray.hpp>

#include <Core/Logging/Logger.hpp>

#include <EditorDecalPainterState.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

static CVar<bool> s_cvDecalsDebugDraw { "Rendering.Decals.DebugDraw", false };

namespace
{

template <class Callable>
void DispatchToSimThread(Callable&& callable)
{
    if (IsOnThread(g_simThread))
    {
        callable();
    }
    else
    {
        GetThreadById(g_simThread)->GetScheduler().Enqueue(std::forward<Callable>(callable), TaskEnqueueFlags::FIRE_AND_FORGET);
    }
}

// standard (Hamilton) rotation taking +Y onto the given direction
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

RenderableAttributeSet DecalCursorDrawAttributes()
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

} // anonymous namespace

#pragma region EditorDecalPainterState

EditorDecalPainterState::EditorDecalPainterState() = default;

EditorDecalPainterState::~EditorDecalPainterState() = default;

void EditorDecalPainterState::Initialize(EditorSubsystem* subsystem)
{
    AssertDebug(subsystem != nullptr);

    m_subsystem = subsystem;
}

bool EditorDecalPainterState::IsEnabled() const
{
    return m_enabled;
}

void EditorDecalPainterState::SetEnabled(bool enabled)
{
    DispatchToSimThread([this, enabled]()
        {
            AssertOnThread(g_simThread);

            if (enabled && !CanEnterDecalTools())
            {
                return;
            }

            if (!enabled && m_isStroking)
            {
                EndStroke();
            }

            // the terrain brush and the decal painter both own left-drag in the viewport
            if (enabled)
            {
                m_subsystem->GetTerrainState()->SetEnabled(false);
            }

            m_enabled = enabled;

            if (!m_enabled)
            {
                m_hasHover = false;
            }
        });
}

void EditorDecalPainterState::Toggle()
{
    DispatchToSimThread([this]()
        {
            SetEnabled(!m_enabled);
        });
}

bool EditorDecalPainterState::CanEnterDecalTools() const
{
    AssertOnThread(g_simThread);

    // simulation runs against a throwaway snapshot of the edited world, anything painted there would be lost
    if (m_subsystem->IsSimulating())
    {
        HYP_LOG(Editor, Warning, "Cannot use the decal painter while simulation is active");

        return false;
    }

    return true;
}

const Handle<Decal>& EditorDecalPainterState::GetActiveDecal() const
{
    return m_activeDecal;
}

void EditorDecalPainterState::SetActiveDecal(const Handle<Decal>& decal)
{
    DispatchToSimThread([this, decal]()
        {
            AssertOnThread(g_simThread);

            m_activeDecal = decal;
        });
}

void EditorDecalPainterState::SetActiveDecalByName(Name assetName)
{
    DispatchToSimThread([this, assetName]()
        {
            AssertOnThread(g_simThread);

            Handle<AssetRegistry> registry = GetCurrentAssetRegistry();

            if (!registry)
            {
                return;
            }

            Handle<Decal> decal = DynamicCast<Decal>(registry->GetAsset(AssetBuckets::Decals, assetName));

            if (!decal.IsValid())
            {
                HYP_LOG(Editor, Warning, "Decal painter: '{}' is not a Decal asset", assetName);

                return;
            }

            m_activeDecal = std::move(decal);
        });
}

float EditorDecalPainterState::GetScale() const
{
    return m_scale;
}

void EditorDecalPainterState::SetScale(float scale)
{
    DispatchToSimThread([this, scale]()
        {
            m_scale = MathUtil::Max(scale, 0.01f);
        });
}

float EditorDecalPainterState::GetRotationDegrees() const
{
    return m_rotationDegrees;
}

void EditorDecalPainterState::SetRotationDegrees(float rotationDegrees)
{
    DispatchToSimThread([this, rotationDegrees]()
        {
            m_rotationDegrees = rotationDegrees;

            if (!m_randomRotation)
            {
                m_nextRotationDegrees = rotationDegrees;
            }
        });
}

bool EditorDecalPainterState::GetRandomRotation() const
{
    return m_randomRotation;
}

void EditorDecalPainterState::SetRandomRotation(bool randomRotation)
{
    DispatchToSimThread([this, randomRotation]()
        {
            m_randomRotation = randomRotation;

            if (!m_randomRotation)
            {
                m_nextRotationDegrees = m_rotationDegrees;
            }
        });
}

float EditorDecalPainterState::GetSpacing() const
{
    return m_spacing;
}

void EditorDecalPainterState::SetSpacing(float spacing)
{
    DispatchToSimThread([this, spacing]()
        {
            m_spacing = MathUtil::Max(spacing, 0.0f);
        });
}

float EditorDecalPainterState::GetEraseRadius() const
{
    return m_eraseRadius;
}

void EditorDecalPainterState::SetEraseRadius(float eraseRadius)
{
    DispatchToSimThread([this, eraseRadius]()
        {
            m_eraseRadius = MathUtil::Max(eraseRadius, 0.05f);
        });
}

bool EditorDecalPainterState::GetAlignToSurface() const
{
    return m_alignToSurface;
}

void EditorDecalPainterState::SetAlignToSurface(bool alignToSurface)
{
    DispatchToSimThread([this, alignToSurface]()
        {
            m_alignToSurface = alignToSurface;
        });
}

bool EditorDecalPainterState::TryGetSurfaceHit(const Vec2f& relativePos, SurfaceHit& outHit) const
{
    AssertOnThread(g_simThread);

    EditorViewport* activeViewport = m_subsystem->GetActiveViewport();

    if (!activeViewport || !m_subsystem->GetCurrentProject().IsValid())
    {
        return false;
    }

    const Handle<World>& world = m_subsystem->GetCurrentProject()->GetWorld();

    if (!world.IsValid())
    {
        return false;
    }

    const Ray ray = activeViewport->GetCamera()->GetPickRay(relativePos);

    bool hasHit = false;
    float closestDistance = MathUtil::MaxSafeValue<float>();

    for (const Handle<Scene>& scene : world->GetScenes())
    {
        if (!scene.IsValid() || !(scene->GetSceneFlags() & SceneFlags::HAS_OCTREE))
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
            // bounding volume hits don't give a surface to project onto
            if (hit.isApproximate)
            {
                continue;
            }

            if (hit.distance < closestDistance)
            {
                closestDistance = hit.distance;

                outHit.scene = scene;
                outHit.position = hit.hitpoint;
                outHit.normal = hit.normal.LengthSquared() > 0.0001f ? hit.normal.Normalized() : -ray.direction;

                // normals come from triangle winding, face them back at the camera
                if (outHit.normal.Dot(ray.direction) > 0.0f)
                {
                    outHit.normal = -outHit.normal;
                }

                hasHit = true;
            }

            break;
        }
    }

    return hasHit;
}

Transform EditorDecalPainterState::MakePlacementTransform(const SurfaceHit& hit) const
{
    const Vec3f normal = m_alignToSurface ? hit.normal : Vec3f::UnitY();

    const Quat4f twist = Quat4f::AxisAngles(Vec3f::UnitY(), MathUtil::DegToRad(m_nextRotationDegrees));
    const Quat4f rotation = RotationFromUpTo(normal) * twist;

    Vec3f halfExtent = Vec3f(0.5f) * m_scale;

    if (m_activeDecal.IsValid())
    {
        halfExtent = m_activeDecal->GetDecalDesc().defaultSize * 0.5f * m_scale;
    }

    Transform transform;
    // Transform stores the inverse of the standard quaternion (see Mat4f::Rotation)
    transform.SetRotation(rotation.Inverse());
    transform.SetScale(halfExtent);
    transform.SetTranslation(hit.position);

    return transform;
}

bool EditorDecalPainterState::AcceptStrokeScene(const Handle<Scene>& scene)
{
    if (!scene.IsValid())
    {
        return false;
    }

    Handle<Scene> strokeScene = m_strokeScene.Lock();

    if (!strokeScene.IsValid())
    {
        m_strokeScene = scene;

        return true;
    }

    // one undo entry per stroke keeps to a single scene
    return strokeScene == scene;
}

EditorDecalPainterState::StrokeProxyEdit& EditorDecalPainterState::GetStrokeEdit(const Handle<DecalProxy>& proxy, bool createdByStroke)
{
    for (StrokeProxyEdit& edit : m_strokeEdits)
    {
        if (edit.proxy == proxy)
        {
            return edit;
        }
    }

    StrokeProxyEdit& edit = m_strokeEdits.EmplaceBack();
    edit.proxy = proxy;
    edit.createdByStroke = createdByStroke;

    return edit;
}

Handle<DecalProxy> EditorDecalPainterState::FindOrCreateDecalProxy(const Handle<Scene>& scene)
{
    AssertOnThread(g_simThread);

    Handle<DecalProxy> decalProxy;

    for (auto [existingProxy] : scene->GetEntityManager()->GetEntitySet<EntityType<DecalProxy>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
    {
        if (existingProxy->GetDecal() == m_activeDecal)
        {
            decalProxy = MakeStrongRef(existingProxy);

            break;
        }
    }

    if (decalProxy.IsValid())
    {
        return decalProxy;
    }

    decalProxy = MakeHandle<DecalProxy>(m_activeDecal);
    decalProxy->SetName(NAME_FMT("Decals_{}", m_activeDecal->GetName()));

    scene->GetRoot()->AddChild(decalProxy);

    GetStrokeEdit(decalProxy, /* createdByStroke */ true);

    return decalProxy;
}

void EditorDecalPainterState::StampAt(const SurfaceHit& hit)
{
    AssertOnThread(g_simThread);

    if (!m_activeDecal.IsValid() || !AcceptStrokeScene(hit.scene))
    {
        return;
    }

    Handle<DecalProxy> decalProxy = FindOrCreateDecalProxy(hit.scene);

    const DecalId id = decalProxy->AddDecal(MakePlacementTransform(hit));

    for (const DecalInstance& instance : decalProxy->GetInstances())
    {
        if (instance.id == id)
        {
            GetStrokeEdit(decalProxy).added.PushBack(instance);

            break;
        }
    }

    m_hasLastStamp = true;
    m_lastStampPosition = hit.position;

    if (m_randomRotation)
    {
        static uint32 s_seed = 0x9E3779B9u;

        m_nextRotationDegrees = MathUtil::RandomInRange(s_seed, 0.0f, 360.0f);
    }
}

void EditorDecalPainterState::EraseAt(const SurfaceHit& hit)
{
    AssertOnThread(g_simThread);

    if (!AcceptStrokeScene(hit.scene))
    {
        return;
    }

    Array<Handle<DecalProxy>> decalProxies;

    for (auto [decalProxy] : hit.scene->GetEntityManager()->GetEntitySet<EntityType<DecalProxy>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
    {
        decalProxies.PushBack(MakeStrongRef(decalProxy));
    }

    const BoundingSphere sphere(hit.position, m_eraseRadius);

    Array<DecalInstance, SceneAllocator> removed;

    for (const Handle<DecalProxy>& decalProxy : decalProxies)
    {
        removed.Resize(0); // clear, but don't free memory

        decalProxy->RemoveDecalsInSphere(sphere, removed);

        if (removed.Empty())
        {
            continue;
        }

        StrokeProxyEdit& edit = GetStrokeEdit(decalProxy);

        for (DecalInstance& instance : removed)
        {
            // erasing a decal placed earlier in this same stroke just cancels it out
            auto addedIt = edit.added.FindIf([&instance](const DecalInstance& added)
                {
                    return added.id == instance.id;
                });

            if (addedIt != edit.added.End())
            {
                edit.added.Erase(addedIt);

                continue;
            }

            edit.removed.PushBack(std::move(instance));
        }
    }
}

void EditorDecalPainterState::BeginStroke(const Vec2f& relativePos, bool erase)
{
    AssertOnThread(g_simThread);

    if (!m_enabled)
    {
        return;
    }

    if (!erase && !m_activeDecal.IsValid())
    {
        HYP_LOG(Editor, Warning, "Decal painter: pick a decal to paint with first");

        return;
    }

    m_isStroking = true;
    m_strokeErase = erase;
    m_hasLastStamp = false;
    m_strokeScene.Reset();
    m_strokeEdits.Clear();

    SurfaceHit hit;

    if (!TryGetSurfaceHit(relativePos, hit))
    {
        return;
    }

    m_hasHover = true;
    m_hover = hit;

    if (m_strokeErase)
    {
        EraseAt(hit);
    }
    else
    {
        StampAt(hit);
    }
}

void EditorDecalPainterState::UpdateStroke(const Vec2f& relativePos, bool erase)
{
    AssertOnThread(g_simThread);

    if (!m_isStroking)
    {
        return;
    }

    SurfaceHit hit;

    if (!TryGetSurfaceHit(relativePos, hit))
    {
        m_hasHover = false;

        return;
    }

    m_hasHover = true;
    m_hover = hit;

    if (m_strokeErase)
    {
        EraseAt(hit);

        return;
    }

    // spacing 0 = one stamp per click
    if (m_spacing <= 0.0f)
    {
        return;
    }

    if (m_hasLastStamp && m_lastStampPosition.Distance(hit.position) < m_spacing)
    {
        return;
    }

    StampAt(hit);
}

void EditorDecalPainterState::EndStroke()
{
    AssertOnThread(g_simThread);

    if (!m_isStroking)
    {
        return;
    }

    m_isStroking = false;

    const WeakHandle<Scene> strokeScene = m_strokeScene;
    m_strokeScene.Reset();

    Array<StrokeProxyEdit> edits = std::move(m_strokeEdits);
    m_strokeEdits.Clear();

    uint32 numAdded = 0;
    uint32 numRemoved = 0;

    for (const StrokeProxyEdit& edit : edits)
    {
        numAdded += uint32(edit.added.Size());
        numRemoved += uint32(edit.removed.Size());
    }

    if (numAdded == 0 && numRemoved == 0)
    {
        // a proxy created for stamps that got erased again within the same stroke
        for (const StrokeProxyEdit& edit : edits)
        {
            if (edit.createdByStroke && edit.proxy->NumDecals() == 0)
            {
                edit.proxy->Remove();
            }
        }

        return;
    }

    const Handle<EditorProject>& project = m_subsystem->GetCurrentProject();

    if (!project.IsValid())
    {
        return;
    }

    const String actionText = numAdded == 0
        ? HYP_FORMAT("Erase {} decal(s)", numRemoved)
        : HYP_FORMAT("Paint {} decal(s)", numAdded);

    // the stroke has already been applied; execute runs again on push so both sides are idempotent
    Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
        actionText,
        Proc<EditorActionFunctions()>(
            [strokeScene, edits]() -> EditorActionFunctions
            {
                return EditorActionFunctions {
                    .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                        [strokeScene, edits](EditorSubsystem*, EditorProject*)
                        {
                            Handle<Scene> scene = strokeScene.Lock();

                            if (!scene.IsValid())
                            {
                                return;
                            }

                            for (const StrokeProxyEdit& edit : edits)
                            {
                                if (edit.createdByStroke && edit.proxy->GetParent() == nullptr)
                                {
                                    scene->GetRoot()->AddChild(edit.proxy);
                                }

                                for (const DecalInstance& instance : edit.removed)
                                {
                                    edit.proxy->RemoveDecal(instance.id);
                                }

                                for (const DecalInstance& instance : edit.added)
                                {
                                    edit.proxy->AddDecalWithId(instance);
                                }
                            }
                        }),
                    .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                        [edits](EditorSubsystem*, EditorProject*)
                        {
                            for (const StrokeProxyEdit& edit : edits)
                            {
                                for (const DecalInstance& instance : edit.added)
                                {
                                    edit.proxy->RemoveDecal(instance.id);
                                }

                                for (const DecalInstance& instance : edit.removed)
                                {
                                    edit.proxy->AddDecalWithId(instance);
                                }

                                if (edit.createdByStroke)
                                {
                                    edit.proxy->Remove();
                                }
                            }
                        })
                };
            }));

    InitObject(action);

    project->GetActionStack()->PushAction(action);
}

void EditorDecalPainterState::Update()
{
    HYP_SCOPE;

    if (m_subsystem->IsSimulating())
    {
        EndStroke();

        return;
    }

    if (!m_enabled)
    {
        EndStroke();

        m_hasHover = false;
    }
}

void EditorDecalPainterState::UpdateHover(const Vec2f& relativePos)
{
    AssertOnThread(g_simThread);

    if (!m_enabled)
    {
        m_hasHover = false;

        return;
    }

    if (m_isStroking)
    {
        return;
    }

    SurfaceHit hit;

    m_hasHover = TryGetSurfaceHit(relativePos, hit);

    if (m_hasHover)
    {
        m_hover = hit;
    }
}

void EditorDecalPainterState::DebugDrawCursor(DebugDrawCommandList& debugDrawCommandList)
{
    static const RenderableAttributeSet attributes = DecalCursorDrawAttributes();

    if (s_cvDecalsDebugDraw.Get() && m_subsystem->GetCurrentProject().IsValid())
    {
        if (const Handle<World>& world = m_subsystem->GetCurrentProject()->GetWorld(); world.IsValid())
        {
            for (const Handle<Scene>& scene : world->GetScenes())
            {
                if (!scene.IsValid())
                {
                    continue;
                }

                for (auto [decalProxy] : scene->GetEntityManager()->GetEntitySet<EntityType<DecalProxy>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
                {
                    for (const DecalInstance& instance : decalProxy->GetInstances())
                    {
                        debugDrawCommandList.box(instance.transform, Color(0.9f, 0.4f, 1.0f, 1.0f), attributes);
                    }
                }
            }
        }
    }

    if (!m_enabled || !m_hasHover)
    {
        return;
    }

    const bool erasing = m_isStroking ? m_strokeErase : false;

    if (erasing)
    {
        debugDrawCommandList.sphere(m_hover.position, m_eraseRadius, Color(1.0f, 0.3f, 0.2f, 1.0f), attributes);

        return;
    }

    const Color color = m_isStroking
        ? Color(1.0f, 0.55f, 0.1f, 1.0f)
        : (m_activeDecal.IsValid() ? Color(0.35f, 0.8f, 1.0f, 1.0f) : Color(0.6f, 0.6f, 0.6f, 1.0f));

    debugDrawCommandList.box(MakePlacementTransform(m_hover), color, attributes);
}

#pragma endregion EditorDecalPainterState

} // namespace Hyperion
