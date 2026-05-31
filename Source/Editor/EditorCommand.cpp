/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <EditorPch.hpp>

#include <Editor/EditorCommand.hpp>

#include <Core/reflection/Class.hpp>

#include <EditorCommand.generated.inl>

namespace Hyperion {

String EditorCommandBase::GetText() const
{
    return InstanceClass()->GetName().LookupString();
}

} // namespace Hyperion
