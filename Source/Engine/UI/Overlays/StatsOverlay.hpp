/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <UI/Overlays/Overlay.hpp>

#include <Core/Math/Color.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/Map.hpp>

#include <Core/Utilities/Uuid.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

class World;
class UIListView;
class UIButton;
class UIText;
class UIDataSource;
class EngineStatBase;

HYP_CLASS()
class ENGINE_API StatsOverlay : public OverlayBase
{
    HYP_OBJECT_BODY(StatsOverlay);

public:
    StatsOverlay();
    virtual ~StatsOverlay() override;

protected:
    HYP_METHOD()
    virtual Handle<UIObject> CreateUIObject_Impl(UIObject* spawnParent) override;

    HYP_METHOD()
    virtual int GetPlacement_Impl() const override
    {
        return 0;
    }

    HYP_METHOD()
    virtual void Update_Impl(float delta) override;

    HYP_METHOD()
    virtual bool IsEnabled_Impl() const override
    {
        return true;
    }

private:
    Handle<UIListView> m_panel;
    Handle<UIDataSource> m_dataSource;
    TMap<EngineStatBase*, UUID> m_statUuids;

    ClockTimer m_hotFunctionsUpdateTimer; // updates at a slower rate
    Handle<UIListView> m_hotFunctionsPanel;
    Handle<UIText> m_hotFunctionsText;
};

} // namespace Hyperion
