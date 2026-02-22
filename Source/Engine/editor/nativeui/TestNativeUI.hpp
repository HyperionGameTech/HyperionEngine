
#pragma once

#include <editor/EditorWindow.hpp>

#include <Core/functional/Delegate.hpp>
#include <Core/math/Vector3.hpp>

namespace Hyperion {

struct AddReflectionProbeResult
{
    uint32 textureDimension = 128; // 64, 128, or 256
    bool bakeLighting = false;
    Vec3f probeVolumeDimensions = Vec3f(10.0f, 10.0f, 10.0f);
    Vec3f worldTranslation = Vec3f(0.0f, 0.0f, 0.0f);
};

HYP_CLASS(NoScriptBindings)
class TestNativeUI : public EditorWindow
{
    HYP_OBJECT_BODY(TestNativeUI);

public:
    Delegate<void, AddReflectionProbeResult> OnAccepted;
    Delegate<void> OnCancelled;

protected:
    virtual void Show_Internal() override;
};

} // namespace Hyperion
