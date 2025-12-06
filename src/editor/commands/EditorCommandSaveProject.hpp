/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <editor/EditorCommand.hpp>

namespace hyperion {

HYP_CLASS()
class HYP_API EditorCommandSaveProject final : public EditorCommandBase
{
    HYP_OBJECT_BODY(EditorCommandSaveProject);

public:
    virtual ~EditorCommandSaveProject() override = default;

    virtual void Execute(EditorSubsystem* subsystem) override;
};

} // namespace hyperion