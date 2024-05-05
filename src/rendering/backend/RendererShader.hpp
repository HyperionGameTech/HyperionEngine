/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_SHADER_HPP
#define HYPERION_BACKEND_RENDERER_SHADER_HPP

#include <core/memory/ByteBuffer.hpp>
#include <core/containers/Array.hpp>
#include <core/containers/String.hpp>
#include <core/memory/RefCountedPtr.hpp>
#include <core/Defines.hpp>

#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RendererDevice.hpp>

#include <HashCode.hpp>
#include <Types.hpp>

namespace hyperion {

class CompiledShader;

namespace renderer {

struct ShaderObject
{
    ByteBuffer bytes;

    HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(bytes);

        return hc;
    }
};

enum ShaderModuleType : uint
{
    UNSET = 0,

    /* Graphics and general purpose shaders */
    VERTEX,
    FRAGMENT,
    GEOMETRY,
    COMPUTE,

    /* Mesh shaders */
    TASK,
    MESH,

    /* Tesselation */
    TESS_CONTROL,
    TESS_EVAL,

    /* Raytracing hardware specific */
    RAY_GEN,
    RAY_INTERSECT,
    RAY_ANY_HIT,
    RAY_CLOSEST_HIT,
    RAY_MISS,

    MAX
};

namespace platform {

template <PlatformType PLATFORM>
struct ShaderModule;

template <PlatformType PLATFORM>
struct ShaderGroup;

template <PlatformType PLATFORM>
struct ShaderPlatformImpl;

template <PlatformType PLATFORM>
class Shader
{
public:
    HYP_API Shader();
    HYP_API Shader(const RC<CompiledShader> &compiled_shader);
    Shader(const Shader &other)               = delete;
    Shader &operator=(const Shader &other)    = delete;
    HYP_API ~Shader();

    [[nodiscard]]
    HYP_FORCE_INLINE
    ShaderPlatformImpl<PLATFORM> &GetPlatformImpl()
        { return m_platform_impl; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    const ShaderPlatformImpl<PLATFORM> &GetPlatformImpl() const
        { return m_platform_impl; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    const String &GetEntryPointName() const
        { return m_entry_point_name; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    const Array<ShaderModule<PLATFORM>> &GetShaderModules() const
        { return m_shader_modules; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    const Array<ShaderGroup<PLATFORM>> &GetShaderGroups() const
        { return m_shader_groups; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool IsRaytracing() const
    {
        return m_shader_modules.Any([](const ShaderModule<PLATFORM> &it)
        {
            return it.IsRaytracing();
        });
    }

    HYP_API bool IsCreated() const;

    [[nodiscard]]
    HYP_FORCE_INLINE
    const RC<CompiledShader> &GetCompiledShader() const
        { return m_compiled_shader; }

    HYP_API void SetCompiledShader(const RC<CompiledShader> &compiled_shader);

    HYP_API Result Create(Device<PLATFORM> *device);
    HYP_API Result Destroy(Device<PLATFORM> *device);

    [[nodiscard]]
    HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        HashCode hc;

        for (const ShaderModule<PLATFORM> &shader_module : m_shader_modules) {
            hc.Add(uint(shader_module.type));
            hc.Add(shader_module.spirv.GetHashCode());
        }

        return hc;
    }

private:
    Result AttachSubShaders();
    Result AttachSubShader(Device<PLATFORM> *device, ShaderModuleType type, const ShaderObject &shader_object);

    Result CreateShaderGroups();

    ShaderPlatformImpl<PLATFORM> m_platform_impl;

    RC<CompiledShader>                  m_compiled_shader;

    String                              m_entry_point_name;

    Array<ShaderModule<PLATFORM>>       m_shader_modules;
    Array<ShaderGroup<PLATFORM>>        m_shader_groups;
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererShader.hpp>
#else
#error Unsupported rendering backend
#endif

namespace hyperion {
namespace renderer {

using Shader = platform::Shader<Platform::CURRENT>;
using ShaderModule = platform::ShaderModule<Platform::CURRENT>;
using ShaderGroup = platform::ShaderGroup<Platform::CURRENT>;

} // namespace renderer
} // namespace hyperion

#endif