/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <editor/ui/EditorPropertyPanel.hpp>

#include <core/reflection/ObjectMacros.hpp>

namespace hyperion {

HYP_CLASS()
class HYP_API EditTransform : public EditorPropertyPanelBase
{
    HYP_OBJECT_BODY(EditTransform);

public:
    EditTransform();
    virtual ~EditTransform() override;

    virtual void Build_Impl(const HypData& hypData, const Property* property) override;
};

} // namespace hyperion
