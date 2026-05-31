/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once
#include <Core/Memory/ByteBuffer.hpp>
#include <Core/Memory/RefCountedPtr.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/String.hpp>

#include <Core/Defines.hpp>

#if HYP_ENABLE_SHADER_RELOAD
#include <Core/Utilities/Time.hpp>
#endif

#include <Rendering/Shared.hpp>

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

#if HYP_ENABLE_SHADER_RELOAD
    HYP_FORCE_INLINE Time GetCompiledTimestamp() const
    {
        return m_compiledTimestamp;
    }

    HYP_FORCE_INLINE void SetCompiledTimestamp(Time timestamp)
    {
        m_compiledTimestamp = timestamp;
    }
#endif

#if HYP_DEBUG_MODE
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

#if HYP_ENABLE_SHADER_RELOAD
    Time m_compiledTimestamp;
#endif

#if HYP_DEBUG_MODE
    Name m_debugName;
#endif
};

} // namespace Hyperion

#ifndef INCLUDE_FROM_RHI
#define INCLUDE_FROM_RHI_BASE

#if HYP_VULKAN
#include <Rendering/Vulkan/VulkanShaderInstance.hpp>
#elif HYP_DX12
#include <Rendering/DX12/DX12ShaderInstance.hpp>
#endif

#undef INCLUDE_FROM_RHI_BASE
#else
#undef INCLUDE_FROM_RHI
#endif
