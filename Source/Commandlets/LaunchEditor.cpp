/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 *
 *  Usage: LaunchEditor
 */

#include <HyperionPch.hpp>

#include <Core/reflection/ClassUtils.hpp>
#include <Core/reflection/ClassRegistry.hpp>

#include <engine/commandlet/Commandlet.hpp>

#include <dotnet/DotNETHost.hpp>
#include <dotnet/Assembly.hpp>
#include <dotnet/ManagedClass.hpp>

namespace Hyperion {

namespace PlatformUtils {
ENGINE_API extern PlatformString GetExecutableAbsolutePath();
} // namespace PlatformUtils

class LaunchEditor : public CommandletBase
{
    HYP_OBJECT_BODY(LaunchEditor);

public:
    virtual ~LaunchEditor() override = default;

protected:
    virtual Result Run_Impl(const CommandLineArguments &args) override
    {
#if defined(HYP_COMMANDLET) && defined(HYP_DOTNET)
        const FilePath basePath = FilePath(PlatformUtils::GetExecutableAbsolutePath().ToUtf8()).BasePath();

        DotNETHost &host = DotNETHost::GetInstance();

        if (!host.IsEnabled())
        {
            return { HYP_MAKE_ERROR(Error, "DotNETHost is not enabled (HYP_DOTNET not set)") };
        }

        host.Initialize(basePath);

        RC<dotnet::Assembly> editorAssembly = host.LoadAssembly("Hyperion.Editor.dll");

        if (!editorAssembly || !editorAssembly->IsLoaded())
        {
            return { HYP_MAKE_ERROR(Error, "Failed to load Hyperion.Editor.dll") };
        }

        RC<dotnet::ManagedClass> editorEntryClass = editorAssembly->FindClassByName("EditorEntry");

        if (!editorEntryClass)
        {
            return { HYP_MAKE_ERROR(Error, "Failed to find EditorEntry class in Hyperion.Editor.dll") };
        }

        editorEntryClass->InvokeStaticMethod<void>("Run");

        return {};
#else
        return { HYP_MAKE_ERROR(Error, "LaunchEditor requires HYP_DOTNET to be enabled") };
#endif
    }
};

const Class *g_clsLaunchEditor = nullptr;

const Class* LaunchEditor::StaticClass()
{
    return g_clsLaunchEditor;
}

HYP_BEGIN_CLASS(LaunchEditor, -1, 0, NAME("CommandletBase"),
    ClassAttribute("command", "launcheditor"))
HYP_END_CLASS

HYP_REGISTER_STATIC_CLASS(LaunchEditor);

} // namespace Hyperion
