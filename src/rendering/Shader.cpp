#include "Shader.hpp"
#include "../Engine.hpp"

namespace hyperion::v2 {

using renderer::Result;

#pragma region Render commands

struct RENDER_COMMAND(CreateShaderProgram) : RenderCommand
{
    ShaderProgramRef shader_program;
    CompiledShader compiled_shader;

    RENDER_COMMAND(CreateShaderProgram)(
        const ShaderProgramRef &shader_program,
        const CompiledShader &compiled_shader
    ) : shader_program(shader_program),
        compiled_shader(compiled_shader)
    {
    }

    virtual ~RENDER_COMMAND(CreateShaderProgram)() = default;

    virtual Result operator()()
    {
        if (!compiled_shader.IsValid()) {
            return { Result::RENDERER_ERR, "Invalid compiled shader" };
        }

        for (SizeType index = 0; index < compiled_shader.modules.Size(); index++) {
            const auto &byte_buffer = compiled_shader.modules[index];

            if (byte_buffer.Empty()) {
                continue;
            }

            HYPERION_BUBBLE_ERRORS(shader_program->AttachShader(
                Engine::Get()->GetGPUInstance()->GetDevice(),
                ShaderModule::Type(index),
                { byte_buffer }
            ));
        }

        return shader_program->Create(Engine::Get()->GetGPUDevice());
    }
};

struct RENDER_COMMAND(DestroyShaderProgram) : RenderCommand
{
    ShaderProgramRef shader_program;

    RENDER_COMMAND(DestroyShaderProgram)(const ShaderProgramRef &shader_program)
        : shader_program(shader_program)
    {
    }

    virtual Result operator()()
    {
        return shader_program->Destroy(Engine::Get()->GetGPUDevice());
    }
};

struct RENDER_COMMAND(CreateGlobalSphericalHarmonicsGridImages) : RenderCommand
{
    FixedArray<GlobalSphericalHarmonicsGrid::GridTexture, 9> grid_textures;
    
    RENDER_COMMAND(CreateGlobalSphericalHarmonicsGridImages)(const FixedArray<GlobalSphericalHarmonicsGrid::GridTexture, 9> &grid_textures)
        : grid_textures(grid_textures)
    {
    }

    virtual Result operator()()
    {
        for (auto &item : grid_textures) {
            HYPERION_BUBBLE_ERRORS(item.image->Create(Engine::Get()->GetGPUDevice()));
            HYPERION_BUBBLE_ERRORS(item.image_view->Create(Engine::Get()->GetGPUDevice(), item.image));
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateGlobalSphericalHarmonicsClipmaps) : RenderCommand
{
    FixedArray<GlobalSphericalHarmonicsGrid::GridTexture, 9> grid_textures;
    
    RENDER_COMMAND(CreateGlobalSphericalHarmonicsClipmaps)(const FixedArray<GlobalSphericalHarmonicsGrid::GridTexture, 9> &grid_textures)
        : grid_textures(grid_textures)
    {
    }

    virtual Result operator()()
    {
        for (auto &item : grid_textures) {
            HYPERION_BUBBLE_ERRORS(item.image->Create(Engine::Get()->GetGPUDevice()));
            HYPERION_BUBBLE_ERRORS(item.image_view->Create(Engine::Get()->GetGPUDevice(), item.image));
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion

GlobalSphericalHarmonicsGrid::GlobalSphericalHarmonicsGrid()
{
    {
        const UInt dimension_cube = 32;//UInt(MathUtil::Ceil(std::cbrt(max_bound_ambient_probes)));
        const Extent3D image_dimensions { dimension_cube, dimension_cube, dimension_cube };

        UByte *empty_image_bytes = new UByte[image_dimensions.Size() * sizeof(Float) * 4];
        Memory::Set(empty_image_bytes, 0, image_dimensions.Size() * sizeof(Float) * 4);

        for (auto &item : textures) {
            item.image = RenderObjects::Make<Image>(StorageImage(
                image_dimensions,
                InternalFormat::RGBA16F,
                ImageType::TEXTURE_TYPE_3D,
                FilterMode::TEXTURE_FILTER_LINEAR,
                empty_image_bytes
            ));

            item.image_view = RenderObjects::Make<ImageView>();
        }

        delete[] empty_image_bytes;
    }

    { // clipmaps
        const Extent3D image_dimensions { 32, 32, 8 };

        UByte *empty_image_bytes = new UByte[image_dimensions.Size() * sizeof(Float) * 4];
        Memory::Set(empty_image_bytes, 0, image_dimensions.Size() * sizeof(Float) * 4);

        for (auto &item : clipmaps) {
            item.image = RenderObjects::Make<Image>(StorageImage(
                image_dimensions,
                InternalFormat::RGBA16F,
                ImageType::TEXTURE_TYPE_3D,
                FilterMode::TEXTURE_FILTER_LINEAR,
                empty_image_bytes
            ));

            item.image_view = RenderObjects::Make<ImageView>();
        }

        delete[] empty_image_bytes;
    }
}

void GlobalSphericalHarmonicsGrid::Create()
{
    PUSH_RENDER_COMMAND(CreateGlobalSphericalHarmonicsGridImages, textures);
    PUSH_RENDER_COMMAND(CreateGlobalSphericalHarmonicsClipmaps, clipmaps);
}

void GlobalSphericalHarmonicsGrid::Destroy()
{
    for (auto &item : textures) {
        SafeRelease(std::move(item.image));
        SafeRelease(std::move(item.image_view));
    }

    for (auto &item : clipmaps) {
        SafeRelease(std::move(item.image));
        SafeRelease(std::move(item.image_view));
    }
}

ShaderGlobals::ShaderGlobals()
{
    
}

void ShaderGlobals::Create()
{
    auto *device = Engine::Get()->GetGPUDevice();

    scenes.Create(device);
    materials.Create(device);
    objects.Create(device);
    skeletons.Create(device);
    lights.Create(device);
    shadow_maps.Create(device);
    env_probes.Create(device);
    env_grids.Create(device);
    immediate_draws.Create(device);
    entity_instance_batches.Create(device);

    textures.Create();

    spherical_harmonics_grid.Create();
}

void ShaderGlobals::Destroy()
{
    auto *device = Engine::Get()->GetGPUDevice();
    
    env_probes.Destroy(device);
    env_grids.Destroy(device);

    scenes.Destroy(device);
    objects.Destroy(device);
    materials.Destroy(device);
    skeletons.Destroy(device);
    lights.Destroy(device);
    shadow_maps.Destroy(device);
    immediate_draws.Destroy(device);
    entity_instance_batches.Destroy(device);

    spherical_harmonics_grid.Destroy();
}

Shader::Shader()
    : EngineComponentBase(),
      m_shader_program(RenderObjects::Make<ShaderProgram>())
{
}

Shader::Shader(const CompiledShader &compiled_shader)
    : EngineComponentBase(),
      m_compiled_shader(compiled_shader),
      m_shader_program(RenderObjects::Make<ShaderProgram>())
{
}

Shader::~Shader()
{
    if (IsInitCalled()) {
        SetReady(false);
    }

    SafeRelease(std::move(m_shader_program));
}

void Shader::Init()
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init();

    if (!m_compiled_shader.IsValid()) {
        return;
    }

    PUSH_RENDER_COMMAND(
        CreateShaderProgram,
        m_shader_program,
        m_compiled_shader
    );

    SetReady(true);
}

} // namespace hyperion
