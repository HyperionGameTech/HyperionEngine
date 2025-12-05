/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Types.hpp>

#include <core/reflection/ObjectBase.hpp>
#include <core/reflection/Handle.hpp>

namespace hyperion {

class EditorSubsystem;

HYP_CLASS(Abstract)
class HYP_API EditorCommandBase : public ObjectBase
{
    HYP_OBJECT_BODY(EditorCommandBase);

public:
    virtual ~EditorCommandBase() = default;

    virtual void Execute(EditorSubsystem* subsystem) = 0;
};

} // namespace hyperion