/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_SCENE_HPP
#define HYPERION_SCENE_HPP

#include <scene/Node.hpp>
#include <scene/Entity.hpp>
#include <scene/Octree.hpp>
#include <scene/camera/Camera.hpp>

#include <core/Base.hpp>
#include <core/Name.hpp>

#include <core/utilities/DataMutationState.hpp>

#include <core/logging/LoggerFwd.hpp>

#include <core/object/HypObject.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <core/math/Color.hpp>

#include <GameCounter.hpp>
#include <Types.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Scene);

class RenderEnvironment;
class World;
class Scene;
class EntityManager;
class WorldGrid;
class RenderScene;

struct FogParams
{
    Color color = Color(0xF2F8F7FF);
    float start_distance = 250.0f;
    float end_distance = 1000.0f;
};

HYP_ENUM()
enum class SceneFlags : uint32
{
    NONE = 0x0,
    FOREGROUND = 0x1,
    DETACHED = 0x2,
    UI = 0x8
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
    SceneValidationError(const StaticMessage& current_function, ValueWrapper<MessageString>, Args&&... args)
        : Error(current_function, ValueWrapper<MessageString>(), std::forward<Args>(args)...)
    {
    }

    virtual ~SceneValidationError() override = default;
};

using SceneValidationResult = TResult<void, SceneValidationError>;

class HYP_API SceneValidation
{
public:
    static SceneValidationResult ValidateScene(const Scene* scene);
};

HYP_CLASS()
class HYP_API Scene : public HypObject<Scene>
{
    friend class World;
    friend class UIStage;

    HYP_OBJECT_BODY(Scene);

public:
    Scene();

    Scene(EnumFlags<SceneFlags> flags);

    Scene(
        World* world,
        EnumFlags<SceneFlags> flags = SceneFlags::NONE);

    Scene(
        World* world,
        ThreadID owner_thread_id,
        EnumFlags<SceneFlags> flags = SceneFlags::NONE);

    Scene(const Scene& other) = delete;
    Scene& operator=(const Scene& other) = delete;
    ~Scene();

    HYP_FORCE_INLINE RenderScene& GetRenderResource() const
    {
        return *m_render_resource;
    }

    /*! \brief Get the thread ID that owns this Scene. */
    HYP_FORCE_INLINE ThreadID GetOwnerThreadID() const
    {
        return m_owner_thread_id;
    }

    /*! \brief Set the thread ID that owns this Scene.
     *  This is used to assert that the Scene is being accessed from the correct thread.
     *  \note Only call this if you know what you are doing. */
    void SetOwnerThreadID(ThreadID owner_thread_id);

    HYP_METHOD()
    const Handle<Camera>& GetPrimaryCamera() const;

    HYP_METHOD()
    HYP_FORCE_INLINE EnumFlags<SceneFlags> GetFlags() const
    {
        return m_flags;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE void SetFlags(EnumFlags<SceneFlags> flags)
    {
        m_flags = flags;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE Name GetName() const
    {
        return m_name;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE void SetName(Name name)
    {
        m_name = name;
    }

    HYP_METHOD()
    HYP_NODISCARD Handle<Node> FindNodeWithEntity(ID<Entity> entity) const;

    HYP_METHOD()
    HYP_NODISCARD Handle<Node> FindNodeByName(UTF8StringView name) const;

    HYP_METHOD(Property = "Root", Serialize = true, Editor = true)
    HYP_FORCE_INLINE const Handle<Node>& GetRoot() const
    {
        return m_root;
    }

    HYP_METHOD(Property = "Root", Serialize = true, Editor = true)
    void SetRoot(const Handle<Node>& root);

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<EntityManager>& GetEntityManager() const
    {
        return m_entity_manager;
    }

    HYP_FORCE_INLINE Octree& GetOctree()
    {
        return m_octree;
    }

    HYP_FORCE_INLINE const Octree& GetOctree() const
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

    void SetWorld(World* world);

    HYP_METHOD()
    HYP_FORCE_INLINE bool IsForegroundScene() const
    {
        return m_flags & SceneFlags::FOREGROUND;
    }

    HYP_FORCE_INLINE bool IsBackgroundScene() const
    {
        return !(m_flags & SceneFlags::FOREGROUND);
    }

    HYP_METHOD(Property = "IsAudioListener", Serialize = true)
    HYP_FORCE_INLINE bool IsAudioListener() const
    {
        return m_is_audio_listener;
    }

    HYP_METHOD(Property = "IsAudioListener", Serialize = true)
    HYP_FORCE_INLINE void SetIsAudioListener(bool is_audio_listener)
    {
        m_is_audio_listener = is_audio_listener;
    }

    void Init() override;

    void Update(GameCounter::TickUnit delta);

    HYP_METHOD()
    bool AddToWorld(World* world);

    HYP_METHOD()
    bool RemoveFromWorld();

    /*! \brief Gets a unique name for a node in this scene. The returned name will be in the format "base_name(num)".
     *  The name will only be unique as long as another node with the same name does not exist in this scene. (no record is kept of the results from this function)
     *  \note The node name will be unique only to this scene.
     *  \param base_name The base name to use for the node name. */
    HYP_METHOD()
    String GetUniqueNodeName(UTF8StringView base_name) const;

    Delegate<void, const Handle<Node>& /* new */, const Handle<Node>& /* prev */> OnRootNodeChanged;

private:
    template <class SystemType>
    void AddSystemIfApplicable();

    HYP_FIELD(Property = "Name", Serialize = true, Editor = true)
    Name m_name;

    HYP_FIELD(Property = "Flags", Serialize = true)
    EnumFlags<SceneFlags> m_flags;

    Handle<Node> m_root;

    ThreadID m_owner_thread_id;

    World* m_world;

    FogParams m_fog_params;

    Handle<EntityManager> m_entity_manager;

    Octree m_octree;

    bool m_is_audio_listener;

    GameCounter::TickUnit m_previous_delta;

    RenderScene* m_render_resource;
};

} // namespace hyperion

#endif
