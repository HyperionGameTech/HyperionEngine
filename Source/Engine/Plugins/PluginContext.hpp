/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Functional/Proc.hpp>

#include <Core/Name/Name.hpp>

#include <Scene/WorldSystemRegistry.hpp>

#include <Asset/ModelImportProcessor.hpp>

namespace Hyperion {

enum class PluginHostMode : uint32;

class ENGINE_API PluginContext
{
public:
    PluginContext(
        Name pluginName,
        Name moduleName,
        PluginHostMode hostMode);

    PluginContext(const PluginContext& other) = delete;
    PluginContext& operator=(const PluginContext& other) = delete;

    ~PluginContext();

    HYP_FORCE_INLINE Name GetPluginName() const
    {
        return m_pluginName;
    }

    HYP_FORCE_INLINE Name GetModuleName() const
    {
        return m_moduleName;
    }

    HYP_FORCE_INLINE PluginHostMode GetHostMode() const
    {
        return m_hostMode;
    }

    bool IsEditorHost() const;

    template <typename SystemType>
    void RegisterWorldSystem(
        EnumFlags<WorldFlags> requiredFlags = WorldFlags::None,
        EnumFlags<WorldFlags> excludedFlags = WorldFlags::None,
        Proc<bool(const World&)>&& predicate = {})
    {
        m_worldSystemIds.PushBack(WorldSystemRegistry::GetInstance().Register<SystemType>(
            requiredFlags,
            excludedFlags,
            std::move(predicate)));
    }

    template <typename ProcessorType>
    void RegisterModelImportProcessor()
    {
        m_modelImportProcessorIds.PushBack(ModelImportProcessorRegistry::GetInstance().Register<ProcessorType>());
    }

    void AddCleanup(Proc<void()>&& cleanup);

    void Release();

private:
    Name m_pluginName;
    Name m_moduleName;
    PluginHostMode m_hostMode;

    Array<uint32> m_worldSystemIds;
    Array<uint32> m_modelImportProcessorIds;
    Array<Proc<void()>> m_cleanups;
};

} // namespace Hyperion
