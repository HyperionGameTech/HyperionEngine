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
class EditorCommandBase : public ObjectBase
{
    HYP_OBJECT_BODY(EditorCommandBase);

public:
    static Pool* GetAllocator() { return g_editorPool; }

    virtual ~EditorCommandBase() = default;

    HYP_METHOD()
    virtual String GetText() const;

    virtual void Execute(EditorSubsystem* subsystem) = 0;

    /*! \brief Whether this command is allowed to run while the editor's project world is actively simulating (playing).
     *  Defaults to false -- most editor commands mutate project/scene state in ways that aren't safe to run
     *  against a live simulation, so commands must opt in explicitly if they're safe to run while simulating. */
    virtual bool AllowedWhileSimulating() const
    {
        return false;
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

} // namespace Hyperion
