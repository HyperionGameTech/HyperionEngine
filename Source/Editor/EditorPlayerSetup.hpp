/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Reflection/Handle.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Functional/Proc.hpp>
#include <Core/Memory/UniquePtr.hpp>
#include <Core/Threading/Mutex.hpp>

#include <Plugins/PluginContext.hpp>

#include <Editor/EditorMemory.hpp>

namespace Hyperion {

class Node;
class Entity;
class Camera;
class CapsulePhysicsShape;

/*! \brief The pieces of a player. Create them up front, then call AttachToScene once
 *  playerEntity has been added to a scene (components can only be added to entities in a scene). */
struct EditorPlayer
{
    Handle<Entity> playerEntity;
    Handle<CapsulePhysicsShape> capsuleShape;
    Handle<Camera> camera;

    // the visible model, filled in by a player setup processor; parented under the player with its feet at the bottom of the capsule
    Handle<Entity> characterModel;
};

class IPlayerSetupProcessor
{
public:
    virtual ~IPlayerSetupProcessor() = default;

    // before the player is in a scene; can fill in characterModel
    virtual void OnCreatePlayer(EditorPlayer& player, bool isThirdPerson)
    {
    }

    // the player is in a scene, so components can be added; called again after an undo/redo cycle
    virtual void OnAttachToScene(const EditorPlayer& player)
    {
    }
};

class EDITOR_API PlayerSetupProcessorRegistry
{
public:
    static PlayerSetupProcessorRegistry& GetInstance();

    template <class ProcessorType>
    uint32 Register()
    {
        return Register(MakeUniqueWithAllocator<ProcessorType, EditorAllocator>());
    }

    uint32 Register(UniquePtr<IPlayerSetupProcessor, EditorAllocator>&& processor);
    void Unregister(uint32 id);

    void ForEach(const Proc<void(IPlayerSetupProcessor&)>& proc) const;

private:
    struct Entry
    {
        uint32 id = 0;
        UniquePtr<IPlayerSetupProcessor, EditorAllocator> processor;
    };

    mutable Mutex m_mutex;
    Array<Entry> m_entries;
    uint32 m_nextId = 1;
};

template <class ProcessorType>
void RegisterPlayerSetupProcessor(PluginContext& context)
{
    const uint32 id = PlayerSetupProcessorRegistry::GetInstance().Register<ProcessorType>();

    context.AddCleanup([id]()
        {
            PlayerSetupProcessorRegistry::GetInstance().Unregister(id);
        });
}

class EDITOR_API EditorPlayerSetup final
{
public:
    static constexpr float GroundSurfaceHeight = 0.05f;

    static EditorPlayer CreateThirdPersonPlayer(Name playerName, Name cameraName);
    static EditorPlayer CreateFirstPersonPlayer(Name playerName, Name cameraName);

    /*! \brief Adds the character controller and character model components, and parents the camera and model to the player.
     *  Safe to call again after an undo/redo cycle. */
    static void AttachToScene(const EditorPlayer& player);

    /*! \brief Adds a large static slab under \p parent with its top face at y = groundSurfaceHeight (just above the editor grid), so a new project's player has something to stand on. */
    static Handle<Entity> AddGround(const Handle<Node>& parent, Name name);
};

} // namespace Hyperion
