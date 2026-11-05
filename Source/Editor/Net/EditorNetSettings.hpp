/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Reflection/ObjectMacros.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/String.hpp>

#include <Editor/EditorMemory.hpp>

namespace Hyperion {

HYP_STRUCT()
struct EditorNetSettings
{
    HYP_STRUCT_BODY(EditorNetSettings);

    HYP_FIELD(Property = "Multiplayer", Serialize, Editor, Label = "Multiplayer")
    bool multiplayer = false;

    HYP_FIELD(Property = "ServerHost", Serialize, Editor, Label = "Server Host", ShowIf = "Multiplayer")
    String serverHost;

    HYP_FIELD(Property = "ServerPort", Serialize, Editor, Label = "Server Port", ShowIf = "Multiplayer")
    uint32 serverPort = 0;

    EDITOR_API Array<String> GetGameArgs() const;
};

} // namespace Hyperion
