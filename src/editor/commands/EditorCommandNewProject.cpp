/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <editor/EditorSubsystem.hpp>
#include <editor/EditorActionStack.hpp>
#include <editor/EditorProject.hpp>

#include <editor/commands/EditorCommandNewProject.hpp>

#include <EditorCommandNewProject.generated.inl>

namespace hyperion {

void EditorCommandNewProject::Execute(EditorSubsystem* subsystem)
{
    subsystem->NewProject();
}

} // namespace hyperion
