/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <Editor/Painting/EditorSurfacePainterState.hpp>
#include <Editor/Terrain/EditorTerrainState.hpp>
#include <Editor/EditorSubsystem.hpp>
#include <Editor/EditorViewport.hpp>
#include <Editor/EditorProject.hpp>

#include <Scene/Scene.hpp>
#include <Scene/World.hpp>
#include <Scene/WorldGrid/WorldGrid.hpp>
#include <Scene/WorldGrid/Terrain/TerrainWorldGridLayer.hpp>

#include <Scene/Camera/Camera.hpp>

#include <Rendering/DebugDrawer.hpp>

#include <Core/Math/MathUtil.hpp>
#include <Core/Math/Quat4f.hpp>
#include <Core/Math/Ray.hpp>

#include <Core/Logging/Logger.hpp>

#include <EditorSurfacePainterState.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

namespace {

constexpr float PaintAreaRayHeight = 10000.0f;

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

RenderableAttributeSet CursorDrawAttributes()
{
    RenderableAttributeSet attributes;

    MeshAttributes& meshAttributes = attributes.GetMeshAttributes();
    meshAttributes.inputLayout = StaticVertexInputLayout<VT_Simple>;
    meshAttributes.topology = Topology::Triangles;

    MaterialAttributes& materialAttributes = attributes.GetMaterialAttributes();
    materialAttributes.bucket = RenderBucket::Debug;
    materialAttributes.fillMode = FillMode::Line;
    materialAttributes.blendFunction = BlendFunction::None();
    materialAttributes.flags = MaterialAttributeFlags::DepthTest;

    return attributes;
}

} // anonymous namespace

#pragma region EditorSurfacePainterState

EditorSurfacePainterState::EditorSurfacePainterState() = default;
EditorSurfacePainterState::~EditorSurfacePainterState() = default;

const RenderableAttributeSet& EditorSurfacePainterState::GetCursorDrawAttributes()
{
    static const RenderableAttributeSet s_attributes = CursorDrawAttributes();

    return s_attributes;
}

void EditorSurfacePainterState::DispatchToSimThread(Proc<void()>&& proc)
{
    if (IsOnThread(g_simThread))
    {
        proc();
    }
    else
    {
        GetThreadById(g_simThread)->GetScheduler().Enqueue(std::move(proc), TaskEnqueueFlags::FIRE_AND_FORGET);
    }
}

void EditorSurfacePainterState::Initialize(EditorSubsystem* subsystem)
{
    AssertDebug(subsystem != nullptr);

    m_subsystem = subsystem;
}

bool EditorSurfacePainterState::IsEnabled() const
{
    return m_enabled;
}

void EditorSurfacePainterState::SetEnabled(bool enabled)
{
    DispatchToSimThread([this, enabled]()
        {
            AssertOnThread(g_simThread);

            if (enabled && !CanEnterTool())
            {
                return;
            }

            if (!enabled && m_isStroking)
            {
                EndStroke();
            }

            // the terrain brush and the painters all own left-drag in the viewport
            if (enabled)
            {
                m_subsystem->GetTerrainState()->SetEnabled(false);
                m_subsystem->DisableSurfacePainters(/* except */ this);
                m_subsystem->ExitExtensionTools();
            }

            m_enabled = enabled;

            if (!m_enabled)
            {
                m_hasHover = false;
            }
        });
}

void EditorSurfacePainterState::Toggle()
{
    DispatchToSimThread([this]()
        {
            SetEnabled(!m_enabled);
        });
}

bool EditorSurfacePainterState::CanEnterTool() const
{
    AssertOnThread(g_simThread);

    return !m_subsystem->IsSimulating() && !m_subsystem->IsInIsolatedEditMode();
}

float EditorSurfacePainterState::GetScale() const
{
    return m_scale;
}

void EditorSurfacePainterState::SetScale(float scale)
{
    DispatchToSimThread([this, scale]()
        {
            m_scale = MathUtil::Max(scale, 0.01f);
        });
}

float EditorSurfacePainterState::GetRotationDegrees() const
{
    return m_rotationDegrees;
}

void EditorSurfacePainterState::SetRotationDegrees(float rotationDegrees)
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

bool EditorSurfacePainterState::GetRandomRotation() const
{
    return m_randomRotation;
}

void EditorSurfacePainterState::SetRandomRotation(bool randomRotation)
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

float EditorSurfacePainterState::GetSpacing() const
{
    return m_spacing;
}

void EditorSurfacePainterState::SetSpacing(float spacing)
{
    DispatchToSimThread([this, spacing]()
        {
            m_spacing = MathUtil::Max(spacing, 0.0f);
        });
}

float EditorSurfacePainterState::GetEraseRadius() const
{
    return m_eraseRadius;
}

void EditorSurfacePainterState::SetEraseRadius(float eraseRadius)
{
    DispatchToSimThread([this, eraseRadius]()
        {
            m_eraseRadius = MathUtil::Max(eraseRadius, 0.05f);
        });
}

bool EditorSurfacePainterState::GetAlignToSurface() const
{
    return m_alignToSurface;
}

void EditorSurfacePainterState::SetAlignToSurface(bool alignToSurface)
{
    DispatchToSimThread([this, alignToSurface]()
        {
            m_alignToSurface = alignToSurface;
        });
}

bool EditorSurfacePainterState::GetEraseMode() const
{
    return m_eraseMode;
}

void EditorSurfacePainterState::SetEraseMode(bool eraseMode)
{
    DispatchToSimThread([this, eraseMode]()
        {
            if (m_isStroking)
            {
                EndStroke();
            }

            m_eraseMode = eraseMode;
        });
}

bool EditorSurfacePainterState::TryGetSurfaceHit(const Vec2f& relativePos, SurfaceHit& outHit) const
{
    AssertOnThread(g_simThread);

    EditorViewport* activeViewport = m_subsystem->GetActiveViewport();

    if (!activeViewport)
    {
        return false;
    }

    if (!RaycastSurface(activeViewport->GetCamera()->GetPickRay(relativePos), outHit))
    {
        return false;
    }

    return RedirectFromTransientScene(outHit);
}

bool EditorSurfacePainterState::RedirectFromTransientScene(SurfaceHit& hit) const
{
    // generated scenes (e.g. the terrain's) are never saved, so stamps on them go in the active scene instead
    if (hit.scene->IsTransient())
    {
        hit.scene = m_subsystem->GetActiveScene();

        if (!hit.scene.IsValid() || hit.scene->IsTransient() || !hit.scene->GetRoot().IsValid())
        {
            HYP_LOG_ONCE(Editor, Warning, "The {} needs an active scene to paint into", GetToolName());

            return false;
        }
    }

    return true;
}

bool EditorSurfacePainterState::RaycastSurface(const Ray& ray, SurfaceHit& outHit) const
{
    AssertOnThread(g_simThread);

    if (!m_subsystem->GetCurrentProject().IsValid())
    {
        return false;
    }

    const Handle<World>& world = m_subsystem->GetCurrentProject()->GetWorld();

    if (!world.IsValid())
    {
        return false;
    }

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
            if (hit.isApproximate || ShouldIgnoreHit(hit))
            {
                continue;
            }

            if (hit.distance < closestDistance)
            {
                closestDistance = hit.distance;

                outHit.scene = scene;
                outHit.position = hit.hitpoint;
                outHit.normal = hit.normal.LengthSquared() > 0.0001f ? hit.normal.Normalized() : -ray.direction;
                outHit.isGround = false;

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

    // terrain does not use bvh, so we test the heights
    if (const Handle<WorldGrid>& worldGrid = world->GetWorldGrid(); worldGrid.IsValid())
    {
        for (const Handle<WorldGridLayer>& layer : worldGrid->GetLayers())
        {
            const Handle<TerrainWorldGridLayer>& terrainLayer = TryCast<TerrainWorldGridLayer>(layer);

            if (!terrainLayer.IsValid() || !terrainLayer->GetScene().IsValid())
            {
                continue;
            }

            Vec3f hitPoint;
            if (!terrainLayer->RaycastSurface(ray, hitPoint))
            {
                continue;
            }

            const float distance = hitPoint.Distance(ray.position);

            if (distance >= closestDistance)
            {
                continue;
            }

            closestDistance = distance;

            // the drawn surface's slope, a sample either side
            const Vec3f& scale = terrainLayer->GetLayerInfo().scale;
            const float step = MathUtil::Max(scale.x, scale.z, 0.01f);
            const Vec2f hitPointXY = hitPoint.GetXY();

            const float left = terrainLayer->SampleDrawnHeightAt(hitPointXY - Vec2f(step, 0.0f));
            const float right = terrainLayer->SampleDrawnHeightAt(hitPointXY + Vec2f(step, 0.0f));
            const float back = terrainLayer->SampleDrawnHeightAt(hitPointXY - Vec2f(0.0f, step));
            const float front = terrainLayer->SampleDrawnHeightAt(hitPointXY + Vec2f(0.0f, step));

            outHit.scene = terrainLayer->GetScene();
            outHit.position = hitPoint;
            outHit.normal = Vec3f { left - right, 2.0f * step, back - front }.Normalized();
            outHit.isGround = true;

            hasHit = true;
        }
    }

    return hasHit;
}

Transform EditorSurfacePainterState::MakePlacementTransform(const SurfaceHit& hit) const
{
    const Vec3f normal = m_alignToSurface ? hit.normal : Vec3f::UnitY();

    const Quat4f twist = Quat4f::AxisAngles(Vec3f::UnitY(), MathUtil::DegToRad(m_nextRotationDegrees));
    const Quat4f rotation = RotationFromUpTo(normal) * twist;

    Transform transform;
    transform.SetRotation(rotation);
    transform.SetScale(GetPlacementScale());
    transform.SetTranslation(GetPlacementPosition(hit));

    return transform;
}

Transform EditorSurfacePainterState::GetCursorTransform(const SurfaceHit& hit) const
{
    return MakePlacementTransform(hit);
}

bool EditorSurfacePainterState::AcceptStrokeScene(const Handle<Scene>& scene)
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

    return (strokeScene == scene);
}

void EditorSurfacePainterState::OnStamped(const SurfaceHit& hit)
{
    m_stampCount++;

    m_hasLastStamp = true;
    m_lastStampPosition = hit.position;

    if (m_randomRotation)
    {
        m_nextRotationDegrees = MathUtil::RandomInRange(m_rotationSeed, 0.0f, 360.0f);
    }
}

float EditorSurfacePainterState::RandomInRange(float min, float max)
{
    return MathUtil::RandomInRange(m_rotationSeed, min, max);
}

uint32 EditorSurfacePainterState::PaintArea(
    const Vec3f& center,
    float radius,
    uint32 count,
    bool erase)
{
    AssertOnThread(g_simThread);

    if (m_isStroking || count == 0)
    {
        return 0;
    }

    if (!erase && !HasActiveAsset())
    {
        HYP_LOG(Editor, Warning, "Pick a {} to paint with first", GetAssetTypeName());

        return 0;
    }

    m_isStroking = true;
    m_strokeErase = erase;
    m_hasLastStamp = false;
    m_strokeScene.Reset();

    ResetStrokeEdits();

    const uint32 stampCountBefore = m_stampCount;
    uint32 erasedAt = 0;

    Array<Vec3f> stamped;

    for (uint32 index = 0; index < count; index++)
    {
        Vec3f point = center;

        if (radius > 0.0f)
        {
            const float angle = RandomInRange(0.0f, 2.0f * MathUtil::pi<float>);
            const float distance = radius * MathUtil::Sqrt(RandomInRange(0.0f, 1.0f));

            point.x += MathUtil::Cos(angle) * distance;
            point.z += MathUtil::Sin(angle) * distance;
        }

        Ray ray;
        ray.position = point + Vec3f(0.0f, PaintAreaRayHeight, 0.0f);
        ray.direction = -Vec3f::UnitY();

        SurfaceHit hit;

        if (!RaycastSurface(ray, hit) || !RedirectFromTransientScene(hit))
        {
            continue;
        }

        if (erase)
        {
            EraseAt(hit);
            erasedAt++;

            continue;
        }

        bool crowded = false;

        for (const Vec3f& previous : stamped)
        {
            if (previous.Distance(hit.position) < m_spacing)
            {
                crowded = true;

                break;
            }
        }

        if (crowded)
        {
            continue;
        }

        const uint32 before = m_stampCount;

        PaintAt(hit);

        if (m_stampCount != before)
        {
            stamped.PushBack(hit.position);
        }
    }

    EndStroke();

    if (erase)
    {
        return erasedAt;
    }

    return (m_stampCount - stampCountBefore);
}

void EditorSurfacePainterState::StampAt(const Vec2f& relativePos)
{
    AssertOnThread(g_simThread);

    if (m_isStroking)
    {
        return;
    }

    BeginStroke(relativePos);
    EndStroke();
}

void EditorSurfacePainterState::BeginStroke(const Vec2f& relativePos)
{
    AssertOnThread(g_simThread);

    if (!m_enabled)
    {
        return;
    }

    const bool erase = m_eraseMode;

    if (!erase && !HasActiveAsset())
    {
        HYP_LOG(Editor, Warning, "Pick a {} to paint with first", GetAssetTypeName());

        return;
    }

    m_isStroking = true;
    m_strokeErase = erase;
    m_hasLastStamp = false;
    m_strokeScene.Reset();

    ResetStrokeEdits();

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
        PaintAt(hit);
    }
}

void EditorSurfacePainterState::UpdateStroke(const Vec2f& relativePos)
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

    // spacing 0 = one stamp per stroke
    if (m_spacing <= 0.0f)
    {
        return;
    }

    if (m_hasLastStamp && m_lastStampPosition.Distance(hit.position) < m_spacing)
    {
        return;
    }

    PaintAt(hit);
}

void EditorSurfacePainterState::EndStroke()
{
    AssertOnThread(g_simThread);

    if (!m_isStroking)
    {
        return;
    }

    m_isStroking = false;

    const WeakHandle<Scene> strokeScene = m_strokeScene;
    m_strokeScene.Reset();

    CommitStrokeEdits(strokeScene);
}

void EditorSurfacePainterState::Update()
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

void EditorSurfacePainterState::UpdateHover(const Vec2f& relativePos)
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

void EditorSurfacePainterState::DebugDrawCursor(DebugDrawCommandList& debugDrawCommandList)
{
    const RenderableAttributeSet& attributes = GetCursorDrawAttributes();

    if (!m_enabled || !m_hasHover)
    {
        return;
    }

    // Erasing
    if (m_isStroking ? m_strokeErase : m_eraseMode)
    {
        debugDrawCommandList.sphere(m_hover.position, m_eraseRadius, Color(1.0f, 0.3f, 0.2f, 1.0f), attributes);

        return;
    }

    Color color = Color(0.6f, 0.6f, 0.6f, 1.0f);

    if (CanPaintSurface(m_hover))
    {
        if (m_isStroking)
        {
            color = Color(1.0f, 0.55f, 0.1f, 1.0f);
        }
        else if (HasActiveAsset())
        {
            color = Color(0.35f, 0.8f, 1.0f, 1.0f);
        }
    }

    debugDrawCommandList.box(GetCursorTransform(m_hover), color, attributes);
}

#pragma endregion EditorSurfacePainterState

} // namespace Hyperion
