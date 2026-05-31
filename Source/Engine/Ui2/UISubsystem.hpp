/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/Subsystem.hpp>

#include <Core/reflection/ObjectBase.hpp>
#include <Core/reflection/Handle.hpp>

#include <Core/functional/Delegate.hpp>

#include <Rendering/RenderTypes.hpp>
#include <Rendering/DrawCall.hpp>

namespace Hyperion {

class Scene;
class View;
class World;

class UIStage;
class UIObject;
class UIPass;
class UIListView;
class FontAtlas;

class RenderProxyList;

class OverlayBase;

HYP_STRUCT(NoScriptBindings)
struct UIEntityInstanceBatch : EntityInstanceBatch
{
    HYP_STRUCT_BODY(UIEntityInstanceBatch);

    HYP_FIELD()
    FixedArray<Vec4f, MaxEntitiesPerBatch> texcoords;

    HYP_FIELD()
    FixedArray<Vec4f, MaxEntitiesPerBatch> offsets;

    HYP_FIELD()
    FixedArray<Vec4f, MaxEntitiesPerBatch> sizes;

    HYP_FIELD()
    FixedArray<Vec4u, MaxEntitiesPerBatch> properties;
};

static_assert(sizeof(UIEntityInstanceBatch) % 64 == 0);

HYP_CLASS()
class ENGINE_API UISubsystem final : public Subsystem
{
    HYP_OBJECT_BODY(UISubsystem);

public:
    UISubsystem();
    explicit UISubsystem(const Handle<UIStage>& uiStage);

    UISubsystem(const UISubsystem& other) = delete;
    UISubsystem& operator=(const UISubsystem& other) = delete;

    ~UISubsystem() override;

    HYP_FORCE_INLINE const Handle<UIStage>& GetUIStage() const
    {
        return m_uiStage;
    }

    HYP_METHOD()
    void AddDebugOverlay(const Handle<OverlayBase>& debugOverlay);

    HYP_METHOD()
    bool RemoveDebugOverlay(OverlayBase* debugOverlay);

    void PreUpdate(float delta) override;
    void Update(float delta) override;

protected:
    SubsystemUpdatePhase GetUpdatePhase_Internal() const override
    {
        return SubsystemUpdatePhase::AfterVis;
    }

private:
    void Init() override;

    void OnAddedToWorld() override;
    void OnRemovedFromWorld() override;

    void RenderCollect(RenderProxyList& rpl);

    void InitFont();

    void InitDebugOverlays();
    void UpdateDebugOverlays();

    Handle<UIStage> m_uiStage;

    Handle<View> m_view;

    UIPass* m_uiRenderer;

    Array<Handle<OverlayBase>> m_debugOverlays;

    // top-left, bottom-left, top-right, bottom-right
    FixedArray<Handle<UIObject>, 4> m_debugOverlayContainers;

    DelegateHandler m_onWindowResizedHandle;
    DelegateHandler m_onCurrentWindowChangedHandle;

    bool m_wasProcessedLastFrame;
};

} // namespace Hyperion
