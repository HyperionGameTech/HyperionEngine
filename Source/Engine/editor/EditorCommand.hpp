/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <Core/Types.hpp>

#include <Core/reflection/ObjectBase.hpp>
#include <Core/reflection/Handle.hpp>

namespace Hyperion {

class EditorSubsystem;

HYP_CLASS(Abstract)
class HYP_API EditorCommandBase : public ObjectBase
{
    HYP_OBJECT_BODY(EditorCommandBase);

public:
    virtual ~EditorCommandBase() = default;

    virtual void Execute(EditorSubsystem* subsystem) = 0;
};

} // namespace Hyperion