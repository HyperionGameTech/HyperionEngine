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

HYP_CLASS(NoScriptBindings)
class ENGINE_API BaseStatsOverlay : public OverlayBase
{
    HYP_OBJECT_BODY(BaseStatsOverlay);

    static const Array<Pair<int, Color>> s_fpsColors;

public:
    BaseStatsOverlay();
    virtual ~BaseStatsOverlay() override;

protected:
    virtual Handle<UIObject> CreateUIObject(UIObject* spawnParent) override;

    virtual int GetPlacement() const override
    {
        return 1; // Bottom-left corner
    }

    virtual void Update(float delta) override;

    virtual bool IsEnabled() const override
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
