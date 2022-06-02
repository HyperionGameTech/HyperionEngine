#ifndef HYPERION_V2_SHADER_H
#define HYPERION_V2_SHADER_H

#include "base.h"
#include "atomics.h"
#include "buffers.h"
#include "bindless.h"

#include <rendering/backend/renderer_shader.h>
#include <rendering/backend/renderer_buffer.h>
#include <rendering/backend/renderer_structs.h>

#include <math/transform.h>

#include <hash_code.h>

#include <memory>
#include <utility>
#include <string>

namespace hyperion::v2 {

using renderer::ShaderProgram;
using renderer::ShaderObject;
using renderer::ShaderModule;
using renderer::GPUBuffer;
using renderer::UniformBuffer;
using renderer::StorageBuffer;
using renderer::PerFrameData;

class Engine;

struct ShaderGlobals {
    ShaderGlobals(size_t num_buffers)
        : scenes(num_buffers),
          lights(num_buffers),
          objects(num_buffers),
          materials(num_buffers),
          skeletons(num_buffers),
          shadow_maps(num_buffers)
    {
    }

    ShaderGlobals(const ShaderGlobals &other) = delete;
    ShaderGlobals &operator=(const ShaderGlobals &other) = delete;

    void Create(Engine *engine);
    void Destroy(Engine *engine);

    ShaderData<StorageBuffer, SceneShaderData, max_scenes>        scenes;
    ShaderData<StorageBuffer, LightShaderData, max_lights>        lights;
    ShaderData<StorageBuffer, ObjectShaderData, max_objects>      objects;
    ShaderData<StorageBuffer, MaterialShaderData, max_materials>  materials;
    ShaderData<StorageBuffer, SkeletonShaderData, max_skeletons>  skeletons;
    ShaderData<UniformBuffer, ShadowShaderData, max_shadow_maps>  shadow_maps;
    BindlessStorage                                               textures;
};

struct SubShader {
    ShaderModule::Type type;
    ShaderObject       spirv;
};

class Shader : public EngineComponentBase<STUB_CLASS(Shader)> {
public:
    Shader(const std::vector<SubShader> &sub_shaders);
    Shader(const Shader &) = delete;
    Shader &operator=(const Shader &) = delete;
    ~Shader();

    ShaderProgram *GetShaderProgram() const { return m_shader_program.get(); }

    void Init(Engine *engine);

private:
    std::unique_ptr<ShaderProgram> m_shader_program;
    std::vector<SubShader>         m_sub_shaders;
};

enum class ShaderKey {
    BASIC_FORWARD,
    BASIC_VEGETATION,
    BASIC_SKYBOX,
    STENCIL_OUTLINE,
    CUSTOM
};

struct ShaderMapKey {
    ShaderKey   key;
    std::string name;

    bool operator==(const ShaderMapKey &other) const
        { return key == other.key && name == other.name; }

    bool operator<(const ShaderMapKey &other) const
        { return std::tie(key, name) < std::tie(other.key, other.name); }

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(int(key));
        hc.Add(name);

        return hc;
    }
};

} // namespace hyperion::v2

HYP_DEF_STL_HASH(hyperion::v2::ShaderMapKey);

#endif // !HYPERION_V2_SHADER_H

