/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/name/Name.hpp>
#include <Core/Types.hpp>

#include <Core/math/Color.hpp>

#include <Core/utilities/Uuid.hpp>
#include <Core/utilities/DataMutationState.hpp>

#include <scripting/ScriptableDelegate.hpp>

#include <Core/logging/LoggerFwd.hpp>

#include <scene/Node.hpp>
#include <scene/Entity.hpp>
#include <scene/SceneOctree.hpp>

#include <scene/camera/Camera.hpp>

#include <rendering/RenderObject.hpp>

#include <asset/AssetObject.hpp>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Scene);

class World;
class Scene;
class EntityManager;
class WorldGrid;

HYP_ENUM()
enum class SceneFlags : uint32
{
    NONE = 0x0,

    FOREGROUND = 0x1, //!< Scene is a foreground scene (i.e., it is rendered normally).
    DETACHED = 0x2,   //!< Scene is not attached to any World.
    UI = 0x8,         //!< Scene is created for UI (see UIStage).
    EDITOR = 0x10,    //!< Scene is an editor-owned scene.

    STREAMED = 0x20,   //!< Allow streaming the scene in and out of the World dynamically, based on StreamingVolume proximity.
    HAS_OCTREE = 0x40, //!< Scene uses an octree for spatial partitioning.

    AUDIO_LISTENER = 0x80, //!< Scene has an audio listener (only one scene in a world should have this flag set)

    DEFAULT = FOREGROUND | STREAMED | HAS_OCTREE
};

HYP_MAKE_ENUM_FLAGS(SceneFlags);

extern void Scene_OnPostLoad(Scene& scene);

HYP_CLASS(PostLoad = "Scene_OnPostLoad")
class HYP_API Scene final : public AssetObject
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
    HYP_NODISCARD Handle<Node> FindNodeByName(StringHash name) const;

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

    /*! \brief Gets a unique name for a node in this scene. The returned name will be in the format "base_name(num)".
     *  The name will only be unique as long as another node with the same name does not exist in this scene. (no record is kept of the results from this function)
     *  \note The node name will be unique only to this scene.
     *  \param baseName The base name to use for the node name. */
    HYP_METHOD()
    Name GetUniqueNodeName(UTF8StringView baseName) const;

    void Update(float delta);

    HYP_FIELD()
    ScriptableDelegate<void, Handle<Node>, Handle<Node>> OnRootNodeChanged;

private:
    void Init() override;

    template <class SystemType>
    void AddSystemIfApplicable();

    HYP_FIELD(Property = "SceneFlags")
    EnumFlags<SceneFlags> m_sceneFlags;

    HYP_FIELD(Property = "Root")
    Handle<Node> m_root;

    HYP_FIELD(Property = "OwnerThreadId", Transient)
    ThreadId m_ownerThreadId;

    HYP_FIELD(Property = "World", Transient)
    World* m_world;

    HYP_FIELD(Property = "EntityManager", Transient)
    Handle<EntityManager> m_entityManager;

    SceneOctree m_octree;

    HYP_FIELD(Property = "PreviousDelta", Transient)
    float m_previousDelta;

    HYP_FIELD(Property = "StreamingCentroid")
    Vec2i m_streamingCentroid;
};

} // namespace Hyperion
