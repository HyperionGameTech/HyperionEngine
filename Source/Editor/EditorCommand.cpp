/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <EditorPch.hpp>

#include <Editor/EditorCommand.hpp>

#include <Core/Reflection/Class.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Utilities/Pair.hpp>

#include <EditorCommand.generated.inl>

namespace Hyperion {

String EditorCommandBase::GetText() const
{
    return InstanceClass()->GetName().LookupString();
}

// filled at static init, before Names can be created, so it keeps the class names as strings
static Array<Pair<const char*, const char*>>& GetPluginEditorCommands()
{
    static Array<Pair<const char*, const char*>> s_pluginEditorCommands;

    return s_pluginEditorCommands;
}

void RegisterPluginEditorCommand(const char* commandClassName, const char* pluginName)
{
    GetPluginEditorCommands().PushBack({ commandClassName, pluginName });
}

const char* GetPluginOfEditorCommand(Name commandClassName)
{
    const ANSIStringView className = commandClassName.LookupString();

    for (const Pair<const char*, const char*>& entry : GetPluginEditorCommands())
    {
        if (className == ANSIStringView(entry.first) && className.Size() == ANSIStringView(entry.first).Size())
        {
            return entry.second;
        }
    }

    return nullptr;
}

} // namespace Hyperion
