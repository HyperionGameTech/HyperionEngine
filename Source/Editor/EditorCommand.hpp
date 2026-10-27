/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Types.hpp>

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Editor/EditorMemory.hpp>

namespace Hyperion {

class EditorSubsystem;

HYP_CLASS(Abstract)
class EDITOR_API EditorCommandBase : public ObjectBase
{
    HYP_OBJECT_BODY(EditorCommandBase);

public:
    static Pool* GetAllocator() { return g_editorPool; }

    virtual ~EditorCommandBase() = default;

    HYP_METHOD()
    virtual String GetText() const;

    virtual void Execute(EditorSubsystem* subsystem) = 0;

    virtual bool AllowedWhileSimulating() const
    {
        return false;
    }

    virtual bool AllowedWhileEditingPrefab() const
    {
        return true;
    }

    HYP_METHOD()
    const Array<String>& GetArguments() const
    {
        return m_args;
    }

    HYP_METHOD()
    void SetArguments(const Array<String>& args)
    {
        m_args = args;
    }

    HYP_METHOD()
    int NumArguments() const
    {
        return int(m_args.Size());
    }

    HYP_METHOD()
    const String& GetArgument(int index) const
    {
        if (index < 0 || index >= int(m_args.Size()))
        {
            return String::empty;
        }

        return m_args[index];
    }

private:
    Array<String> m_args;
};

// commands defined in a built-in plugin's editor module (DEFINE_EDITOR_COMMAND under HYP_PLUGIN_NAME), which only run while it's enabled
EDITOR_API void RegisterPluginEditorCommand(const char* commandClassName, const char* pluginName);
EDITOR_API const char* GetPluginOfEditorCommand(Name commandClassName);

struct PluginEditorCommandRegistration
{
    PluginEditorCommandRegistration(const char* commandClassName, const char* pluginName)
    {
        RegisterPluginEditorCommand(commandClassName, pluginName);
    }
};

} // namespace Hyperion
