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

    HYP_FIELD(Property = "SinglePlayer", Serialize, Editor, Title = "Single Player")
    bool singlePlayer = true;

    HYP_FIELD(Property = "Multiplayer", Serialize, Editor, Title = "Multiplayer")
    bool multiplayer = false;

    HYP_FIELD(Property = "ServerHost", Serialize, Editor, Title = "Server Host")
    String serverHost;

    HYP_FIELD(Property = "ServerPort", Serialize, Editor, Title = "Server Port")
    uint32 serverPort = 0;

    EDITOR_API Array<String> GetGameArgs() const;
};

} // namespace Hyperion
