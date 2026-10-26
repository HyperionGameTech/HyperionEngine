/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Containers/Array.hpp>
#include <Core/Containers/Map.hpp>

#include <Core/Functional/Proc.hpp>

#include <Core/Memory/UniquePtr.hpp>

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/Threading/Mutex.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

struct ModelImportContents;
struct ModelImportSettings;
struct AssetImportInfo;
class Node;
class Animation;

HYP_CLASS(Abstract)
class ENGINE_API ModelImportProcessorSettings : public ObjectBase
{
    HYP_OBJECT_BODY(ModelImportProcessorSettings);

public:
    ~ModelImportProcessorSettings() override = default;
};

class IModelImportProcessor
{
public:
    virtual ~IModelImportProcessor() = default;

    virtual const Class* GetSettingsClass() const
    {
        return nullptr;
    }

    virtual void Process(ModelImportContents& contents, const AssetImportInfo& importInfo, const ModelImportProcessorSettings* settings) = 0;

    virtual void OnClipsMerged(Node& root, const Map<const Animation*, Handle<Animation>>& mergedClipsByFreshClip)
    {
    }
};

class ENGINE_API ModelImportProcessorRegistry
{
public:
    static ModelImportProcessorRegistry& GetInstance();

    template <class ProcessorType>
    uint32 Register()
    {
        return Register(MakeUnique<ProcessorType>());
    }

    uint32 Register(UniquePtr<IModelImportProcessor>&& processor);
    void Unregister(uint32 id);

    void ForEach(const Proc<void(IModelImportProcessor&)>& proc) const;

private:
    struct Entry
    {
        uint32 id = 0;
        UniquePtr<IModelImportProcessor> processor;
    };

    mutable Mutex m_mutex;
    Array<Entry> m_entries;
    uint32 m_nextId = 1;
};

ENGINE_API void RunModelImportProcessors(ModelImportContents& contents, const AssetImportInfo& importInfo);
ENGINE_API void AddMissingProcessorSettings(ModelImportSettings& settings);

} // namespace Hyperion
