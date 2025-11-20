/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Name.hpp>
#include <core/Types.hpp>

#include <core/math/Color.hpp>

#include <core/utilities/Uuid.hpp>
#include <core/utilities/DataMutationState.hpp>

#include <core/logging/LoggerFwd.hpp>

#include <scene/Node.hpp>
#include <scene/Entity.hpp>
#include <scene/SceneOctree.hpp>

#include <scene/camera/Camera.hpp>

#include <rendering/RenderObject.hpp>

#include <asset/AssetObject.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Scene);

class RenderEnvironment;
class World;
class Scene;
class EntityManager;
class WorldGrid;

struct FogParams
{
    Color color = Color(0xF2F8F7FF);
    float startDistance = 250.0f;
    float endDistance = 1000.0f;
};

HYP_ENUM()
enum class SceneFlags : uint32
{
    NONE = 0x0,

    FOREGROUND = 0x1,                   //!< Scene is a foreground scene (i.e., it is rendered normally).
    DETACHED = 0x2,                     //!< Scene is not attached to any World.
    UI = 0x8,                           //!< Scene is created for UI (see UIStage).
    EDITOR = 0x10,                      //!< Scene is an editor-owned scene.

    STREAMED = 0x20,                    //!< Allow streaming the scene in and out of the World dynamically, based on StreamingVolume proximity.
    HAS_OCTREE = 0x40,                  //!< Scene uses an octree for spatial partitioning.
    PARALLEL_SYSTEM_EXECUTION = 0x80,   //!< Enable asynchronous System processing for the Scene's EntityManager. Useful when there are lots of Systems but may add overhead if there are few

    DEFAULT = FOREGROUND | STREAMED | HAS_OCTREE | PARALLEL_SYSTEM_EXECUTION
};

HYP_MAKE_ENUM_FLAGS(SceneFlags);

class SceneValidationError final : public Error
{
public:
    SceneValidationError()
        : Error()
    {
    }

    template <auto MessageString, class... Args>
    SceneValidationError(const StaticMessage& currentFunction, ValueWrapper<MessageString>, Args&&... args)
        : Error(currentFunction, ValueWrapper<MessageString>(), std::forward<Args>(args)...)
    {
    }

    ~SceneValidationError() = default;
};

using SceneValidationResult = TResult<void, SceneValidationError>;

class HYP_API SceneValidation
{
public:
    static SceneValidationResult ValidateScene(const Scene* scene);
};

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
    HYP_FORCE_INLINE EnumFlags<SceneFlags> GetSceneFlags() const
    {
        return m_sceneFlags;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE void SetSceneFlags(EnumFlags<SceneFlags> flags)
    {
        m_sceneFlags = flags;
    }

    HYP_METHOD()
    HYP_NODISCARD Handle<Node> FindNodeByName(StringHash name) const;

    HYP_METHOD(Property = "Root", Editor = true)
    HYP_FORCE_INLINE const Handle<Node>& GetRoot() const
    {
        return m_root;
    }

    HYP_METHOD(Property = "Root", Editor = true)
    void SetRoot(const Handle<Node>& root);

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<EntityManager>& GetEntityManager() const
    {
        return m_entityManager;
    }

    HYP_FORCE_INLINE SceneOctree& GetOctree()
    {
        return m_octree;
    }

    HYP_FORCE_INLINE const SceneOctree& GetOctree() const
    {
        return m_octree;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsAttachedToWorld() const
    {
        return m_world != nullptr;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE World* GetWorld() const
    {
        return m_world;
    }

    HYP_METHOD()
    void SetWorld(World* world);

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsForegroundScene() const
    {
        return m_sceneFlags & SceneFlags::FOREGROUND;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsBackgroundScene() const
    {
        return !(m_sceneFlags & SceneFlags::FOREGROUND);
    }

    HYP_METHOD()
    HYP_FORCE_INLINE bool GetIsAudioListener() const
    {
        return m_isAudioListener;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE void SetIsAudioListener(bool isAudioListener)
    {
        m_isAudioListener = isAudioListener;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE const Vec2i& GetStreamingCentroid() const
    {
        return m_streamingCentroid;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE void SetStreamingCenroid(const Vec2i& streamingCentroid)
    {
        m_streamingCentroid = streamingCentroid;
    }

    void Update(float delta);

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

    Delegate<void, const Handle<Node>& /* new */, const Handle<Node>& /* prev */> OnRootNodeChanged;

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

    FogParams m_fogParams;

    HYP_FIELD(Property = "EntityManager", Transient)
    Handle<EntityManager> m_entityManager;

    SceneOctree m_octree;

    HYP_FIELD(Property = "IsAudioListener")
    bool m_isAudioListener;

    HYP_FIELD(Property = "PreviousDelta", Transient)
    float m_previousDelta;

    HYP_FIELD(Property = "StreamingCentroid")
    Vec2i m_streamingCentroid;
};

} // namespace hyperion
