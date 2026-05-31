/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <ui/overlays/Overlay.hpp>

#include <Core/math/Color.hpp>

#include <Core/containers/Array.hpp>
#include <Core/containers/Map.hpp>

#include <Core/utilities/Uuid.hpp>

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
    HYP_METHOD()
    virtual Handle<UIObject> CreateUIObject_Impl(UIObject* spawnParent) override;

    HYP_METHOD()
    virtual int GetPlacement_Impl() const override
    {
        return 1;
    }

    HYP_METHOD()
    virtual void Update_Impl(float delta) override;

    HYP_METHOD()
    virtual bool IsEnabled_Impl() const override
    {
        return true;
    }

private:
    UIConsole* m_console;
};

} // namespace Hyperion
