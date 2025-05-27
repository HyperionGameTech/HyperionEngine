/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_SHADER_HPP
#define HYPERION_BACKEND_RENDERER_SHADER_HPP

#include <core/memory/ByteBuffer.hpp>
#include <core/memory/RefCountedPtr.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/String.hpp>

#include <core/algorithm/AnyOf.hpp>

#include <core/Defines.hpp>

#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererDevice.hpp>

#include <HashCode.hpp>
#include <Types.hpp>

namespace hyperion {

struct CompiledShader;

namespace renderer {

struct ShaderObject
{
    ByteBuffer bytes;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(bytes);

        return hc;
    }
};

enum ShaderModuleType : uint32
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

static inline bool IsRaytracingShaderModule(ShaderModuleType type)
{
    return type == ShaderModuleType::RAY_GEN
        || type == ShaderModuleType::RAY_INTERSECT
        || type == ShaderModuleType::RAY_ANY_HIT
        || type == ShaderModuleType::RAY_CLOSEST_HIT
        || type == ShaderModuleType::RAY_MISS;
}

class ShaderBase : public RenderObject<ShaderBase>
{
public:
    HYP_API virtual ~ShaderBase() override = default;

    HYP_FORCE_INLINE const RC<CompiledShader>& GetCompiledShader() const
    {
        return m_compiled_shader;
    }

    HYP_API virtual bool IsCreated() const = 0;

    HYP_API virtual RendererResult Create() = 0;
    HYP_API virtual RendererResult Destroy() = 0;

protected:
    ShaderBase(const RC<CompiledShader>& compiled_shader)
        : m_compiled_shader(compiled_shader)
    {
    }

    RC<CompiledShader> m_compiled_shader;
};

} // namespace renderer
} // namespace hyperion

#endif