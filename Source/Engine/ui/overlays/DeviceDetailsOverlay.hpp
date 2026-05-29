/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <ui/overlays/Overlay.hpp>

#include <Core/math/Color.hpp>

#include <Core/containers/Array.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

class UIText;
class UIListView;

HYP_CLASS()
class ENGINE_API DeviceDetailsOverlay : public OverlayBase
{
    HYP_OBJECT_BODY(DeviceDetailsOverlay);

public:
    DeviceDetailsOverlay();
    virtual ~DeviceDetailsOverlay() override;

protected:
    HYP_METHOD()
    virtual Handle<UIObject> CreateUIObject_Impl(UIObject* spawnParent) override;

    HYP_METHOD()
    virtual int GetPlacement_Impl() const override
    {
        return 1; // bottom-left
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
    Handle<UIText> m_fpsText;
    Handle<UIText> m_gpuModelText;
    Handle<UIText> m_gpuVendorText;
    Handle<UIText> m_gpuTypeText;
};

} // namespace Hyperion
