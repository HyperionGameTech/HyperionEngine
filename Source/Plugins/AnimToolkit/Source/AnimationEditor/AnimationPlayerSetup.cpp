/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <AnimationEditor/AnimationPlayerSetup.hpp>

#include <Scene/Entity.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/Prefab.hpp>

#include <Animation/CharacterModelComponent.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/AssetBucket.hpp>

#include <Core/Logging/Logger.hpp>

namespace Hyperion {

EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);

void AnimationPlayerSetupProcessor::OnCreatePlayer(EditorPlayer& player, bool isThirdPerson)
{
    if (!isThirdPerson || player.characterModel.IsValid())
    {
        return;
    }

    Handle<Prefab> characterPrefab = GetEngineAssetRegistry()->GetAsset<Prefab>(AssetBuckets::Prefabs, "ThirdPersonCharacter"_sh);

    Handle<Node> characterNode = characterPrefab.IsValid() ? characterPrefab->Spawn() : Handle<Node>::Null();

    if (!characterNode)
    {
        HYP_LOG(Editor, Warning, "ThirdPersonCharacter prefab not found in the engine asset registry; run BuildShapesCommandlet to build it. The player will have no visible model.");

        return;
    }

    player.characterModel = MakeHandle<Entity>();
    player.characterModel->SetName(NAME("CharacterModel"));
    player.characterModel->SetIsDynamic(true);
    InitObject(player.characterModel);

    player.characterModel->AddChild(characterNode);
}

void AnimationPlayerSetupProcessor::OnAttachToScene(const EditorPlayer& player)
{
    // CharacterModelSystem keeps the model aligned to the capsule from here on
    if (!player.characterModel.IsValid() || player.characterModel->HasComponent<CharacterModelComponent>())
    {
        return;
    }

    CharacterModelComponent characterModelComponent;
    characterModelComponent.facingMode = CharacterFacingMode::MovementDirection;
    characterModelComponent.turnSharpness = 20.0f;

    player.characterModel->AddComponent<CharacterModelComponent>(characterModelComponent);
}

} // namespace Hyperion
