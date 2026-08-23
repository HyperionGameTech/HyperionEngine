/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Name/Name.hpp>
#include <Core/Types.hpp>

#include <Core/Math/Color.hpp>

#include <Core/Utilities/DataMutationState.hpp>

#include <Scripting/ScriptableDelegate.hpp>

#include <Core/Logging/LoggerFwd.hpp>

#include <Scene/Node.hpp>
#include <Scene/Entity.hpp>
#include <Scene/SceneOctree.hpp>

#include <Scene/Camera/Camera.hpp>

#include <Rendering/RenderTypes.hpp>

#include <Asset/AssetObject.hpp>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Scene);

class World;
class Scene;
class EntityManager;
class WorldGrid;

// clang-format off

HYP_ENUM()
enum class SceneFlags : uint32
{
    NONE = 0x0,             //!< @editor=false

    FOREGROUND = 0x1,       //!< @title="Is foreground scene" @description="Scene is a foreground scene (i.e., it is rendered normally)."
    BACKDROP = 0x2,         //!< @title="Is backdrop" @description="Scene contains backdrop nodes (e.g skybox). (Not mutually exclusive with Foreground)"
    
    /// These below are reserved for internal usage and not surfaced in-editor
    DETACHED = 0x4,         //!< @editor=false -- Scene is not attached to any World.
    UI = 0x8,               //!< @editor=false -- Scene is created for UI (see UIStage).
    EDITOR = 0x10,          //!< @editor=false -- Scene is an editor-owned scene.

    STREAMED = 0x20,        //!< @title="Is streamed in" @description="Allow the engine to load/unload from memory based on proximity to Streaming Volumes"
    HAS_OCTREE = 0x40,      //!< @title="Uses Octree for spatial partitioning" @description="Scene uses an octree for spatial partitioning."

    AUDIO_LISTENER = 0x80,  //!< @title="Is primary audio listener" @description="Scene has an audio listener (only one scene in a world should have this flag set)"

    DEFAULT = FOREGROUND | STREAMED | HAS_OCTREE    //!< @editor=false
};

// clang-format on

HYP_MAKE_ENUM_FLAGS(SceneFlags);

extern void Scene_OnPostLoad(Scene& scene);

HYP_CLASS(PostLoad = "Scene_OnPostLoad", AssetBucket = "Scenes")
class ENGINE_API Scene final : public AssetObject
{
    friend class World;
    friend class UIStage;

    HYP_OBJECT_BODY(Scene);

public:
    Scene();

    explicit Scene(EnumFlags<SceneFlags> flags);
    explicit Scene(Name name, EnumFlags<SceneFlags> flags = SceneFlags::DEFAULT);
    explicit Scene(Name name, ThreadId ownerThreadId, EnumFlags<SceneFlags> flags = SceneFlags::DEFAULT);

    Scene(const Scene& other) = delete;
    Scene& operator=(const Scene& other) = delete;

    ~Scene();

    void Initialize();
    void Shutdown();

    /*! \brief Get the thread Id that owns this Scene. */
    HYP_FORCE_INLINE ThreadId GetOwnerThreadId() const
    {
        return m_ownerThreadId;
    }

    /*! \brief Set the thread Id that owns this Scene.
     *  This is used to assert that the Scene is being accessed from the correct thread.
     *  \note Only call this if you know what you are doing. */
    void SetOwnerThreadId(ThreadId ownerThreadId);

    HYP_METHOD()
    Camera* GetPrimaryCamera() const;

    HYP_METHOD()
    EnumFlags<SceneFlags> GetSceneFlags() const
    {
        return m_sceneFlags;
    }

    HYP_METHOD()
    void SetSceneFlags(EnumFlags<SceneFlags> flags)
    {
        m_sceneFlags = flags;
        MarkDirty();
    }

    HYP_METHOD()
    HYP_NODISCARD Node* FindNodeByName(StringHash name) const;

    HYP_METHOD()
    HYP_NODISCARD Node* FindNodeByUUID(const UUID& uuid) const;

    HYP_METHOD(Property = "Root", Editor = true)
    const Handle<Node>& GetRoot() const
    {
        return m_root;
    }

    HYP_METHOD(Property = "Root", Editor = true)
    void SetRoot(const Handle<Node>& root);

    HYP_METHOD()
    const Handle<EntityManager>& GetEntityManager() const
    {
        return m_entityManager;
    }

    SceneOctree& GetOctree()
    {
        return m_octree;
    }

    const SceneOctree& GetOctree() const
    {
        return m_octree;
    }

    HYP_METHOD()
    bool IsAttachedToWorld() const
    {
        return m_world != nullptr;
    }

    HYP_METHOD(Property = "World", Transient)
    World* GetWorld() const
    {
        return m_world;
    }

    HYP_METHOD(Property = "World", Transient)
    void SetWorld(World* world);

    HYP_METHOD()
    bool IsForegroundScene() const
    {
        return m_sceneFlags & SceneFlags::FOREGROUND;
    }

    HYP_METHOD()
    bool IsBackgroundScene() const
    {
        return !(m_sceneFlags & SceneFlags::FOREGROUND);
    }

    HYP_METHOD(Property = "IsAudioListener", Transient)
    bool GetIsAudioListener() const
    {
        return m_sceneFlags & SceneFlags::AUDIO_LISTENER;
    }

    HYP_METHOD(Property = "IsAudioListener", Transient)
    void SetIsAudioListener(bool isAudioListener)
    {
        if (isAudioListener)
        {
            m_sceneFlags |= SceneFlags::AUDIO_LISTENER;
        }
        else
        {
            m_sceneFlags &= ~SceneFlags::AUDIO_LISTENER;
        }

        MarkDirty();
    }

    HYP_METHOD(Property = "StreamingCentroid")
    const Vec2i& GetStreamingCentroid() const
    {
        return m_streamingCentroid;
    }

    HYP_METHOD(Property = "StreamingCentroid")
    void SetStreamingCenroid(const Vec2i& streamingCentroid)
    {
        m_streamingCentroid = streamingCentroid;
        MarkDirty();
    }

    HYP_METHOD()
    bool AddToWorld(World* world);

    HYP_METHOD()
    bool RemoveFromWorld();

    void Update(float delta);

    HYP_FIELD()
    static ScriptableDelegate<void, Handle<Node>, Handle<Node>> OnRootNodeChanged;

private:
    template <class SystemType>
    void AddSystemIfApplicable();

    HYP_FIELD(Property = "SceneFlags")
    EnumFlags<SceneFlags> m_sceneFlags;

    HYP_FIELD(Property = "Root", Editor = false)
    Handle<Node> m_root;

    HYP_FIELD(Property = "OwnerThreadId", Transient, Editor = false)
    ThreadId m_ownerThreadId;

    HYP_FIELD(Property = "World", Transient, Editor = false)
    World* m_world;

    HYP_FIELD(Property = "EntityManager", Transient = true, Editor = false)
    Handle<EntityManager> m_entityManager;

    SceneOctree m_octree;

    HYP_FIELD(Property = "PreviousDelta", Transient, Editor = false)
    float m_previousDelta;

    HYP_FIELD(Property = "StreamingCentroid", Serialize = true, Editor = true)
    Vec2i m_streamingCentroid;

    bool m_isInitialized;
};

} // namespace Hyperion
