/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once
#include <core/memory/ByteBuffer.hpp>
#include <core/memory/RefCountedPtr.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/String.hpp>

#include <core/Defines.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/RenderDevice.hpp>

#include <HashCode.hpp>
#include <Types.hpp>

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

class ShaderBase : public RenderObject<ShaderBase>
{
public:
    virtual ~ShaderBase() override = default;

    HYP_FORCE_INLINE const RC<CompiledShader>& GetCompiledShader() const
    {
        return m_compiledShader;
    }

    virtual bool IsCreated() const = 0;

    virtual RendererResult Create() = 0;
    virtual RendererResult Destroy() = 0;

protected:
    ShaderBase(const RC<CompiledShader>& compiledShader)
        : m_compiledShader(compiledShader)
    {
    }

    RC<CompiledShader> m_compiledShader;
};

} // namespace hyperion
