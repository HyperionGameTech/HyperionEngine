#include "scene.h"
#include <engine.h>
#include <rendering/environment.h>

namespace hyperion::v2 {

Scene::Scene(std::unique_ptr<Camera> &&camera)
    : EngineComponentBase(),
      m_camera(std::move(camera)),
      m_root_node(std::make_unique<Node>("root")),
      m_environment(new Environment),
      m_shader_data_state(ShaderDataState::DIRTY)
{
    m_root_node->SetScene(this);
}

Scene::~Scene()
{
    m_root_node->SetScene(nullptr);

    Teardown();

    delete m_environment;
}
    
void Scene::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }
    
    EngineComponentBase::Init(engine);

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_SCENES, [this](Engine *engine) {
        for (auto &texture : m_environment_textures) {
            if (texture != nullptr) {
                texture.Init();
            }
        }

        m_environment->Init(engine);

        SetReady(true);

        //EnqueueRenderUpdates(engine);


        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_SCENES, [this](Engine *engine) {
            DebugLog(LogType::Debug, "Destroy scene #%lu\t%p\n", m_id.value, (void *)this);

            delete m_environment;
            m_environment = nullptr;

            HYP_FLUSH_RENDER_QUEUE(engine);

            SetReady(false);
        }), engine);
    }));
}

void Scene::Update(Engine *engine, GameCounter::TickUnit delta)
{
    AssertReady();

    m_environment->Update(engine, delta);

    if (m_camera != nullptr) {
        m_camera->Update(delta);

        if (m_camera->GetViewProjectionMatrix() != m_last_view_projection_matrix) {
            m_last_view_projection_matrix = m_camera->GetViewProjectionMatrix();
            m_shader_data_state |= ShaderDataState::DIRTY;
        }
    }

    //if (m_shader_data_state.IsDirty()) {
        EnqueueRenderUpdates(engine);
    //}

    m_root_node->Update(engine, delta);
}

void Scene::EnqueueRenderUpdates(Engine *engine)
{
    struct {
        BoundingBox aabb;
        Matrix4     view;
        Matrix4     projection;
        Vector3     translation;
        Vector3     direction;
        int         width;
        int         height;
        Vector3     light_direction;
        float       global_timer;
    } params = {
        .aabb            = m_aabb,
        .light_direction = Vector3(-0.5f, 0.5f, 0.0f).Normalize(),
        .global_timer    = m_environment->GetGlobalTimer()
    };

    if (m_camera != nullptr) {
        params.view        = m_camera->GetViewMatrix();
        params.projection  = m_camera->GetProjectionMatrix();
        params.translation = m_camera->GetTranslation();
        params.direction   = m_camera->GetDirection();
        params.width       = m_camera->GetWidth();
        params.height      = m_camera->GetHeight();
    }

    engine->render_scheduler.EnqueueReplace(m_render_update_id, [this, engine, params](...) {
        SceneShaderData shader_data{
            .view             = params.view,
            .projection       = params.projection,
            .camera_position  = params.translation.ToVector4(),
            .camera_direction = params.direction.ToVector4(),
            .light_direction  = params.light_direction.ToVector4(),
            .resolution_x     = static_cast<uint32_t>(params.width),
            .resolution_y     = static_cast<uint32_t>(params.height),
            .aabb_max         = params.aabb.max.ToVector4(),
            .aabb_min         = params.aabb.min.ToVector4(),
            .global_timer     = params.global_timer
        };

        shader_data.environment_texture_usage = 0;

        for (uint32_t i = 0; i < static_cast<uint32_t>(m_environment_textures.size()); i++) {
            if (auto &texture = m_environment_textures[i]) {
                shader_data.environment_texture_index = texture->GetId().value - 1;
                shader_data.environment_texture_usage |= 1 << i;
            }
        }
        
        engine->shader_globals->scenes.Set(m_id.value - 1, shader_data);

        HYPERION_RETURN_OK;
    });

    m_shader_data_state = ShaderDataState::CLEAN;
}

void Scene::SetEnvironmentTexture(uint32_t index, Ref<Texture> &&texture)
{
    if (texture && IsReady()) {
        texture.Init();
    }

    m_environment_textures[index] = std::move(texture);

    m_shader_data_state |= ShaderDataState::DIRTY;
}

} // namespace hyperion::v2
