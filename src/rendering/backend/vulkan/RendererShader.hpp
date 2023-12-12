#ifndef HYPERION_RENDERER_BACKEND_VULKAN_SHADER_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_SHADER_HPP


#include <core/lib/ByteBuffer.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/String.hpp>

#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RendererDevice.hpp>

#include <asset/ByteReader.hpp>

#include <HashCode.hpp>
#include <Types.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

template <>
struct ShaderModule<Platform::VULKAN>
{
    ShaderModuleType    type;
    String              entry_point_name;
    ByteBuffer          spirv;
    VkShaderModule      shader_module;

    ShaderModule(ShaderModuleType type, String entry_point_name)
        : type(type),
          entry_point_name(std::move(entry_point_name)),
          spirv { },
          shader_module { }
    {
    }

    ShaderModule(ShaderModuleType type, String entry_point_name, const ByteBuffer &spirv, VkShaderModule shader_module = nullptr)
        : type(type),
          entry_point_name(std::move(entry_point_name)),
          spirv(spirv),
          shader_module(shader_module)
    {
    }

    ShaderModule(const ShaderModule &other) = default;
    ~ShaderModule() = default;

    bool operator<(const ShaderModule &other) const
        { return type < other.type; }

    bool IsRaytracing() const
        { return IsRaytracingType(type); }

    static bool IsRaytracingType(ShaderModuleType type)
    {
        return type == ShaderModuleType::RAY_GEN
            || type == ShaderModuleType::RAY_INTERSECT
            || type == ShaderModuleType::RAY_ANY_HIT
            || type == ShaderModuleType::RAY_CLOSEST_HIT
            || type == ShaderModuleType::RAY_MISS;
    }
};

template <>
struct ShaderGroup<Platform::VULKAN>
{
    ShaderModuleType type;
    VkRayTracingShaderGroupCreateInfoKHR raytracing_group_create_info;
};

template <>
class ShaderProgram<Platform::VULKAN>
{
public:
    ShaderProgram();
    ShaderProgram(String entry_point_name);
    ShaderProgram(const ShaderProgram &other)               = delete;
    ShaderProgram &operator=(const ShaderProgram &other)    = delete;
    ~ShaderProgram();

    const Array<ShaderModule<Platform::VULKAN>> &GetShaderModules() const
        { return m_shader_modules; }

    Array<VkPipelineShaderStageCreateInfo> &GetShaderStages()
        { return m_shader_stages; }

    const Array<VkPipelineShaderStageCreateInfo> &GetShaderStages() const
        { return m_shader_stages; }

    /* For raytracing only */
    const Array<ShaderGroup<Platform::VULKAN>> &GetShaderGroups() const
        { return m_shader_groups; }

    bool IsRaytracing() const
    {
        return m_shader_modules.Any([](const ShaderModule<Platform::VULKAN> &it) {
            return it.IsRaytracing();
        });
    }

    Result AttachShader(Device<Platform::VULKAN> *device, ShaderModuleType type, const ShaderObject &shader_object);

    Result Create(Device<Platform::VULKAN> *device);
    Result Destroy(Device<Platform::VULKAN> *device);

    HashCode GetHashCode() const
    {
        HashCode hc;

        for (const auto &shader_module : m_shader_modules) {
            hc.Add(static_cast<Int>(shader_module.type));
            hc.Add(shader_module.spirv.GetHashCode());
        }

        return hc;
    }

private:
    VkPipelineShaderStageCreateInfo CreateShaderStage(const ShaderModule<Platform::VULKAN> &);
    Result CreateShaderGroups();

    String                                  m_entry_point_name;

    Array<ShaderModule<Platform::VULKAN>>   m_shader_modules;

    Array<VkPipelineShaderStageCreateInfo>  m_shader_stages;
    Array<ShaderGroup<Platform::VULKAN>>    m_shader_groups;
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#endif //HYPERION_RENDERER_BACKEND_VULKAN_SHADER_HPP
