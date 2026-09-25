/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <Editor/EditorPlayerSetup.hpp>

#include <Scene/EntityManager.hpp>
#include <Scene/Entity.hpp>
#include <Scene/EntityTag.hpp>
#include <Scene/Prefab.hpp>

#include <Scene/Camera/Camera.hpp>
#include <Scene/Camera/ThirdPersonCamera.hpp>

#include <Scene/Components/CharacterControllerComponent.hpp>
#include <Scene/Components/CharacterModelComponent.hpp>
#include <Scene/Components/MeshComponent.hpp>
#include <Scene/Components/RigidBodyComponent.hpp>

#include <Scene/Util/SceneHelpers.hpp>

#include <Physics/PhysicsShape.hpp>

#include <Rendering/Mesh.hpp>
#include <Rendering/Material.hpp>
#include <Rendering/Util/MeshBuilder.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/AssetBucket.hpp>

#include <Core/Logging/Logger.hpp>

namespace Hyperion {

EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);

namespace /* Constants */ {

static constexpr float PlayerCapsuleRadius = 0.3f;
static constexpr float PlayerCapsuleHeight = 1.2f;

static constexpr float PlayerWalkSpeed = 1.35f;
static constexpr float PlayerSprintSpeed = 7.5f;

static constexpr float CameraPivotHeight = 1.8f;

static constexpr float GroundHalfExtent = 100.0f;
static constexpr float GroundHalfThickness = 0.5f;
} // namespace

EditorThirdPersonPlayer EditorPlayerSetup::CreateThirdPersonPlayer(Name playerName, Name cameraName)
{
    EditorThirdPersonPlayer player;

    player.playerEntity = MakeHandle<Entity>();
    player.playerEntity->SetName(playerName);
    player.playerEntity->SetIsDynamic(true);
    InitObject(player.playerEntity);

    player.capsuleShape = MakeHandle<CapsulePhysicsShape>(NAME_FMT("{}CapsuleShape", playerName), PlayerCapsuleRadius, PlayerCapsuleHeight);
    InitObject(player.capsuleShape);

    player.camera = MakeHandle<Camera>();
    player.camera->SetName(cameraName);
    player.camera->SetDimensions(Vec2i(1920, 1080));
    player.camera->SetCameraFlags(CameraFlags::MatchWindowSize | CameraFlags::HasStreamingVolume);
    player.camera->SetFarClip(3000.0f);
    player.camera->SetNearClip(0.1f);
    player.camera->SetIsDynamic(true);
    // The third person controller positions the camera in world space itself
    player.camera->SetNodeFlags(player.camera->GetNodeFlags() | NodeFlags::IgnoreParentTransform);
    player.camera->AddTag<EntityTag::PrimaryCamera>();
    player.camera->AddCameraController(MakeHandle<ThirdPersonCameraController>());
    InitObject(player.camera);

    Handle<Prefab> characterPrefab = GetEngineAssetRegistry()->GetAsset<Prefab>(AssetBuckets::Prefabs, "ThirdPersonCharacter"_sh);

    if (Handle<Node> characterNode = characterPrefab.IsValid() ? characterPrefab->Spawn() : Handle<Node>::Null())
    {
        player.characterModel = MakeHandle<Entity>();
        player.characterModel->SetName(NAME("CharacterModel"));
        player.characterModel->SetIsDynamic(true);
        InitObject(player.characterModel);

        player.characterModel->AddChild(characterNode);
    }
    else
    {
        HYP_LOG(Editor, Warning, "ThirdPersonCharacter prefab not found in the engine asset registry; run BuildShapesCommandlet to build it. The player will have no visible model.");
    }

    return player;
}

void EditorPlayerSetup::AttachToScene(const EditorThirdPersonPlayer& player)
{
    Assert(player.playerEntity.IsValid());
    Assert(player.playerEntity->GetScene() != nullptr);

    Entity* playerEntity = player.playerEntity.Get();

    const float capsuleHalfHeight = player.capsuleShape->GetHeight() * 0.5f + player.capsuleShape->GetRadius();

    if (!playerEntity->HasComponent<CharacterControllerComponent>())
    {
        CharacterControllerComponent characterControllerComponent;
        characterControllerComponent.shape = player.capsuleShape;
        characterControllerComponent.movement.moveSpeed = PlayerWalkSpeed;
        characterControllerComponent.movement.sprintSpeed = PlayerSprintSpeed;

        playerEntity->AddComponent<CharacterControllerComponent>(characterControllerComponent);
    }
    else
    {
        playerEntity->GetComponent<CharacterControllerComponent>().shape = player.capsuleShape;
    }

    if (player.camera.IsValid())
    {
        if (player.camera->GetParent() != playerEntity)
        {
            playerEntity->AddChild(player.camera);
        }

        // Pivot is relative to the player's origin, which MoveCharacter places above the capsule center while playing
        const float playingFeetOffset = -(capsuleHalfHeight + SceneHelpers::GetCapsuleHeightOffset(playerEntity->GetComponent<CharacterControllerComponent>()));

        if (ThirdPersonCameraController* thirdPersonController = DynamicCast<ThirdPersonCameraController>(player.camera->GetCameraController().Get()))
        {
            thirdPersonController->SetPivotOffset(Vec3f(0.0f, playingFeetOffset + CameraPivotHeight, 0.0f));
        }
    }

    if (player.characterModel.IsValid())
    {
        if (player.characterModel->GetParent() != playerEntity)
        {
            playerEntity->AddChild(player.characterModel);
        }

        // CharacterModelSystem keeps this aligned from here on; start with the feet at the bottom of the capsule
        player.characterModel->SetLocalTranslation(Vec3f(0.0f, -capsuleHalfHeight, 0.0f));

        if (!player.characterModel->HasComponent<CharacterModelComponent>())
        {
            CharacterModelComponent characterModelComponent;
            characterModelComponent.facingMode = CharacterFacingMode::MovementDirection;
            characterModelComponent.turnSharpness = 20.0f;

            player.characterModel->AddComponent<CharacterModelComponent>(characterModelComponent);
        }
    }
}

Handle<Entity> EditorPlayerSetup::AddGround(const Handle<Node>& parent, Name name)
{
    Assert(parent.IsValid());

    Handle<Mesh> groundMesh = MeshBuilder::Cube();
    groundMesh->SetName(NAME_FMT("{}Mesh", name));
    GetCurrentAssetRegistry()->PutAssetUnique(groundMesh);

    MaterialAttributes materialAttributes;
    materialAttributes.shaderName = NAME("GeometryPass");
    materialAttributes.bucket = RenderBucket::Opaque;

    MaterialParameters materialParameters;
    materialParameters.albedo = Vec4f(1.0f, 1.0f, 1.0f, 1.0f);
    materialParameters.roughness = 0.08f;
    materialParameters.metalness = 0.0f;

    MaterialTextures materialTextures;
    // @TODO Checkerboard texture. But don't get from RI.placeholderData -- if headless, RI will not be initialized.
    ///materialTextures[MaterialTextureKey::Diffuse] = 

    Handle<Material> groundMaterial = MakeHandle<Material>(NAME_FMT("{}Material", name), materialAttributes, materialParameters, materialTextures);    
    InitObject(groundMaterial);
    GetCurrentAssetRegistry()->PutAssetUnique(groundMaterial);

    const BoundingBox& meshBounds = groundMesh->GetAABB();

    Handle<BoxPhysicsShape> groundShape = MakeHandle<BoxPhysicsShape>(NAME_FMT("{}PhysicsShape", name), meshBounds);
    InitObject(groundShape);
    GetCurrentAssetRegistry()->PutAssetUnique(groundShape);

    const Vec3f meshHalfExtent = Vec3f::Max(meshBounds.GetExtent() * 0.5f, Vec3f(0.0001f));
    const Vec3f groundScale = Vec3f(GroundHalfExtent, GroundHalfThickness, GroundHalfExtent) / meshHalfExtent;

    Handle<Entity> groundEntity = MakeHandle<Entity>();
    groundEntity->SetName(name);
    groundEntity->SetLocalScale(groundScale);
    groundEntity->SetLocalTranslation(Vec3f(0.0f, -meshBounds.GetMax().y * groundScale.y - 0.01f /* padding to prevent z-fight with grid */, 0.0f));
    InitObject(groundEntity);

    parent->AddChild(groundEntity);

    MeshComponent meshComponent;
    meshComponent.mesh = groundMesh;
    meshComponent.material = groundMaterial;
    groundEntity->AddComponent<MeshComponent>(meshComponent);

    groundEntity->SetLocalBounds(meshBounds);

    RigidBodyComponent rigidBodyComponent;
    rigidBodyComponent.shape = groundShape;
    groundEntity->AddComponent<RigidBodyComponent>(rigidBodyComponent);

    return groundEntity;
}

} // namespace Hyperion
