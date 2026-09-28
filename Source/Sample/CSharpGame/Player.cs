using System;
using Hyperion;

namespace Hyperion.Samples
{
    public sealed class Player
    {
        private const float CapsuleRadius = 0.3f;
        private const float CapsuleHeight = 1.2f;
        private const float CameraPivotHeight = 1.7f;

        private const float CapsuleHeightInset = 0.1f;

        public Entity Entity { get; }
        public Camera Camera { get; }
        public ThirdPersonCameraController CameraController { get; }

        public Player(Node root, Vec3f spawnPosition, float cameraYawDegrees = 0.0f)
        {
            float capsuleHalfHeight = CapsuleHeight * 0.5f + CapsuleRadius;

            CapsulePhysicsShape capsuleShape = new CapsulePhysicsShape();
            capsuleShape.Radius = CapsuleRadius;
            capsuleShape.Height = CapsuleHeight;

            Entity = new Entity();
            Entity.Name = "Player";
            Entity.IsDynamic = true;
            Entity.LocalTranslation = spawnPosition + new Vec3f(0.0f, capsuleHalfHeight, 0.0f);

            root.AddChild(Entity);

            Entity.AddTag(EntityTag.Player);

            CharacterControllerComponent characterController = new CharacterControllerComponent
            {
                Shape = new Handle<PhysicsShape>(capsuleShape),
                Movement = new CharacterMovementSettings
                {
                    MoveSpeed = 3.0f,
                    SprintSpeed = 7.5f,
                    OrientToMovement = false
                }
            };

            Entity.AddComponent(ref characterController);

            CameraController = new ThirdPersonCameraController();
            Camera = CreateCamera(capsuleHalfHeight, cameraYawDegrees);
            AddCharacterModel(capsuleHalfHeight);
        }

        public Vec3f Position => Entity.GetWorldTranslation();

        public Vec3f AimDirection => CameraController.GetViewDirection();

        private Camera CreateCamera(float capsuleHalfHeight, float yawDegrees)
        {
            ThirdPersonCameraController cameraController = CameraController;

            Camera camera = new Camera();
            camera.Name = "PlayerCamera";
            camera.SetDimensions(new Vec2i(1920, 1080));
            camera.SetCameraFlags(CameraFlags.MatchWindowSize);
            camera.SetNearClip(0.1f);
            camera.SetFarClip(1000.0f);
            camera.IsDynamic = true;

            camera.Flags = camera.Flags | NodeFlags.IgnoreParentTransform;

            camera.AddCameraController(cameraController, 0);

            Entity.AddChild(camera);

            camera.AddTag(EntityTag.PrimaryCamera);

            float feetOffset = -(capsuleHalfHeight + CapsuleHeight - CapsuleHeightInset);
            cameraController.SetPivotOffset(new Vec3f(0.0f, feetOffset + CameraPivotHeight, 0.0f));
            cameraController.SetDistance(4.5f);

            cameraController.AddYawPitch(yawDegrees, 0.0f);

            return camera;
        }

        private void AddCharacterModel(float capsuleHalfHeight)
        {
            Prefab? characterPrefab = AssetRegistry.Engine?.GetAsset<Prefab>(AssetBucket.Prefabs, "ThirdPersonCharacter");
            Node? characterNode = characterPrefab?.Spawn();

            if (characterNode == null)
            {
                Logger.Log(LogLevel.Warning, "ThirdPersonCharacter prefab not found in the engine asset registry; the player will be invisible");

                return;
            }

            Entity characterModel = new Entity();
            characterModel.Name = "CharacterModel";
            characterModel.IsDynamic = true;

            Entity.AddChild(characterModel);
            characterModel.AddChild(characterNode);

            characterModel.LocalTranslation = new Vec3f(0.0f, -capsuleHalfHeight, 0.0f);

            Class? characterModelComponentClass = Class.TryGetClass("CharacterModelComponent");

            EntityManager? entityManager = characterModel.GetEntityManager();

            if (entityManager == null || !characterModelComponentClass.HasValue)
            {
                return;
            }

            Class componentClass = characterModelComponentClass.Value;

            if (!entityManager.AddDefaultComponent(characterModel, componentClass))
            {
                return;
            }

            // face where the camera looks rather than where we're moving, so thrown balls go the way the character faces
            // @TODO use a C# CharacterModelComponent struct instead of reflection, once the native component is cleaned up
            IntPtr componentPtr = entityManager.GetComponentPtr(characterModel, componentClass.TypeId);
            Property? facingModeProperty = componentClass.GetProperty(new Name("FacingMode"));

            if (componentPtr != IntPtr.Zero && facingModeProperty.HasValue)
            {
                using BoxedValue facingMode = new BoxedValue(CharacterFacingMode.ViewDirection);
                facingModeProperty.Value.Set(componentClass.Address, componentPtr, facingMode);
            }
        }
    }
}
