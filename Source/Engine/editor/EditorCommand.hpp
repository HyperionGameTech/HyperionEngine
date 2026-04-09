/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Types.hpp>

#include <Core/reflection/ObjectBase.hpp>
#include <Core/reflection/Handle.hpp>

namespace Hyperion {

class EditorSubsystem;

HYP_CLASS(Abstract)
class HYP_API EditorCommandBase : public ObjectBase
{
    HYP_OBJECT_BODY(EditorCommandBase);

public:
    virtual ~EditorCommandBase() = default;

    HYP_METHOD()
    virtual String GetText() const;

    virtual void Execute(EditorSubsystem* subsystem) = 0;

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