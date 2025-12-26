/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <editor/ui/EditorPropertyPanel.hpp>

#include <core/reflection/ObjectMacros.hpp>

namespace Hyperion {

HYP_CLASS()
class HYP_API EditName : public EditorPropertyPanelBase
{
    HYP_OBJECT_BODY(EditName);

public:
    EditName();
    virtual ~EditName() override;

    virtual void Build_Impl(const BoxedValue& boxed, const Property* property) override;
};

} // namespace Hyperion
