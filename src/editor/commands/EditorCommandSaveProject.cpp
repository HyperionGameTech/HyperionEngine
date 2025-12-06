/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <editor/EditorSubsystem.hpp>
#include <editor/EditorProject.hpp>

#include <editor/commands/EditorCommandSaveProject.hpp>

#include <core/logging/Logger.hpp>

#include <EditorCommandSaveProject.generated.inl>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

void EditorCommandSaveProject::Execute(EditorSubsystem* subsystem)
{
    EditorProject* project = subsystem->GetCurrentProject();
    if (project != nullptr)
    {
        Result result = project->Save();
        if (!result)
        {
            HYP_LOG(Editor, Error, "Failed to save project: {}", result.GetError().GetMessage());
        }
    }
}

} // namespace hyperion
