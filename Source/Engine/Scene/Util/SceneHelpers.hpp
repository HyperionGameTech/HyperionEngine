/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once


namespace Hyperion {

class Camera;
class World;
class Entity;
class Scene;

struct CharacterControllerComponent;

struct PlayerMove;

namespace net {
enum class NetConnectionId : uint32;
} // namespace net

namespace SceneHelpers {

Camera* FindMainCamera(World& world);

Entity* FindMyLocalPlayerEntity(const Scene& scene, net::NetConnectionId ownerConnectionId);

bool IsLocalPlayerEntity(const Entity& entity);

/// Can we simulate physics for the entity?
bool CanSimulateEntityPhysics(const Entity& entity);

float GetCapsuleHeightOffset(const CharacterControllerComponent& component);
void MoveCharacter(Entity* entity, CharacterControllerComponent& component, const PlayerMove& move, Vec3f& outResultTranslation);

} // namespace SceneHelpers

} // namespace Hyperion
