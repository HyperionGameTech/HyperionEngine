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

protected:
    virtual Handle<UIObject> CreateUIObject(UIObject* spawnParent) override;

    virtual int GetPlacement() const override
    {
        return 1;
    }

    virtual void Update(float delta) override;

    virtual bool IsEnabled() const override
    {
        return true;
    }

private:
    UIConsole* m_console;
};

} // namespace Hyperion
