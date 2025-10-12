/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once
#include <core/memory/ByteBuffer.hpp>
#include <core/memory/RefCountedPtr.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/String.hpp>

#include <core/Defines.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/RenderDevice.hpp>

#include <core/HashCode.hpp>
#include <core/Types.hpp>

namespace hyperion {

struct CompiledShader;

struct ShaderObject
{
    Name srcName;
    ByteBuffer bytes;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(srcName);
        hc.Add(bytes);

        return hc;
    }
};

HYP_ENUM()
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

HYP_CLASS(Abstract, NoScriptBindings)
class ShaderBase : public HypObjectBase
{
    HYP_OBJECT_BODY(ShaderBase);

public:
    ShaderBase() = default;
    virtual ~ShaderBase() override = default;

    HYP_FORCE_INLINE const RC<CompiledShader>& GetCompiledShader() const
    {
        return m_compiledShader;
    }

    virtual bool IsCreated() const = 0;

    virtual RendererResult Create() = 0;

    Name GetDebugName() const
    {
        return m_debugName;
    }

    virtual void SetDebugName(Name name)
    {
        m_debugName = name;
    }

protected:
    ShaderBase(const RC<CompiledShader>& compiledShader)
        : m_compiledShader(compiledShader)
    {
    }

    RC<CompiledShader> m_compiledShader;
    Name m_debugName;
};

} // namespace hyperion
