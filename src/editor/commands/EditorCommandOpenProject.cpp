/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <editor/EditorSubsystem.hpp>
#include <editor/EditorActionStack.hpp>
#include <editor/EditorProject.hpp>

#include <editor/commands/EditorCommandOpenProject.hpp>

#include <core/logging/Logger.hpp>

#include <system/OpenFileDialog.hpp>

#include <EditorCommandOpenProject.generated.inl>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

HYP_API extern const FilePath& GetResourceDirectory();

void EditorCommandOpenProject::Execute(EditorSubsystem* subsystem)
{
    HYP_SCOPE;

    ShowOpenFileDialog(
        "Select the project to open",
        GetResourceDirectory(),
        { "hypproj" },
        /* allowMultiple */ false, /* allowDirectories */ true,
        [weakSubsystem = MakeWeakRef(subsystem)](TResult<Array<FilePath>>&& result) mutable
        {
            if (result.HasError())
            {
                HYP_LOG(Editor, Error, "Failed to select project file: {}", result.GetError().GetMessage());
                return;
            }

            if (result->Empty())
            {
                HYP_LOG(Editor, Warning, "No project file selected.");
                return;
            }

            GetThreadById(g_gameThread)->GetScheduler().Enqueue([weakSubsystem = std::move(weakSubsystem), projectFilepath = std::move(result.GetValue()[0])]() mutable
                {
                    Handle<EditorSubsystem> subsystem = weakSubsystem.Lock();
                    if (!subsystem)
                    {
                        HYP_LOG(Editor, Error, "Failed to lock EditorSubsystem from weak reference in ShowOpenProjectDialog");
                        return;
                    }

                    TResult<Handle<EditorProject>> loadProjectResult = EditorProject::Load(projectFilepath);

                    if (loadProjectResult.HasError())
                    {
                        HYP_LOG(Editor, Error, "Failed to load project: {}", loadProjectResult.GetError().GetMessage());
                        return;
                    }

                    Handle<EditorProject> project = loadProjectResult.GetValue();

                    if (!project.IsValid())
                    {
                        HYP_LOG(Editor, Error, "Loaded project is invalid.");
                        return;
                    }

                    subsystem->OpenProject(project);
                },
                TaskEnqueueFlags::FIRE_AND_FORGET);
        });
}

} // namespace hyperion
