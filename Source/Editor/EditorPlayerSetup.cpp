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
#include <Scene/Camera/FirstPersonCamera.hpp>
#include <Scene/Camera/ThirdPersonCamera.hpp>

#include <Scene/Components/CharacterControllerComponent.hpp>
#include <Scene/Components/MeshComponent.hpp>
#include <Scene/Components/RigidBodyComponent.hpp>

#include <Scene/Util/SceneHelpers.hpp>

#include <Physics/PhysicsShape.hpp>

#include <Rendering/Mesh.hpp>
#include <Rendering/Material.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/Util/MeshBuilder.hpp>
#include <Rendering/Util/TextureUtils.hpp>

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
static constexpr float FirstPersonEyeHeight = 1.65f;

static constexpr float GroundHalfExtent = 100.0f;
static constexpr float GroundHalfThickness = 0.5f;

static constexpr float GroundCheckerTileSize = 2.0f;
static constexpr uint32 GroundCheckerTextureSize = 64;
static const Color GroundCheckerLightColor { 0.58f, 0.58f, 0.58f };
static const Color GroundCheckerDarkColor { 0.27f, 0.27f, 0.27f };

EditorPlayer CreatePlayer(Name playerName, Name cameraName, EnumFlags<NodeFlags> cameraNodeFlags, const Handle<CameraController>& cameraController)
{
    EditorPlayer player;

    player.playerEntity = MakeHandle<Entity>();
    player.playerEntity->SetName(playerName);
    player.playerEntity->SetIsDynamic(true);
    player.playerEntity->AddTag<EntityTag::Player>();
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
    player.camera->SetNodeFlags(player.camera->GetNodeFlags() | cameraNodeFlags);
    player.camera->AddTag<EntityTag::PrimaryCamera>();
    player.camera->AddCameraController(cameraController);
    InitObject(player.camera);

    return player;
}

} // namespace

EditorPlayer EditorPlayerSetup::CreateFirstPersonPlayer(Name playerName, Name cameraName)
{
    Handle<FirstPersonCameraController> cameraController = MakeHandle<FirstPersonCameraController>();
    cameraController->SetMovementEnabled(false);

    // The camera rides the player at eye height but keeps its own look rotation
    EditorPlayer player = CreatePlayer(playerName, cameraName, NodeFlags::IgnoreParentRotation, cameraController);

    PlayerSetupProcessorRegistry::GetInstance().ForEach([&player](IPlayerSetupProcessor& processor)
        {
            processor.OnCreatePlayer(player, /* isThirdPerson */ false);
        });

    return player;
}

EditorPlayer EditorPlayerSetup::CreateThirdPersonPlayer(Name playerName, Name cameraName)
{
    // The third person controller positions the camera in world space itself
    EditorPlayer player = CreatePlayer(playerName, cameraName, NodeFlags::IgnoreParentTransform, MakeHandle<ThirdPersonCameraController>());

    PlayerSetupProcessorRegistry::GetInstance().ForEach([&player](IPlayerSetupProcessor& processor)
        {
            processor.OnCreatePlayer(player, /* isThirdPerson */ true);
        });

    return player;
}

void EditorPlayerSetup::AttachToScene(const EditorPlayer& player)
{
    Assert(player.playerEntity.IsValid());
    Assert(player.playerEntity->GetScene() != nullptr);

    Entity* playerEntity = player.playerEntity.Get();

    const float capsuleHalfHeight = player.capsuleShape->GetHeight() * 0.5f + player.capsuleShape->GetRadius();

    FirstPersonCameraController* firstPersonController = player.camera.IsValid()
        ? TryCast<FirstPersonCameraController>(player.camera->GetCameraController().Get())
        : nullptr;

    if (!playerEntity->HasComponent<CharacterControllerComponent>())
    {
        CharacterControllerComponent characterControllerComponent;
        characterControllerComponent.shape = player.capsuleShape;
        characterControllerComponent.movement.moveSpeed = PlayerWalkSpeed;
        characterControllerComponent.movement.sprintSpeed = PlayerSprintSpeed;
        characterControllerComponent.movement.orientToMovement = firstPersonController == nullptr;

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

        if (ThirdPersonCameraController* thirdPersonController = TryCast<ThirdPersonCameraController>(player.camera->GetCameraController().Get()))
        {
            thirdPersonController->SetPivotOffset(Vec3f(0.0f, playingFeetOffset + CameraPivotHeight, 0.0f));
        }
        else if (firstPersonController != nullptr)
        {
            player.camera->SetLocalTranslation(Vec3f(0.0f, playingFeetOffset + FirstPersonEyeHeight, 0.0f));
        }
    }

    if (player.characterModel.IsValid())
    {
        if (player.characterModel->GetParent() != playerEntity)
        {
            playerEntity->AddChild(player.characterModel);
        }

        player.characterModel->SetLocalTranslation(Vec3f(0.0f, -capsuleHalfHeight, 0.0f));
    }

    PlayerSetupProcessorRegistry::GetInstance().ForEach([&player](IPlayerSetupProcessor& processor)
        {
            processor.OnAttachToScene(player);
        });
}

PlayerSetupProcessorRegistry& PlayerSetupProcessorRegistry::GetInstance()
{
    static PlayerSetupProcessorRegistry s_instance;

    return s_instance;
}


uint32 PlayerSetupProcessorRegistry::Register(UniquePtr<IPlayerSetupProcessor, EditorAllocator>&& processor)
{
    Mutex::Guard guard(m_mutex);

    const uint32 id = m_nextId++;

    m_entries.PushBack(Entry { id, std::move(processor) });

    return id;
}

void PlayerSetupProcessorRegistry::Unregister(uint32 id)
{
    Mutex::Guard guard(m_mutex);

    auto it = m_entries.FindIf([id](const Entry& entry)
        {
            return entry.id == id;
        });

    if (it != m_entries.End())
    {
        m_entries.Erase(it);
    }
}

void PlayerSetupProcessorRegistry::ForEach(const Proc<void(IPlayerSetupProcessor&)>& proc) const
{
    Mutex::Guard guard(m_mutex);

    for (const Entry& entry : m_entries)
    {
        proc(*entry.processor);
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
    materialParameters.uvScale = Vec2f(GroundHalfExtent * 2.0f / GroundCheckerTileSize);

    Handle<Texture> groundTexture = TextureUtils::CreateCheckerTexture(
        NAME_FMT("{}Texture", name),
        GroundCheckerTextureSize,
        GroundCheckerLightColor,
        GroundCheckerDarkColor);

    GetCurrentAssetRegistry()->PutAssetUnique(groundTexture);

    MaterialTextures materialTextures;
    materialTextures[MaterialTextureKey::Diffuse] = groundTexture;

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
    groundEntity->SetLocalTranslation(Vec3f(0.0f, -1.0f, 0.0f));
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
