/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <Editor/Terrain/EditorTerrainState.hpp>
#include <Editor/EditorSubsystem.hpp>
#include <Editor/EditorViewport.hpp>

#include <Scene/Scene.hpp>
#include <Scene/World.hpp>

#include <Scene/WorldGrid/WorldGrid.hpp>
#include <Scene/WorldGrid/Terrain/TerrainWorldGridLayer.hpp>

#include <Scene/Camera/Camera.hpp>

#include <Rendering/DebugDrawer.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Logging/Logger.hpp>

#include <EditorTerrainState.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

#pragma region EditorTerrainState

EditorTerrainState::EditorTerrainState() = default;

EditorTerrainState::~EditorTerrainState() = default;

void EditorTerrainState::Initialize(EditorSubsystem* subsystem)
{
    AssertDebug(subsystem != nullptr);

    m_subsystem = subsystem;
}

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

} // anonymous namespace

bool EditorTerrainState::IsEnabled() const
{
    return m_enabled;
}

void EditorTerrainState::SetEnabled(bool enabled)
{
    DispatchToSimThread([this, enabled]()
    {
        AssertOnThread(g_simThread);

        if (enabled && !CanEnterTerrainTools())
        {
            return;
        }

        if (!enabled && m_isStroking)
        {
            EndStroke();
        }

        m_enabled = enabled;
    });
}

float EditorTerrainState::GetRadius() const
{
    return m_radius;
}

void EditorTerrainState::SetRadius(float radius)
{
    DispatchToSimThread([this, radius]()
    {
        AssertOnThread(g_simThread);

        m_radius = MathUtil::Max(radius, 0.1f);
    });
}

float EditorTerrainState::GetStrength() const
{
    return m_strength;
}

void EditorTerrainState::SetStrength(float strength)
{
    DispatchToSimThread([this, strength]()
    {
        AssertOnThread(g_simThread);

        m_strength = strength;
    });
}

TerrainSculptMode EditorTerrainState::GetMode() const
{
    return m_mode;
}

void EditorTerrainState::SetMode(TerrainSculptMode mode)
{
    DispatchToSimThread([this, mode]()
    {
        AssertOnThread(g_simThread);

        if (mode != TerrainSculptMode::PaintSplat)
        {
            m_sculptDirection = mode;
        }

        m_mode = mode;
    });
}

bool EditorTerrainState::IsSculptActive() const
{
    return m_enabled && m_mode != TerrainSculptMode::PaintSplat;
}

bool EditorTerrainState::IsPaintActive() const
{
    return m_enabled && m_mode == TerrainSculptMode::PaintSplat;
}

void EditorTerrainState::ActivateSculpt()
{
    DispatchToSimThread([this]()
    {
        AssertOnThread(g_simThread);

        // toggle off if already active
        if (IsSculptActive())
        {
            SetEnabled(false);
            return;
        }

        if (!CanEnterTerrainTools())
        {
            return;
        }

        SetMode(m_sculptDirection);
        SetEnabled(true);
    });
}

void EditorTerrainState::ActivatePaint()
{
    DispatchToSimThread([this]()
    {
        AssertOnThread(g_simThread);

        // toggle off if already active
        if (IsPaintActive())
        {
            SetEnabled(false);
            return;
        }

        if (!CanEnterTerrainTools())
        {
            return;
        }

        SetMode(TerrainSculptMode::PaintSplat);
        SetEnabled(true);
    });
}

int EditorTerrainState::GetPaintLayer() const
{
    return int(m_paintLayer);
}

void EditorTerrainState::SetPaintLayer(int paintLayer)
{
    DispatchToSimThread([this, paintLayer]()
    {
        AssertOnThread(g_simThread);

        m_paintLayer = uint32(MathUtil::Clamp(paintLayer, 0, 3));
    });
}

bool EditorTerrainState::CanEnterTerrainTools() const
{
    AssertOnThread(g_simThread);

    // The tools edit the source world; simulation runs against a throwaway snapshot of it, so any
    // edits made while simulating would be discarded when it stops.
    if (m_subsystem->IsSimulating())
    {
        HYP_LOG(Editor, Warning, "Cannot use the terrain tools while simulation is active");

        return false;
    }

    return true;
}

bool EditorTerrainState::CanSculptTerrainForWorld(const Handle<World>& world) const
{
    AssertOnThread(g_simThread);

    if (!world.IsValid())
    {
        return false;
    }

    const Handle<WorldGrid>& worldGrid = world->GetWorldGrid();

    if (!worldGrid.IsValid())
    {
        return false;
    }

    for (const Handle<WorldGridLayer>& layer : worldGrid->GetLayers())
    {
        if (DynamicCast<TerrainWorldGridLayer>(layer).IsValid())
        {
            return true;
        }
    }

    return false;
}

bool EditorTerrainState::TryGetTerrainHit(const Vec2f& relativePos, Handle<TerrainWorldGridLayer>& outLayer, Vec3f& outWorldPos) const
{
    AssertOnThread(g_simThread);

    EditorViewport* activeViewport = m_subsystem->GetActiveViewport();

    if (!activeViewport)
    {
        return false;
    }

    Handle<Scene> activeScene = m_subsystem->GetActiveScene();

    if (!activeScene.IsValid() || !activeScene->GetWorld())
    {
        return false;
    }

    const Handle<WorldGrid>& worldGrid = activeScene->GetWorld()->GetWorldGrid();

    if (!worldGrid.IsValid())
    {
        return false;
    }

    const Ray ray = activeViewport->GetCamera()->GetPickRay(relativePos);

    for (const Handle<WorldGridLayer>& layer : worldGrid->GetLayers())
    {
        const Handle<TerrainWorldGridLayer>& terrainLayer = DynamicCast<TerrainWorldGridLayer>(layer);

        if (!terrainLayer.IsValid())
        {
            continue;
        }

        Vec3f hitPoint;

        if (terrainLayer->RaycastSurface(ray, hitPoint))
        {
            outLayer = terrainLayer;
            outWorldPos = hitPoint;

            return true;
        }
    }

    return false;
}

void EditorTerrainState::ApplyBrushAt(const Handle<TerrainWorldGridLayer>& layer, const Vec3f& worldPos, bool invert, float dt)
{
    AssertOnThread(g_simThread);

    const float brushStrength = m_strength * MathUtil::Clamp(dt, 0.0f, 0.1f);

    switch (m_mode)
    {
    case TerrainSculptMode::PaintSplat:

        layer->PaintSplat(
            worldPos,
            m_radius,
            brushStrength,
            m_paintLayer,
            /* erase */ invert);

        break;
    case TerrainSculptMode::Lower:
        layer->ApplyBrush(worldPos, m_radius, brushStrength, /* raise */ invert);
        break;
    case TerrainSculptMode::Raise:
    default:
        layer->ApplyBrush(worldPos, m_radius, brushStrength, /* raise */ !invert);
        break;
    }

    m_hasHover = true;
    m_hoverWorldPos = worldPos;
    m_hoveredLayer = layer;
}

bool EditorTerrainState::TryApplyAtScreenPos(const Vec2f& relativePos, bool invert, float dt)
{
    AssertOnThread(g_simThread);

    Handle<TerrainWorldGridLayer> layer;
    Vec3f worldPos;

    if (!TryGetTerrainHit(relativePos, layer, worldPos))
    {
        m_hoveredLayer.Reset();
        m_strokeLayer.Reset();

        return false;
    }

    m_strokeWorldPos = worldPos;
    m_strokeLayer = layer;
    m_strokeNeedsPick = false;

    ApplyBrushAt(layer, worldPos, invert, dt);

    return true;
}

bool EditorTerrainState::ApplyStroke(float dt)
{
    AssertOnThread(g_simThread);

    Handle<TerrainWorldGridLayer> layer = m_strokeLayer.Lock();

    if (m_strokeNeedsPick || !layer.IsValid())
    {
        return TryApplyAtScreenPos(m_strokeScreenPos, m_strokeInvert, dt);
    }

    Vec3f worldPos = m_strokeWorldPos;
    worldPos.y = layer->SampleDrawnHeightAt(Vec2f(worldPos.x, worldPos.z));

    ApplyBrushAt(layer, worldPos, m_strokeInvert, dt);

    return true;
}

void EditorTerrainState::BeginStroke(const Vec2f& relativePos, bool invert)
{
    AssertOnThread(g_simThread);

    if (!m_enabled)
    {
        return;
    }

    m_isStroking = true;
    m_strokeInvert = invert;
    m_strokeScreenPos = relativePos;
    m_strokeNeedsPick = true;

    m_strokeTimer.Reset();
    m_strokeTimer.NextTick();

    if (!TryApplyAtScreenPos(relativePos, invert, MathUtil::Max(m_strokeTimer.delta, 1.0f / 60.0f)))
    {
        m_hasHover = false;
    }
}

void EditorTerrainState::UpdateStroke(const Vec2f& relativePos, bool invert)
{
    AssertOnThread(g_simThread);

    if (!m_isStroking)
    {
        return;
    }

    m_strokeInvert = invert;

    if (relativePos != m_strokeScreenPos)
    {
        m_strokeScreenPos = relativePos;
        m_strokeNeedsPick = true;
    }
}

void EditorTerrainState::EndStroke()
{
    AssertOnThread(g_simThread);

    if (!m_isStroking)
    {
        return;
    }

    m_isStroking = false;
    m_strokeLayer.Reset();

    Handle<Scene> activeScene = m_subsystem->GetActiveScene();

    if (!activeScene.IsValid() || !activeScene->GetWorld())
    {
        return;
    }

    const Handle<WorldGrid>& worldGrid = activeScene->GetWorld()->GetWorldGrid();

    if (!worldGrid.IsValid())
    {
        return;
    }

    for (const Handle<WorldGridLayer>& layer : worldGrid->GetLayers())
    {
        if (Handle<TerrainWorldGridLayer> terrainLayer = DynamicCast<TerrainWorldGridLayer>(layer); terrainLayer.IsValid())
        {
            terrainLayer->EndBrushStroke();
        }
    }
}

void EditorTerrainState::Update()
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

        return;
    }

    if (!m_isStroking)
    {
        return;
    }

    m_strokeTimer.NextTick();

    if (!ApplyStroke(m_strokeTimer.delta))
    {
        m_hasHover = false;
    }
}

void EditorTerrainState::UpdateHover(const Vec2f& relativePos)
{
    AssertOnThread(g_simThread);

    if (!m_enabled)
    {
        m_hasHover = false;

        return;
    }

    if (m_isStroking)
    {
        // the stroke places the cursor - a second pick here would put it back on the ray the stroke is anchored against
        return;
    }

    Handle<TerrainWorldGridLayer> layer;
    Vec3f worldPos;

    if (TryGetTerrainHit(relativePos, layer, worldPos))
    {
        m_hasHover = true;
        m_hoverWorldPos = worldPos;
        m_hoveredLayer = layer;
    }
    else
    {
        m_hasHover = false;
        m_hoveredLayer.Reset();
    }
}

static RenderableAttributeSet TerrainCursorDrawAttributes()
{
    RenderableAttributeSet attributes;

    MeshAttributes& meshAttributes = attributes.GetMeshAttributes();
    meshAttributes.inputLayout = StaticVertexInputLayout<VT_Simple>;
    meshAttributes.topology = Topology::Triangles;

    MaterialAttributes& materialAttributes = attributes.GetMaterialAttributes();
    materialAttributes.bucket = RenderBucket::Debug;
    materialAttributes.fillMode = FillMode::Fill;
    materialAttributes.blendFunction = BlendFunction::None();
    materialAttributes.cullFaces = FaceCullMode::None;
    materialAttributes.flags = MAF_DEPTH_TEST;

    return attributes;
}

static Vec3f ProjectOntoTerrain(const Handle<TerrainWorldGridLayer>& layer, const Vec2f& worldXZ)
{
    return Vec3f(worldXZ.x, layer->SampleDrawnHeightAt(worldXZ), worldXZ.y);
}

static Vec3f TerrainSurfaceNormal(const Vec2f& gradient)
{
    return Vec3f(-gradient.x, 1.0f, -gradient.y).Normalized();
}

static void DrawTerrainRibbon(
    DebugDrawCommandList& debugDrawCommandList,
    const RenderableAttributeSet& attributes,
    Span<const Vec3f> edgeA,
    Span<const Vec3f> edgeB,
    bool closed,
    const Color& color)
{
    const uint32 pointCount = uint32(MathUtil::Min(edgeA.Size(), edgeB.Size()));

    if (pointCount < 2)
    {
        return;
    }

    const uint32 segmentCount = closed ? pointCount : pointCount - 1;

    for (uint32 i = 0; i < segmentCount; i++)
    {
        const uint32 j = (i + 1) % pointCount;

        debugDrawCommandList.triangle(edgeA[i], edgeB[i], edgeB[j], color, attributes);
        debugDrawCommandList.triangle(edgeA[i], edgeB[j], edgeA[j], color, attributes);
    }
}

void EditorTerrainState::DebugDrawCursor(DebugDrawCommandList& debugDrawCommandList)
{
    if (!m_enabled || !m_hasHover)
    {
        return;
    }

    static constexpr uint32 GridLinesPerSide = 3;

    Color baseColor;
    switch (m_mode)
    {
    case TerrainSculptMode::Lower:
        baseColor = Color(0.35f, 0.8f, 1.0f, 1.0f);
        break;
    case TerrainSculptMode::PaintSplat:
        baseColor = Color(0.6f, 1.0f, 0.35f, 1.0f);
        break;
    case TerrainSculptMode::Raise:
    default:
        baseColor = Color(1.0f, 0.9f, 0.2f, 1.0f);
        break;
    }

    const Color color = m_isStroking
        ? Color(1.0f, 0.55f, 0.1f, 1.0f)
        : baseColor;
    const Color gridColor = color * Color(0.35f, 0.35f, 0.35f, 1.0f);

    const Handle<TerrainWorldGridLayer> layer = m_hoveredLayer.Lock();
    if (!layer.IsValid())
    {
        return;
    }

    const RenderableAttributeSet attributes = TerrainCursorDrawAttributes();

    const Vec2f centerXZ(m_hoverWorldPos.x, m_hoverWorldPos.z);
    const float radius = m_radius;

    const float ribbonWidth = MathUtil::Clamp(radius * 0.035f, 0.06f, 0.3f);

    // a big brush is usually looked at from farther away, where a fixed lift is below the depth buffer's precision
    // and the ribbon starts breaking up against the terrain
    const float surfaceOffset = MathUtil::Max(radius * 0.01f, 0.1f);

    // step along the terrain's own vertex spacing: a coarser step chords across hills and dips into valleys, which is
    // what makes the cursor look like it is floating rather than lying on the surface
    const Vec3f& layerScale = layer->GetLayerInfo().scale;
    const float sampleSpacing = MathUtil::Max(MathUtil::Min(layerScale.x, layerScale.z) * 0.5f, 0.05f);

    const uint32 ringSegments = MathUtil::Clamp(
        uint32(MathUtil::Ceil((2.0f * MathUtil::pi<float> * radius) / sampleSpacing)),
        48u, 256u);

    Array<Vec3f> ringInner;
    Array<Vec3f> ringOuter;
    Array<Vec2f> ringDirections;

    ringInner.Resize(ringSegments);
    ringOuter.Resize(ringSegments);
    ringDirections.Resize(ringSegments);

    for (uint32 i = 0; i < ringSegments; i++)
    {
        const float angle = 2.0f * MathUtil::pi<float> * (float(i) / float(ringSegments));
        const Vec2f dir(MathUtil::Cos(angle), MathUtil::Sin(angle));

        ringDirections[i] = dir;
        ringInner[i] = ProjectOntoTerrain(layer, centerXZ + dir * (radius - ribbonWidth * 0.5f));
        ringOuter[i] = ProjectOntoTerrain(layer, centerXZ + dir * (radius + ribbonWidth * 0.5f));
    }

    for (uint32 i = 0; i < ringSegments; i++)
    {
        const Vec2f& dir = ringDirections[i];
        const Vec2f tangent(-dir.y, dir.x);

        const uint32 prev = (i + ringSegments - 1) % ringSegments;
        const uint32 next = (i + 1) % ringSegments;

        const float radialGradient = (ringOuter[i].y - ringInner[i].y) / MathUtil::Max(ribbonWidth, MathUtil::epsilonF);

        const Vec2f deltaXZ(ringInner[next].x - ringInner[prev].x, ringInner[next].z - ringInner[prev].z);
        const float tangentDistance = MathUtil::Max(deltaXZ.Length() * 0.5f, MathUtil::epsilonF);
        const float tangentialGradient = (ringInner[next].y - ringInner[prev].y) / tangentDistance;

        const Vec3f normal = TerrainSurfaceNormal(dir * radialGradient + tangent * tangentialGradient);

        ringInner[i] += normal * surfaceOffset;
        ringOuter[i] += normal * surfaceOffset;
    }

    DrawTerrainRibbon(
        debugDrawCommandList,
        attributes,
        Span<const Vec3f>(ringInner.Data(), ringInner.Size()),
        Span<const Vec3f>(ringOuter.Data(), ringOuter.Size()),
        /* closed */ true,
        color);

    const float gridSpacing = radius / float(GridLinesPerSide + 1);

    for (uint32 axis = 0; axis < 2; axis++)
    {
        const Vec2f lineDir = (axis == 0)
            ? Vec2f(0.0f, 1.0f)
            : Vec2f(1.0f, 0.0f);

        const Vec2f linePerp = (axis == 0)
            ? Vec2f(1.0f, 0.0f)
            : Vec2f(0.0f, 1.0f);

        for (int32 lineIndex = -int32(GridLinesPerSide); lineIndex <= int32(GridLinesPerSide); lineIndex++)
        {
            const float offset = float(lineIndex) * gridSpacing;
            const float chordHalfLength = MathUtil::Sqrt(MathUtil::Max(radius * radius - offset * offset, 0.0f)) * 0.96f;

            const bool isCenterLine = lineIndex == 0;
            const float lineWidth = ribbonWidth * (isCenterLine ? 0.65f : 0.45f);
            const Color& lineColor = isCenterLine ? color : gridColor;

            const uint32 linePointCount = MathUtil::Clamp(
                uint32(MathUtil::Ceil((chordHalfLength * 2.0f) / sampleSpacing)),
                6u, 96u) + 1;

            Array<Vec3f> edgeLeft;
            Array<Vec3f> edgeRight;

            edgeLeft.Resize(linePointCount);
            edgeRight.Resize(linePointCount);

            for (uint32 i = 0; i < linePointCount; i++)
            {
                const float t = -1.0f + 2.0f * (float(i) / float(linePointCount - 1));
                const Vec2f linePoint = centerXZ + linePerp * offset + lineDir * (t * chordHalfLength);

                edgeLeft[i] = ProjectOntoTerrain(layer, linePoint - linePerp * (lineWidth * 0.5f));
                edgeRight[i] = ProjectOntoTerrain(layer, linePoint + linePerp * (lineWidth * 0.5f));
            }

            for (uint32 i = 0; i < linePointCount; i++)
            {
                const uint32 prev = (i > 0) ? i - 1 : 0;
                const uint32 next = (i < linePointCount - 1) ? i + 1 : linePointCount - 1;

                const Vec2f deltaXZ(edgeLeft[next].x - edgeLeft[prev].x, edgeLeft[next].z - edgeLeft[prev].z);
                const float distance = MathUtil::Max(deltaXZ.Length(), MathUtil::epsilonF);

                const float centerPrev = (edgeLeft[prev].y + edgeRight[prev].y) * 0.5f;
                const float centerNext = (edgeLeft[next].y + edgeRight[next].y) * 0.5f;

                const Vec3f normal = TerrainSurfaceNormal(lineDir * ((centerNext - centerPrev) / distance));

                edgeLeft[i] += normal * surfaceOffset;
                edgeRight[i] += normal * surfaceOffset;
            }

            DrawTerrainRibbon(
                debugDrawCommandList,
                attributes,
                Span<const Vec3f>(edgeLeft.Data(), edgeLeft.Size()),
                Span<const Vec3f>(edgeRight.Data(), edgeRight.Size()),
                /* closed */ false,
                lineColor);
        }
    }
}

#pragma endregion EditorTerrainState

} // namespace Hyperion
