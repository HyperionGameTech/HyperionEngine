/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once
#include <core/memory/ByteBuffer.hpp>
#include <core/memory/RefCountedPtr.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/String.hpp>

#include <core/Defines.hpp>

#include <rendering/RenderObject.hpp>

#include <core/HashCode.hpp>
#include <core/Types.hpp>

namespace Hyperion {

class Shader;

HYP_CLASS(Abstract, NoScriptBindings)
class ShaderInstanceBase : public ObjectBase
{
    HYP_OBJECT_BODY(ShaderInstanceBase);

public:
    static Pool* GetAllocator() { return g_rhiPool; }
    
    ShaderInstanceBase()
        : m_shader(nullptr)
    {
    }

    virtual ~ShaderInstanceBase() override = default;

    HYP_FORCE_INLINE const Shader* GetShader() const
    {
        return m_shader;
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
    explicit ShaderInstanceBase(const Shader* shader)
        : m_shader(shader)
    {
    }

    const Shader* m_shader;
    Name m_debugName;
};

} // namespace Hyperion

#ifndef INCLUDE_FROM_RHI
#define INCLUDE_FROM_RHI_BASE

#if HYP_VULKAN
#include <rendering/vulkan/VulkanShaderInstance.hpp>
#elif HYP_DX12
#include <rendering/dx12/DX12ShaderInstance.hpp>
#endif

#undef INCLUDE_FROM_RHI_BASE
#else
#undef INCLUDE_FROM_RHI
#endif
