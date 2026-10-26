/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Editor/EditorPlayerSetup.hpp>

namespace Hyperion {

// a third-person player gets the ThirdPersonCharacter prefab as its model, driven by a CharacterModelComponent
class AnimationPlayerSetupProcessor final : public IPlayerSetupProcessor
{
public:
    void OnCreatePlayer(EditorPlayer& player, bool isThirdPerson) override;
    void OnAttachToScene(const EditorPlayer& player) override;
};

} // namespace Hyperion
