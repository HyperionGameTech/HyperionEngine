/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Memory/UniquePtr.hpp>

#include <Core/Utilities/StringView.hpp>

namespace Hyperion {

class PluginContext;

class IPluginModule
{
public:
    virtual ~IPluginModule() = default;

    virtual void OnLoad(PluginContext& context) = 0;

    virtual void OnUnload()
    {
    }
};

struct PluginModuleRegistration
{
    ANSIStringView pluginName;
    ANSIStringView moduleName;
    UniquePtr<IPluginModule> (*create)() = nullptr;
};

ENGINE_API void RegisterPluginModule(const PluginModuleRegistration& registration);
ENGINE_API const Array<PluginModuleRegistration>& GetRegisteredPluginModules();

// each linked plugin module adds one at static init (Generated/Plugins/<Module>/Register/PluginModuleRegister.cpp), and InitializePluginModules
// runs them during engine init. The editor's engine library links every built-in plugin's; a game on a static engine links the ones it uses
using PluginModuleInitializer = void (*)();

ENGINE_API void AddPluginModuleInitializer(PluginModuleInitializer initializer);
ENGINE_API void InitializePluginModules();

} // namespace Hyperion

//Every plugin must use this in order to be registered with the engine upon application startup
#define HYP_DEFINE_PLUGIN_MODULE(PluginName, ModuleName, ModuleClass)                  \
    HYP_EXPORT void HypRegisterPluginModule_##ModuleName()                               \
    {                                                                                    \
        ::Hyperion::RegisterPluginModule(::Hyperion::PluginModuleRegistration {          \
            #PluginName,                                                                 \
            #ModuleName,                                                                 \
            []() -> ::Hyperion::UniquePtr<::Hyperion::IPluginModule>                     \
            {                                                                            \
                return ::Hyperion::MakeUnique<ModuleClass>();                            \
            } });                                                                        \
    }
