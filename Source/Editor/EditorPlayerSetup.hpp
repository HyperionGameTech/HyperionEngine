/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Reflection/Handle.hpp>

#include <Core/Name/Name.hpp>

namespace Hyperion {

class Node;
class Entity;
class Camera;
class CapsulePhysicsShape;

/*! \brief The pieces of a third-person player. Create them up front, then call AttachToScene once
 *  playerEntity has been added to a scene (components can only be added to entities in a scene). */
struct EditorThirdPersonPlayer
{
    Handle<Entity> playerEntity;
    Handle<CapsulePhysicsShape> capsuleShape;
    Handle<Camera> camera;

    // Null if the engine's ThirdPersonCharacter prefab hasn't been built (see BuildShapesCommandlet)
    Handle<Entity> characterModel;
};

class EDITOR_API EditorPlayerSetup final
{
public:
    static EditorThirdPersonPlayer CreateThirdPersonPlayer(Name playerName, Name cameraName);

    /*! \brief Adds the character controller and character model components, and parents the camera and model to the player.
     *  Safe to call again after an undo/redo cycle. */
    static void AttachToScene(const EditorThirdPersonPlayer& player);

    /*! \brief Adds a large static slab under \p parent with its top face at y = 0, so a new project's player has something to stand on. */
    static Handle<Entity> AddGround(const Handle<Node>& parent, Name name);
};

} // namespace Hyperion
