#include <Editor/Tasks/EditorTasks.hpp>

#include <Scene/LightmapVolume.hpp>
#include <Scene/EnvProbe.hpp>
#include <Scene/FogVolume.hpp>
#include <Scene/World.hpp>

#include <Baking/BakerSubsystem.hpp>
#include <Baking/Baker.hpp>

#include <Core/Logging/Logger.hpp>

#include <Core/Reflection/Class.hpp>

#include <EditorTasks.generated.inl>

namespace Hyperion {

EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);

#pragma region GenerateLightmapsEditorTask

GenerateLightmapsEditorTask::GenerateLightmapsEditorTask(const Handle<LightmapVolume>& volume)
    : GenerateLightmapsEditorTask(Array<Handle<ObjectBase>> { { StaticCast<ObjectBase>(volume) } })
{
}

GenerateLightmapsEditorTask::GenerateLightmapsEditorTask(const Handle<EnvProbe>& probe)
    : GenerateLightmapsEditorTask(Array<Handle<ObjectBase>> { { StaticCast<ObjectBase>(probe) } })
{
}

GenerateLightmapsEditorTask::GenerateLightmapsEditorTask(const Array<Handle<ObjectBase>>& sources)
    : TickableEditorTask(),
      m_sources(sources)
{
    for (auto it = m_sources.Begin(); it != m_sources.End();)
    {
        ObjectBase* source = *it;

        if (!source->IsA(LightmapVolume::StaticClass())
            && !source->IsA(EnvProbe::StaticClass())
            && !source->IsA(FogVolume::StaticClass()))
        {
            HYP_LOG(Editor, Error, "GenerateLightmapsEditorTask source is not a LightmapVolume or EnvProbe: \"{}\"", source->InstanceClass()->GetName());
            it = m_sources.Erase(it);

            continue;
        }

        ++it;
    }
}

void GenerateLightmapsEditorTask::Start()
{
    AssertOnThread(g_simThread);

    if (m_sources.Empty())
    {
        HYP_LOG(Editor, Error, "No valid sources provided for GenerateLightmapsEditorTask");

        return;
    }

    HYP_LOG(Editor, Verbose, "Generating lightmaps");

    if (!m_world.IsValid() || !m_scene.IsValid())
    {
        HYP_LOG(Editor, Error, "World or scene not set for GenerateLightmapsEditorTask");

        return;
    }

    BakerSubsystem* bakerSubsystem = m_world->GetSubsystem<BakerSubsystem>();

    if (!bakerSubsystem)
    {
        bakerSubsystem = m_world->AddSubsystem<BakerSubsystem>();
    }

    for (const Handle<ObjectBase>& source : m_sources)
    {
        Task<void> task;

        if (source->IsA<LightmapVolume>())
        {
            task = bakerSubsystem->EnqueueBake(StaticCast<LightmapVolume>(source));
        }
        else if (source->IsA<EnvProbe>())
        {
            task = bakerSubsystem->EnqueueBake(StaticCast<EnvProbe>(source));
        }
        else if (source->IsA<FogVolume>())
        {
            task = bakerSubsystem->EnqueueBake(StaticCast<FogVolume>(source));
        }

        if (task.IsValid())
        {
            m_tasks.PushBack(std::move(task));
        }
    }
}

void GenerateLightmapsEditorTask::Cancel()
{
    if (m_tasks.Any())
    {
        for (Task<void>& task : m_tasks)
        {
            task.Cancel();
        }
    }

    // @TODO Proper cancelation.
    // The baker subsystem needs to be aware that we cancelled.
}

bool GenerateLightmapsEditorTask::IsCompleted() const
{
    return m_tasks.Empty() || Every(m_tasks, &Task<void>::IsCompleted);
}

void GenerateLightmapsEditorTask::Tick()
{
    AssertOnThread(g_simThread);

    for (auto it = m_tasks.Begin(); it != m_tasks.End();)
    {
        Task<void>& task = *it;

        if (task.IsCompleted())
        {
            // remove task upon completion
            it = m_tasks.Erase(it);
        }
        else
        {
            ++it;
        }
    }
}

#pragma endregion GenerateLightmapsEditorTask

#pragma region GenerateBentNormalsEditorTask

GenerateBentNormalsEditorTask::GenerateBentNormalsEditorTask(const Array<Handle<LightmapVolume>>& volumes)
    : TickableEditorTask(),
      m_volumes(volumes)
{
}

void GenerateBentNormalsEditorTask::Start()
{
    AssertOnThread(g_simThread);

    if (m_volumes.Empty())
    {
        HYP_LOG(Editor, Error, "No LightmapVolumes provided for GenerateBentNormalsEditorTask");

        return;
    }

    HYP_LOG(Editor, Verbose, "Generating bent normals");

    if (!m_world.IsValid() || !m_scene.IsValid())
    {
        HYP_LOG(Editor, Error, "World or scene not set for GenerateBentNormalsEditorTask");

        return;
    }

    BakerSubsystem* lightmapperSubsystem = m_world->GetSubsystem<BakerSubsystem>();

    if (!lightmapperSubsystem)
    {
        lightmapperSubsystem = m_world->AddSubsystem<BakerSubsystem>();
    }

    const uint32 bentNormalOnlyMask = 1u << uint32(Baking::LightmapShadingType::BENT_NORMAL);

    for (const Handle<LightmapVolume>& volume : m_volumes)
    {
        Task<void> task = lightmapperSubsystem->EnqueueBake(volume, bentNormalOnlyMask);

        if (task.IsValid())
        {
            m_tasks.PushBack(std::move(task));
        }
    }
}

void GenerateBentNormalsEditorTask::Cancel()
{
    if (m_tasks.Any())
    {
        for (Task<void>& task : m_tasks)
        {
            task.Cancel();
        }
    }
}

bool GenerateBentNormalsEditorTask::IsCompleted() const
{
    return m_tasks.Empty() || Every(m_tasks, &Task<void>::IsCompleted);
}

void GenerateBentNormalsEditorTask::Tick()
{
    AssertOnThread(g_simThread);

    for (auto it = m_tasks.Begin(); it != m_tasks.End();)
    {
        Task<void>& task = *it;

        if (task.IsCompleted())
        {
            // remove task upon completion
            it = m_tasks.Erase(it);
        }
        else
        {
            ++it;
        }
    }
}

#pragma endregion GenerateBentNormalsEditorTask

} // namespace Hyperion
