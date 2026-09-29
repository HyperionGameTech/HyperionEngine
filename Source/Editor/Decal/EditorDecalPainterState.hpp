/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Editor/Painting/EditorSurfacePainterState.hpp>
#include <Editor/Painting/SurfacePainterStroke.hpp>

#include <Core/Name/Name.hpp>

#include <Scene/Decal/DecalTypes.hpp>
#include <Scene/Decal/DecalProxy.hpp>

namespace Hyperion {

class Decal;

struct DecalStrokeTraits
{
    using Target = DecalProxy;
    using Record = DecalInstance;

    static constexpr const char* Noun = "decal";

    static DecalId GetId(const DecalInstance& instance)
    {
        return instance.id;
    }

    static void Add(DecalProxy& proxy, const DecalInstance& instance)
    {
        proxy.AddDecalWithId(instance);
    }

    static void Remove(DecalProxy& proxy, const DecalInstance& instance)
    {
        proxy.RemoveDecal(instance.id);
    }

    static bool IsEmpty(const DecalProxy& proxy)
    {
        return proxy.NumDecals() == 0;
    }
};

HYP_CLASS(Serialize = false)
class EDITOR_API EditorDecalPainterState final : public EditorSurfacePainterState
{
    HYP_OBJECT_BODY(EditorDecalPainterState);

public:
    EditorDecalPainterState();
    ~EditorDecalPainterState() override;

    HYP_METHOD(Property = "ActiveDecal", Editor)
    const Handle<Decal>& GetActiveDecal() const;

    HYP_METHOD(Property = "ActiveDecal", Editor)
    void SetActiveDecal(const Handle<Decal>& decal);

    HYP_METHOD()
    void SetActiveDecalByName(Name assetName);

    void DebugDrawCursor(DebugDrawCommandList& debugDrawCommandList) override;

protected:
    const char* GetToolName() const override
    {
        return "decal painter";
    }

    const char* GetAssetTypeName() const override
    {
        return "decal";
    }

    bool HasActiveAsset() const override;
    Vec3f GetPlacementScale() const override;

    void StampAt(const SurfaceHit& hit) override;
    void EraseAt(const SurfaceHit& hit) override;

    void ResetStrokeEdits() override;
    void CommitStrokeEdits(const WeakHandle<Scene>& strokeScene) override;

private:
    Handle<DecalProxy> FindOrCreateDecalProxy(const Handle<Scene>& scene);

    Handle<Decal> m_activeDecal;

    SurfacePainterStroke<DecalStrokeTraits> m_stroke;
};

} // namespace Hyperion
