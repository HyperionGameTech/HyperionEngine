/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <editor/ui/EditorPropertyPanel.hpp>

#include <core/reflection/ObjectMacros.hpp>

namespace hyperion {

HYP_CLASS()
class HYP_API EditBoundingBox : public EditorPropertyPanelBase
{
    HYP_OBJECT_BODY(EditBoundingBox);

public:
    EditBoundingBox();
    virtual ~EditBoundingBox() override;

    virtual void Build_Impl(const HypData& hypData, const Property* property) override;
};

} // namespace hyperion
