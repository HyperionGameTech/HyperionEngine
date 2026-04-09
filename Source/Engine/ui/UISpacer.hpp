/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <ui/UIObject.hpp>

namespace Hyperion {

#pragma region UISpacer

HYP_CLASS()
class HYP_API UISpacer : public UIObject
{
    HYP_OBJECT_BODY(UISpacer);

public:
    UISpacer() = default;
    virtual ~UISpacer() override = default;
};

#pragma endregion UISpacer

} // namespace Hyperion