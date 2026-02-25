/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once
#include <Core/memory/ByteBuffer.hpp>
#include <Core/memory/RefCountedPtr.hpp>

#include <Core/containers/Array.hpp>
#include <Core/containers/String.hpp>

#include <Core/Defines.hpp>

#include <rendering/RenderObject.hpp>

#include <Core/HashCode.hpp>
#include <Core/Types.hpp>

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
    
#ifdef HYP_DEBUG_MODE
    Name GetDebugName() const
    {
        return m_debugName;
    }

    virtual void SetDebugName(Name name)
    {
        m_debugName = name;
    }
#endif

protected:
    explicit ShaderInstanceBase(const Shader* shader)
        : m_shader(shader)
    {
    }

    const Shader* m_shader;
    
#ifdef HYP_DEBUG_MODE
    Name m_debugName;
#endif
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
