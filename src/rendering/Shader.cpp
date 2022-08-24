#include "Shader.hpp"
#include "../Engine.hpp"

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
    env_probes.Create(device);
    textures.Create(engine);

    cubemap_uniforms.Create(device, sizeof(CubemapUniforms));
}

void ShaderGlobals::Destroy(Engine *engine)
{
    auto *device = engine->GetDevice();

    cubemap_uniforms.Destroy(device);
    env_probes.Destroy(device);

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

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_SHADERS, [this](...) {
        auto *engine = GetEngine();

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

        OnTeardown([this]() {
            auto *engine = GetEngine();

            SetReady(false);

            engine->render_scheduler.Enqueue([this, engine](...) {
                return m_shader_program->Destroy(engine->GetDevice());
            });
            
            HYP_FLUSH_RENDER_QUEUE(engine);
        });
    }));
}

} // namespace hyperion
