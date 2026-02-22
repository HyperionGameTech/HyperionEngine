/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <editor/ui/EditorPropertyPanel.hpp>

#include <Core/reflection/ObjectMacros.hpp>

namespace Hyperion {

HYP_CLASS()
class HYP_API EditBoundingBox : public EditorPropertyPanelBase
{
    HYP_OBJECT_BODY(EditBoundingBox);

public:
    EditBoundingBox();
    virtual ~EditBoundingBox() override;

    virtual void Build_Impl(const BoxedValue& boxed, const Property* property) override;
};

} // namespace Hyperion
