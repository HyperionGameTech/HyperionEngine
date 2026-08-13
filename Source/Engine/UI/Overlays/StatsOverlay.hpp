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

HYP_CLASS(NoScriptBindings)
class ENGINE_API StatsOverlay : public OverlayBase
{
    HYP_OBJECT_BODY(StatsOverlay);

public:
    StatsOverlay();
    virtual ~StatsOverlay() override;

protected:
    virtual Handle<UIObject> CreateUIObject(UIObject* spawnParent) override;

    virtual int GetPlacement() const override
    {
        return 0;
    }

    virtual void Update(float delta) override;

    virtual bool IsEnabled() const override
    {
        return true;
    }

private:
    Handle<UIListView> m_panel;
    Handle<UIDataSource> m_dataSource;
    Map<EngineStatBase*, UUID> m_statUuids;

    ClockTimer m_hotFunctionsUpdateTimer; // updates at a slower rate
    Handle<UIListView> m_hotFunctionsPanel;
    Handle<UIText> m_hotFunctionsText;
};

} // namespace Hyperion
