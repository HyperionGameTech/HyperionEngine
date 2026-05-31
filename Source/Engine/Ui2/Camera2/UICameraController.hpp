/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Scene/camera/OrthoCamera.hpp>

namespace Hyperion {

HYP_CLASS()
class UICameraController : public OrthoCameraController
{
    HYP_OBJECT_BODY(UICameraController);

public:
    UICameraController() = default;
    UICameraController(float left, float right, float bottom, float top)
        : OrthoCameraController(left, right, bottom, top)
    {
    }

    virtual ~UICameraController() override = default;
};

} // namespace Hyperion
