/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <ui/UIObject.hpp>

namespace hyperion {

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

} // namespace hyperion