/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <UI/Overlays/Overlay.hpp>

#include <Core/Math/Color.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

class World;
class UIText;
class UIListView;

HYP_CLASS()
class ENGINE_API BaseStatsOverlay : public OverlayBase
{
    HYP_OBJECT_BODY(BaseStatsOverlay);

    static const Array<Pair<int, Color>> s_fpsColors;

public:
    BaseStatsOverlay();
    virtual ~BaseStatsOverlay() override;

protected:
    HYP_METHOD()
    virtual Handle<UIObject> CreateUIObject_Impl(UIObject* spawnParent) override;

    HYP_METHOD()
    virtual int GetPlacement_Impl() const override
    {
        return 1; // Bottom-left corner
    }

    HYP_METHOD()
    virtual void Update_Impl(float delta) override;

    HYP_METHOD()
    virtual bool IsEnabled_Impl() const override
    {
        return true;
    }

private:
    static Color GetFpsColor(int fps);

    Handle<UIText> m_memoryUsageTextElement;
    Handle<UIText> m_fpsTextElement;
    Handle<UIText> m_countersTextElement;

    float m_deltaAccumGame;
    int m_numTicksGame;
};

} // namespace Hyperion
