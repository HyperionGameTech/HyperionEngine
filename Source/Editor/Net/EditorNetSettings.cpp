/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <Editor/Net/EditorNetSettings.hpp>

#include <EditorNetSettings.generated.inl>

namespace Hyperion {

Array<String> EditorNetSettings::GetGameArgs() const
{
    Array<String> args;

    if (singlePlayer || !multiplayer)
    {
        args.PushBack("--singleplayer");
    }

    if (multiplayer && serverHost.Any())
    {
        args.PushBack(HYP_FORMAT("--host={}", serverHost));
    }

    if (multiplayer && serverPort != 0)
    {
        args.PushBack(HYP_FORMAT("--gameport={}", serverPort));
    }

    return args;
}

} // namespace Hyperion
