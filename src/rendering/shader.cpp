#include "shader.h"
#include "../engine.h"

namespace hyperion::v2 {

void ShaderGlobals::Create(Engine *engine)
{
    auto *device = engine->GetDevice();

    scenes.Create(device);
    materials.Create(device);
    objects.Create(device);
    skeletons.Create(device);
    lights.Create(device);
    shadow_maps.Create(device);
    textures.Create(engine);
}

void ShaderGlobals::Destroy(Engine *engine)
{
    auto *device = engine->GetDevice();

    scenes.Destroy(device);
    objects.Destroy(device);
    materials.Destroy(device);
    skeletons.Destroy(device);
    lights.Destroy(device);
    shadow_maps.Destroy(device);
}

Shader::Shader(const std::vector<SubShader> &sub_shaders)
    : EngineComponentBase(),
      m_shader_program(std::make_unique<ShaderProgram>()),
      m_sub_shaders(sub_shaders)
{
}

Shader::~Shader()
{
    Teardown();
}

void Shader::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init(engine);

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_SHADERS, [this](Engine *engine) {
        for (const auto &sub_shader : m_sub_shaders) {
            AssertThrowMsg(!sub_shader.spirv.bytes.empty(), "Shader data missing");
        }

        engine->render_scheduler.Enqueue([this, engine](...) {
            for (const auto &sub_shader : m_sub_shaders) {
                HYPERION_BUBBLE_ERRORS(
                    m_shader_program->AttachShader(
                        engine->GetInstance()->GetDevice(),
                        sub_shader.type,
                        sub_shader.spirv
                    )
                );
            }

            HYPERION_BUBBLE_ERRORS(m_shader_program->Create(engine->GetDevice()));
            
            SetReady(true);
            
            HYPERION_RETURN_OK;
        });

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_SHADERS, [this](Engine *engine) {
            engine->render_scheduler.Enqueue([this, engine](...) {
                return m_shader_program->Destroy(engine->GetDevice());
            });
            
            HYP_FLUSH_RENDER_QUEUE(engine);

            SetReady(false);
        }), engine);
    }));
}

} // namespace hyperion
