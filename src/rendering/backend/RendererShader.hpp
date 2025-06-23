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
    SMT_UNSET = 0,

    /* Graphics and general purpose shaders */
    SMT_VERTEX,
    SMT_FRAGMENT,
    SMT_GEOMETRY,
    SMT_COMPUTE,

    /* Mesh shaders */
    SMT_TASK,
    SMT_MESH,

    /* Tesselation */
    SMT_TESS_CONTROL,
    SMT_TESS_EVAL,

    /* Raytracing hardware specific */
    SMT_RAY_GEN,
    SMT_RAY_INTERSECT,
    SMT_RAY_ANY_HIT,
    SMT_RAY_CLOSEST_HIT,
    SMT_RAY_MISS,

    SMT_MAX
};

static inline bool IsRaytracingShaderModule(ShaderModuleType type)
{
    return type == SMT_RAY_GEN
        || type == SMT_RAY_INTERSECT
        || type == SMT_RAY_ANY_HIT
        || type == SMT_RAY_CLOSEST_HIT
        || type == SMT_RAY_MISS;
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

} // namespace hyperion

#endif