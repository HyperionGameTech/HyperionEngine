/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <Editor/Decal/EditorDecalPainterState.hpp>
#include <Editor/EditorSubsystem.hpp>
#include <Editor/EditorProject.hpp>

#include <Scene/Scene.hpp>
#include <Scene/World.hpp>
#include <Scene/Decal/Decal.hpp>
#include <Scene/Decal/DecalProxy.hpp>
#include <Scene/EntityManager.hpp>

#include <Asset/AssetRegistry.hpp>

#include <Rendering/DebugDrawer.hpp>

#include <Framework/CVarManager.hpp>

#include <Core/Math/BoundingSphere.hpp>

#include <Core/Logging/Logger.hpp>

#include <EditorDecalPainterState.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

static CVar<bool> s_cvDecalsDebugDraw { "Rendering.Decals.DebugDraw", false };

#pragma region EditorDecalPainterState

EditorDecalPainterState::EditorDecalPainterState() = default;

EditorDecalPainterState::~EditorDecalPainterState() = default;

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

bool EditorDecalPainterState::HasActiveAsset() const
{
    return m_activeDecal.IsValid();
}

Vec3f EditorDecalPainterState::GetPlacementScale() const
{
    if (m_activeDecal.IsValid())
    {
        return m_activeDecal->GetDecalDesc().defaultSize * 0.5f * m_scale;
    }

    return Vec3f(0.5f) * m_scale;
}

Handle<DecalProxy> EditorDecalPainterState::FindOrCreateDecalProxy(const Handle<Scene>& scene)
{
    AssertOnThread(g_simThread);

    for (auto [existingProxy] : scene->GetEntityManager()->GetEntitySet<EntityType<DecalProxy>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
    {
        if (existingProxy->GetDecal() == m_activeDecal)
        {
            return MakeStrongRef(existingProxy);
        }
    }

    Handle<DecalProxy> decalProxy = MakeHandle<DecalProxy>(m_activeDecal);
    decalProxy->SetName(NAME_FMT("Decals_{}", m_activeDecal->GetName()));

    scene->GetRoot()->AddChild(decalProxy);

    m_stroke.GetEdit(decalProxy, /* createdByStroke */ true);

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
            m_stroke.RecordAdded(decalProxy, instance);

            break;
        }
    }

    OnStamped(hit);
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

        for (const DecalInstance& instance : removed)
        {
            m_stroke.RecordRemoved(decalProxy, instance);
        }
    }
}

void EditorDecalPainterState::ResetStrokeEdits()
{
    m_stroke.Reset();
}

void EditorDecalPainterState::CommitStrokeEdits(const WeakHandle<Scene>& strokeScene)
{
    m_stroke.Commit(m_subsystem, strokeScene);
}

void EditorDecalPainterState::DebugDrawCursor(DebugDrawCommandList& debugDrawCommandList)
{
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
                        debugDrawCommandList.box(instance.transform, Color(0.9f, 0.4f, 1.0f, 1.0f), GetCursorDrawAttributes());
                    }
                }
            }
        }
    }

    EditorSurfacePainterState::DebugDrawCursor(debugDrawCommandList);
}

#pragma endregion EditorDecalPainterState

} // namespace Hyperion
