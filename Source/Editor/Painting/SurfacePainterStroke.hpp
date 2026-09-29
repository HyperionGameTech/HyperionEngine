/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Containers/Array.hpp>

#include <Editor/EditorMemory.hpp>
#include <Editor/EditorSubsystem.hpp>
#include <Editor/EditorProject.hpp>
#include <Editor/EditorAction.hpp>
#include <Editor/EditorActionStack.hpp>

#include <Scene/Scene.hpp>

namespace Hyperion {

template <class Traits>
class SurfacePainterStroke
{
public:
    using Target = typename Traits::Target;
    using Record = typename Traits::Record;

    struct TargetEdit
    {
        Handle<Target> target;

        bool createdByStroke = false;

        Array<Record, EditorAllocator> added;
        Array<Record, EditorAllocator> removed;
    };

    HYP_FORCE_INLINE const Array<TargetEdit>& GetEdits() const
    {
        return m_edits;
    }

    void Reset()
    {
        m_edits.Clear();
    }

    TargetEdit& GetEdit(const Handle<Target>& target, bool createdByStroke = false)
    {
        for (TargetEdit& edit : m_edits)
        {
            if (edit.target == target)
            {
                return edit;
            }
        }

        TargetEdit& edit = m_edits.EmplaceBack();
        edit.target = target;
        edit.createdByStroke = createdByStroke;

        return edit;
    }

    void RecordAdded(const Handle<Target>& target, const Record& record)
    {
        GetEdit(target).added.PushBack(record);
    }

    void RecordRemoved(const Handle<Target>& target, const Record& record)
    {
        TargetEdit& edit = GetEdit(target);

        // erasing something placed earlier in this same stroke just cancels it out
        auto addedIt = edit.added.FindIf([&record](const Record& added)
            {
                return Traits::GetId(added) == Traits::GetId(record);
            });

        if (addedIt != edit.added.End())
        {
            edit.added.Erase(addedIt);

            return;
        }

        edit.removed.PushBack(record);
    }

    void Commit(EditorSubsystem* subsystem, const WeakHandle<Scene>& strokeScene)
    {
        Array<TargetEdit> edits = std::move(m_edits);
        m_edits.Clear();

        uint32 numAdded = 0;
        uint32 numRemoved = 0;

        for (const TargetEdit& edit : edits)
        {
            numAdded += uint32(edit.added.Size());
            numRemoved += uint32(edit.removed.Size());
        }

        if (numAdded == 0 && numRemoved == 0)
        {
            // a target created for stamps that got erased again within the same stroke
            for (const TargetEdit& edit : edits)
            {
                if (edit.createdByStroke && Traits::IsEmpty(*edit.target))
                {
                    edit.target->Remove();
                }
            }

            return;
        }

        const Handle<EditorProject>& project = subsystem->GetCurrentProject();

        if (!project.IsValid())
        {
            return;
        }

        const String actionText = numAdded == 0
            ? HYP_FORMAT("Erase {} {}(s)", numRemoved, Traits::Noun)
            : HYP_FORMAT("Paint {} {}(s)", numAdded, Traits::Noun);

        // the stroke has already been applied; execute runs again on push so both sides are idempotent
        Handle<FunctionalEditorAction> action = MakeHandle<FunctionalEditorAction>(
            actionText,
            Proc<EditorActionFunctions()>(
                [strokeScene, edits]() -> EditorActionFunctions
                {
                    return EditorActionFunctions {
                        .execute = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [strokeScene, edits](EditorSubsystem*, EditorProject*)
                            {
                                Handle<Scene> scene = strokeScene.Lock();

                                if (!scene.IsValid())
                                {
                                    return;
                                }

                                for (const TargetEdit& edit : edits)
                                {
                                    if (edit.createdByStroke && edit.target->GetParent() == nullptr)
                                    {
                                        scene->GetRoot()->AddChild(edit.target);
                                    }

                                    for (const Record& record : edit.removed)
                                    {
                                        Traits::Remove(*edit.target, record);
                                    }

                                    for (const Record& record : edit.added)
                                    {
                                        Traits::Add(*edit.target, record);
                                    }
                                }
                            }),
                        .revert = Proc<void(EditorSubsystem*, EditorProject*)>(
                            [edits](EditorSubsystem*, EditorProject*)
                            {
                                for (const TargetEdit& edit : edits)
                                {
                                    for (const Record& record : edit.added)
                                    {
                                        Traits::Remove(*edit.target, record);
                                    }

                                    for (const Record& record : edit.removed)
                                    {
                                        Traits::Add(*edit.target, record);
                                    }

                                    if (edit.createdByStroke)
                                    {
                                        edit.target->Remove();
                                    }
                                }
                            })
                    };
                }));

        InitObject(action);

        project->GetActionStack()->PushAction(action);
    }

private:
    Array<TargetEdit> m_edits;
};

} // namespace Hyperion
