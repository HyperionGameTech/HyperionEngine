/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <editor/EditorSubsystem.hpp>
#include <editor/EditorActionStack.hpp>
#include <editor/EditorProject.hpp>

#include <editor/commands/EditorCommandUndo.hpp>

#include <EditorCommandUndo.generated.inl>

namespace hyperion {

void EditorCommandUndo::Execute(EditorSubsystem* subsystem)
{
    EditorProject* project = subsystem->GetCurrentProject();
    if (project != nullptr)
    {
        project->GetActionStack()->Undo();
    }
}

} // namespace hyperion
