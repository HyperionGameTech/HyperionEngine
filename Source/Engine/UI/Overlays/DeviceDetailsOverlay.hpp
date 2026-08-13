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

class UIText;
class UIListView;

HYP_CLASS(NoScriptBindings)
class ENGINE_API DeviceDetailsOverlay : public OverlayBase
{
    HYP_OBJECT_BODY(DeviceDetailsOverlay);

public:
    DeviceDetailsOverlay();
    virtual ~DeviceDetailsOverlay() override;

protected:
    virtual Handle<UIObject> CreateUIObject(UIObject* spawnParent) override;

    virtual int GetPlacement() const override
    {
        return 1; // bottom-left
    }

    virtual void Update(float delta) override;

    virtual bool IsEnabled() const override
    {
        return true;
    }

private:
    Handle<UIListView> m_panel;
    Handle<UIText> m_fpsText;
    Handle<UIText> m_renderingBackendText;
    Handle<UIText> m_gpuModelText;
    Handle<UIText> m_boundStatusText;
};

} // namespace Hyperion
