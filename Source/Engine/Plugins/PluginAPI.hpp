/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/Utilities/EnumFlags.hpp>

#define HYP_PLUGIN_ABI_VERSION 1

#ifdef HYP_BUILD_PLUGIN
#define HYP_PLUGIN_API HYP_EXPORT
#else
#define HYP_PLUGIN_API
#endif

namespace Hyperion {

static constexpr const char PluginFunctionName_Query[] = "HypPluginQuery";
static constexpr const char PluginFunctionName_Load[] = "HypPluginLoad";
static constexpr const char PluginFunctionName_Unload[] = "HypPluginUnload";

enum class HypPluginHostFlags : uint32
{
    None = 0x0,
    Editor = 0x1,
    Game = 0x2
};

HYP_MAKE_ENUM_FLAGS(HypPluginHostFlags);

struct HypPluginDescriptor
{
    const char* name;
    const char* version;

    uint32 abiVersion;
    uint32 hostFlags;
};

class IPluginHost
{
public:
    virtual ~IPluginHost() = default;

    virtual uint32 GetABIVersion() const = 0;
    virtual uint32 GetEngineVersionMajor() const = 0;
    virtual uint32 GetEngineVersionMinor() const = 0;
    virtual uint32 GetEngineVersionPatch() const = 0;

    virtual bool IsEditorHost() const = 0;
};

class IPlugin
{
public:
    virtual ~IPlugin() = default;

    virtual void OnInitialize(const IPluginHost* host) = 0;
    virtual void OnShutdown()
    {
    }

    virtual void OnEditorLaunch()
    {
    }

    virtual void OnEditorShutdown()
    {
    }
};

} // namespace Hyperion

/// Plugin entry points
extern "C"
{

HYP_PLUGIN_API Hyperion::HypPluginDescriptor* HypPluginQuery();
HYP_PLUGIN_API Hyperion::IPlugin* HypPluginLoad(const Hyperion::IPluginHost* host);
HYP_PLUGIN_API void HypPluginUnload(Hyperion::IPlugin* plugin);

}
