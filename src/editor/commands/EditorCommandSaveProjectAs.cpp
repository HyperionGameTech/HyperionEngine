/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <editor/EditorSubsystem.hpp>
#include <editor/EditorProject.hpp>

#include <editor/commands/EditorCommandSaveProjectAs.hpp>

#include <core/logging/Logger.hpp>

#include <EditorCommandSaveProjectAs.generated.inl>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

void EditorCommandSaveProjectAs::Execute(EditorSubsystem* subsystem)
{
    EditorProject* project = subsystem->GetCurrentProject();
    if (project != nullptr)
    {
    }
}

} // namespace hyperion
