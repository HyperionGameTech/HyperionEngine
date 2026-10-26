/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <AssetPch.hpp>

#include <Asset/ModelImportProcessor.hpp>
#include <Asset/ModelImport.hpp>
#include <Asset/AssetImportInfo.hpp>

#include <Core/Reflection/Class.hpp>
#include <Core/Reflection/BoxedValue.hpp>

#include <Core/Logging/Logger.hpp>

#include <ModelImportProcessor.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Assets);

ModelImportProcessorRegistry& ModelImportProcessorRegistry::GetInstance()
{
    static ModelImportProcessorRegistry s_instance;

    return s_instance;
}

uint32 ModelImportProcessorRegistry::Register(UniquePtr<IModelImportProcessor>&& processor)
{
    Mutex::Guard guard(m_mutex);

    const uint32 id = m_nextId++;

    m_entries.PushBack(Entry { id, std::move(processor) });

    return id;
}

void ModelImportProcessorRegistry::Unregister(uint32 id)
{
    Mutex::Guard guard(m_mutex);

    auto it = m_entries.FindIf([id](const Entry& entry)
        {
            return entry.id == id;
        });

    if (it != m_entries.End())
    {
        m_entries.Erase(it);
    }
}

void ModelImportProcessorRegistry::ForEach(const Proc<void(IModelImportProcessor&)>& proc) const
{
    Mutex::Guard guard(m_mutex);

    for (const Entry& entry : m_entries)
    {
        proc(*entry.processor);
    }
}

static Handle<ModelImportProcessorSettings> FindOrCreateProcessorSettings(const ModelImportSettings& settings, const Class* settingsClass)
{
    for (const Handle<ModelImportProcessorSettings>& processorSettings : settings.processorSettings)
    {
        if (processorSettings.IsValid() && processorSettings->IsA(settingsClass))
        {
            return processorSettings;
        }
    }

    BoxedValue boxed;

    if (!settingsClass->CreateInstance(boxed))
    {
        HYP_LOG(Assets, Error, "Failed to create default model import settings {}", settingsClass->GetName());

        return Handle<ModelImportProcessorSettings>::empty;
    }

    return boxed.Get<Handle<ModelImportProcessorSettings>>();
}

void RunModelImportProcessors(ModelImportContents& contents, const AssetImportInfo& importInfo)
{
    ModelImportProcessorRegistry::GetInstance().ForEach([&contents, &importInfo](IModelImportProcessor& processor)
        {
            Handle<ModelImportProcessorSettings> settings;

            if (const Class* settingsClass = processor.GetSettingsClass())
            {
                settings = FindOrCreateProcessorSettings(importInfo.settings, settingsClass);

                if (!settings.IsValid())
                {
                    return;
                }
            }

            processor.Process(contents, importInfo, settings.Get());
        });
}

void AddMissingProcessorSettings(ModelImportSettings& settings)
{
    ModelImportProcessorRegistry::GetInstance().ForEach([&settings](IModelImportProcessor& processor)
        {
            const Class* settingsClass = processor.GetSettingsClass();

            if (!settingsClass)
            {
                return;
            }

            const Handle<ModelImportProcessorSettings> processorSettings = FindOrCreateProcessorSettings(settings, settingsClass);

            if (processorSettings.IsValid() && !settings.processorSettings.Contains(processorSettings))
            {
                settings.processorSettings.PushBack(processorSettings);
            }
        });
}

} // namespace Hyperion
