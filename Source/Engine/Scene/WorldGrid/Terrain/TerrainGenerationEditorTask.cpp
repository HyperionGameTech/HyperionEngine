/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/WorldGrid/Terrain/TerrainGenerationEditorTask.hpp>

#ifdef HYP_EDITOR

#include <Editor/EditorTask.hpp>
#include <Editor/EditorState.hpp>

#include <Framework/EngineGlobals.hpp>

#include <Core/Threading/AtomicVar.hpp>
#include <Core/Threading/Guarded.hpp>

namespace Hyperion {

namespace TerrainGenerationEditorTask {

struct TerrainGenerationEditorTaskState
{
    ///cells queued for generation that haven't finished loading yet
    AtomicVar<int32> numPendingCells { 0 };
    Guarded<Handle<TickableEditorTask>> editorTask;

    void OnGenerationQueued()
    {
        numPendingCells.Increment(1, MemoryOrder::RELAXED);

        Update();
    }

    void OnGenerationFinished()
    {
        numPendingCells.Decrement(1, MemoryOrder::RELAXED);

        Update();
    }

    void Update()
    {
        auto readCount = [numPendingCells = &numPendingCells]() -> int
        {
            return numPendingCells->Get(MemoryOrder::RELAXED);
        };

        auto updateTaskWithCount = [](Handle<TickableEditorTask>& task, int count) -> bool
        {
            if (task.IsValid() && task->IsCancellationRequested())
            {
                task.Reset();

                return true;
            }

            if (count <= 0)
            {
                if (task.IsValid())
                {
                    task->Cancel();

                    if (task->IsCancellationRequested())
                    {
                        task.Reset();
                    }
                }

                return true;
            }

            if (task.IsValid())
            {
                task->SetDescription(HYP_FORMAT("{} cells", count));

                return true;
            }

            return false;
        };

        auto updateTask = [readCount, updateTaskWithCount](Handle<TickableEditorTask>& task, int& outCount) -> bool
        {
            outCount = readCount();

            return updateTaskWithCount(task, outCount);
        };

        editorTask.Access([this, readCount, updateTaskWithCount](Handle<TickableEditorTask>& task)
        {
            const int count = readCount();
            if (updateTaskWithCount(task, count))
            {
                return;
            }

            if (!g_editorState.IsValid() || !EngineGlobals::IsEditor())
            {
                return;
            }

            Handle<TickableEditorTask> newTask = MakeHandle<TickableEditorTask>(
                [this, readCount, updateTaskWithCount]()
                {
                    editorTask.Access([&](Handle<TickableEditorTask>& task)
                    {
                        int count = readCount();
                        const bool res = updateTaskWithCount(task, count);

                        AssertDebug(res);
                    });
                },
                "Generating terrain",
                HYP_FORMAT("{} cells", count));

            InitObject(newTask);

            newTask->SetIsForegroundTask(true);

            g_editorState->AddTask(newTask);

            task = std::move(newTask);
        });
    }
};

static TerrainGenerationEditorTaskState s_state;

void OnGenerationQueued()
{
    s_state.OnGenerationQueued();
}

void OnGenerationFinished()
{
    s_state.OnGenerationFinished();
}

} // namespace TerrainGenerationEditorTask

} // namespace Hyperion

#endif // HYP_EDITOR
