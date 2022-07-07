#ifndef HYPERION_V2_SCENE_H
#define HYPERION_V2_SCENE_H

#include "Node.hpp"
#include "Spatial.hpp"
#include "Octree.hpp"
#include <rendering/Base.hpp>
#include <rendering/Texture.hpp>
#include <rendering/Shader.hpp>
#include <rendering/Light.hpp>
#include <core/Scheduler.hpp>
#include <core/lib/FlatSet.hpp>
#include <core/lib/FlatMap.hpp>
#include <camera/Camera.hpp>
#include <GameCounter.hpp>
#include <Types.hpp>
#include <unordered_map>

namespace hyperion::v2 {

class Environment;
class World;

class Scene : public EngineComponentBase<STUB_CLASS(Scene)> {
    friend class Spatial; // TODO: refactor to not need as many friend classes
    friend class World;
public:
    static constexpr UInt32 max_environment_textures = SceneShaderData::max_environment_textures;

    Scene(std::unique_ptr<Camera> &&camera);
    Scene(const Scene &other) = delete;
    Scene &operator=(const Scene &other) = delete;
    ~Scene();

    Camera *GetCamera() const                            { return m_camera.get(); }
    void SetCamera(std::unique_ptr<Camera> &&camera)     { m_camera = std::move(camera); }

    bool AddSpatial(Ref<Spatial> &&spatial);
    bool HasSpatial(Spatial::ID id) const;
    bool RemoveSpatial(Spatial::ID id);
    bool RemoveSpatial(const Ref<Spatial> &spatial);

    /*! ONLY CALL FROM GAME THREAD!!! */
    auto &GetSpatials()                                  { return m_spatials; }
    const auto &GetSpatials() const                      { return m_spatials; }

    Node *GetRootNode() const                            { return m_root_node.get(); }

    Environment *GetEnvironment() const                  { return m_environment; }

    World *GetWorld() const                              { return m_world; }
    void SetWorld(World *world);

    Scene::ID GetParentId() const                        { return m_parent_id; }
    void SetParentId(Scene::ID id)                       { m_parent_id = id; }
    
    void Init(Engine *engine);

    BoundingBox m_aabb;

private:
    // World only calls
    void Update(
        Engine *engine,
        GameCounter::TickUnit delta,
        bool update_octree_visiblity = true
    );

    void EnqueueRenderUpdates(Engine *engine);

    void RequestPipelineChanges(Ref<Spatial> &spatial);
    void RemoveFromPipeline(Ref<Spatial> &spatial, GraphicsPipeline *pipeline);
    void RemoveFromPipelines(Ref<Spatial> &spatial);

    std::unique_ptr<Camera>  m_camera;
    std::unique_ptr<Node>    m_root_node;
    Environment             *m_environment;
    World                   *m_world;
    std::array<Ref<Texture>, max_environment_textures> m_environment_textures;

    // spatials live in GAME thread
    std::unordered_map<IDBase, Ref<Spatial>> m_spatials;

    Matrix4                 m_last_view_projection_matrix;

    ScheduledFunctionId     m_render_update_id;

    Scene::ID               m_parent_id;

    mutable ShaderDataState m_shader_data_state;
};

} // namespace hyperion::v2

#endif
