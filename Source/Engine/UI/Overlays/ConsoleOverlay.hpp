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

class UIConsole;

HYP_CLASS()
class ENGINE_API ConsoleOverlay : public OverlayBase
{
    HYP_OBJECT_BODY(ConsoleOverlay);

public:
    ConsoleOverlay();
    virtual ~ConsoleOverlay() override;

    HYP_METHOD()
    bool IsOpen() const
    {
        return m_isOpen;
    }

    HYP_METHOD()
    void SetOpen(bool isOpen);

    HYP_METHOD()
    void Toggle()
    {
        SetOpen(!m_isOpen);
    }

    virtual bool OnInputEvent(const Event& event) override;

protected:
    virtual Handle<UIObject> CreateUIObject(UIObject* spawnParent) override;

    virtual int GetPlacement() const override
    {
        return 1;
    }

    virtual void Update(float delta) override;

    virtual bool IsEnabled() const override
    {
        return m_isOpen;
    }

    virtual bool IgnoresSharedDebugUIVisibility() const override
    {
        return true;
    }

private:
    UIConsole* m_console;
    bool m_isOpen;
    bool m_toggleKeyHeld;
};

} // namespace Hyperion
