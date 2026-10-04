/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Framework/Commandlet/Commandlet.hpp>
#include <Framework/DeviceTier/DeviceTierResolver.hpp>

#include <Core/Reflection/ClassUtils.hpp>
#include <Core/Reflection/ClassRegistry.hpp>

#include <Core/CLI/CommandLine.hpp>

namespace Hyperion {

class DeviceTierCommandlet : public CommandletBase
{
    HYP_OBJECT_BODY(DeviceTierCommandlet);

public:
    virtual ~DeviceTierCommandlet() override = default;

    static const CommandLineArgumentDefinitions& GetArgumentDefinitions()
    {
        static CommandLineArgumentDefinitions s_definitions;

        return s_definitions;
    }

protected:
    virtual Result Run(const CommandLineArguments& args) override
    {
        HYP_LOG(Engine, Info, "Device tier:\n{}", DeviceTierResolver::GetInstance().Describe());

        return {};
    }
};

HYP_EXPORT const Class* g_clsDeviceTierCommandlet = nullptr;

const Class* DeviceTierCommandlet::StaticClass()
{
    return g_clsDeviceTierCommandlet;
}

HYP_BEGIN_CLASS(DeviceTierCommandlet, -1, 0, NAME("CommandletBase"), ClassAttribute("command", "devicetier"))
    Method(NAME("GetArgumentDefinitions"), &Type::GetArgumentDefinitions)
HYP_END_CLASS

HYP_REGISTER_STATIC_CLASS(DeviceTierCommandlet);

} // namespace Hyperion
