#include <Editor/Tasks/EditorTasks.hpp>

#include <Scene/LightmapVolume.hpp>
#include <Scene/EnvProbe.hpp>
#include <Scene/FogVolume.hpp>
#include <Scene/Light/Light.hpp>
#include <Scene/World.hpp>
#include <Scene/Swatch.hpp>

#include <Scene/Util/SceneHelpers.hpp>

#include <Baking/BakerSubsystem.hpp>
#include <Baking/Baker.hpp>

#include <Core/Logging/Logger.hpp>
#include <Core/Threading/Threads.hpp>

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

GenerateLightmapsEditorTask::GenerateLightmapsEditorTask(Span<const Handle<ObjectBase>> sources)
    : TickableEditorTask(),
      m_sources(sources)
{
    m_title = "Bake Task";

    for (auto it = m_sources.Begin(); it != m_sources.End();)
    {
        ObjectBase* source = *it;

        if (!source->IsA(LightmapVolume::StaticClass())
            && !source->IsA(EnvProbe::StaticClass())
            && !source->IsA(FogVolume::StaticClass())
            && !source->IsA(Light::StaticClass()))
        {
            HYP_LOG(Editor, Error, "GenerateLightmapsEditorTask source is not a LightmapVolume, EnvProbe, FogVolume, or Light: \"{}\"", source->InstanceClass()->GetName());
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

    Handle<Swatch> activeSwatch = m_world->GetActiveSwatch();
    Assert(activeSwatch.IsValid());

    if (!activeSwatch.IsValid())
    {
        HYP_LOG(Editor, Error, "No active swatch set for world; cannot bake");

        return;
    }

    BakerSubsystem* bakerSubsystem = m_world->GetSubsystem<BakerSubsystem>();

    if (!bakerSubsystem)
    {
        bakerSubsystem = m_world->AddSubsystem<BakerSubsystem>();
    }

    uint32 numEnqueued = 0;

    for (const Handle<ObjectBase>& source : m_sources)
    {
        Handle<Entity> entitySource = DynamicCast<Entity>(source);
        Assert(entitySource.IsValid());

        Task<void> task;

        if (source->IsA<LightmapVolume>())
        {
            task = bakerSubsystem->EnqueueBake(activeSwatch->bakeLayer, StaticCast<LightmapVolume>(source));
        }
        else if (source->IsA<EnvProbe>())
        {
            task = bakerSubsystem->EnqueueBake(activeSwatch->bakeLayer, StaticCast<EnvProbe>(source));
        }
        else if (source->IsA<FogVolume>())
        {
            task = bakerSubsystem->EnqueueBake(activeSwatch->bakeLayer, StaticCast<FogVolume>(source));
        }
        else if (source->IsA<Light>())
        {
            task = bakerSubsystem->EnqueueBake(activeSwatch->bakeLayer, StaticCast<Light>(source));
        }

        if (task.IsValid())
        {
            m_tasks.PushBack(std::move(task));

            ++numEnqueued;
        }
        else
        {
            HYP_LOG(Editor, Warning, "Could not enqueue bake for {}: a bake may already be in progress for it",
                source->Id());
        }
    }

    if (numEnqueued == 0)
    {
        HYP_LOG(Editor, Error, "No bakes were enqueued for {} source(s); ensure they belong to the active swatch '{}' and have no bake currently running",
            m_sources.Size(), activeSwatch->name);
    }
}

void GenerateLightmapsEditorTask::Cancel_Impl()
{
    if (m_world.IsValid())
    {
        auto cancelImpl = [world = m_world, sources = m_sources]()
        {
            if (BakerSubsystem* bakerSubsystem = world->GetSubsystem<BakerSubsystem>())
            {
                for (const Handle<ObjectBase>& source : sources)
                {
                    bakerSubsystem->CancelBake(source.Get());
                }
            }
        };

        if (IsOnThread(g_simThread))
        {
            cancelImpl();
        }
        else
        {
            GetThreadById(g_simThread)->GetScheduler().Enqueue(
                cancelImpl, TaskEnqueueFlags::FIRE_AND_FORGET);
        }
    }

    TickableEditorTask::Cancel_Impl();
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

    if (!m_world.IsValid() || m_sources.Empty())
    {
        return;
    }

    if (BakerSubsystem* bakerSubsystem = m_world->GetSubsystem<BakerSubsystem>())
    {
        float totalProgress = 0.0f;
        uint32 numRemaining = 0;

        for (const Handle<ObjectBase>& source : m_sources)
        {
            const float sourceProgress = bakerSubsystem->GetBakeProgress(source.Get());

            totalProgress += sourceProgress;

            if (sourceProgress < 1.0f)
            {
                ++numRemaining;
            }
        }

        SetProgress(totalProgress / float(m_sources.Size()));

        SetDescription(numRemaining > 0
                ? HYP_FORMAT("Baking {} of {} sources", m_sources.Size() - numRemaining + 1, m_sources.Size())
                : "Finalizing");
    }
}

#pragma endregion GenerateLightmapsEditorTask

#pragma region GenerateBentNormalsEditorTask

GenerateBentNormalsEditorTask::GenerateBentNormalsEditorTask(const Array<Handle<LightmapVolume>>& volumes)
    : TickableEditorTask(),
      m_volumes(volumes)
{
    m_title = "Generating bent normals";
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

    const uint32 bentNormalOnlyMask = 1u << uint32(Baking::PathTraceType::BentNormals);

    Handle<Swatch> activeSwatch = m_world->GetActiveSwatch();
    Assert(activeSwatch.IsValid());

    if (!activeSwatch.IsValid())
    {
        HYP_LOG(Editor, Warning, "Active swatch was not valid?!?");

        return;
    }

    uint32 numEnqueued = 0;

    for (const Handle<LightmapVolume>& volume : m_volumes)
    {
        Task<void> task = lightmapperSubsystem->EnqueueBake(activeSwatch->bakeLayer, volume, bentNormalOnlyMask);

        if (task.IsValid())
        {
            m_tasks.PushBack(std::move(task));

            ++numEnqueued;
        }
        else
        {
            HYP_LOG(Editor, Warning, "Could not enqueue bent normals bake for {}: a bake may already be in progress for it",
                volume->Id());
        }
    }

    if (numEnqueued == 0)
    {
        HYP_LOG(Editor, Error, "No bent normals bakes were enqueued for {} volume(s); ensure they belong to the active swatch '{}' and have no bake currently running",
            m_volumes.Size(), activeSwatch->name);
    }
}

void GenerateBentNormalsEditorTask::Cancel_Impl()
{
    if (m_world.IsValid())
    {
        GetThreadById(g_simThread)->GetScheduler().Enqueue(
            [world = m_world, volumes = m_volumes]()
            {
                if (BakerSubsystem* bakerSubsystem = world->GetSubsystem<BakerSubsystem>())
                {
                    for (const Handle<LightmapVolume>& volume : volumes)
                    {
                        bakerSubsystem->CancelBake(volume.Get());
                    }
                }
            });
    }

    TickableEditorTask::Cancel_Impl();
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

    if (!m_world.IsValid() || m_volumes.Empty())
    {
        return;
    }

    if (BakerSubsystem* bakerSubsystem = m_world->GetSubsystem<BakerSubsystem>())
    {
        float totalProgress = 0.0f;
        uint32 numRemaining = 0;

        for (const Handle<LightmapVolume>& volume : m_volumes)
        {
            const float volumeProgress = bakerSubsystem->GetBakeProgress(volume.Get());

            totalProgress += volumeProgress;

            if (volumeProgress < 1.0f)
            {
                ++numRemaining;
            }
        }

        SetProgress(totalProgress / float(m_volumes.Size()));

        SetDescription(numRemaining > 0
                ? HYP_FORMAT("Baking {} of {} volumes", numRemaining, m_volumes.Size())
                : "Finishing up");
    }
}

#pragma endregion GenerateBentNormalsEditorTask

} // namespace Hyperion
