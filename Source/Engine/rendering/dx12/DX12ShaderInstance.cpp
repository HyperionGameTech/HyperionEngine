/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12ShaderInstance.hpp>

#include <rendering/util/ShaderCompiler.hpp>

#include <rendering/Shader.hpp>

#include <DX12ShaderInstance.generated.inl>

namespace Hyperion {

DX12ShaderInstance::DX12ShaderInstance()
    : ShaderInstanceBase()
{
}

DX12ShaderInstance::DX12ShaderInstance(const Shader* shader)
    : ShaderInstanceBase(shader)
{
#ifdef HYP_DEBUG_MODE
    if (shader != nullptr)
    {
        SetDebugName(shader->name);
    }
#endif
}

DX12ShaderInstance::~DX12ShaderInstance()
{
    // @TODO
}

bool DX12ShaderInstance::IsCreated() const
{
    return m_shaderBlobs.Any();
}

RendererResult DX12ShaderInstance::Create()
{
    if (IsCreated())
        return {};

    if (!m_shader || !m_shader->IsValid())
        return HYP_MAKE_ERROR(RendererError, "Invalid Shader, cannot create ShaderInstance!");

    m_shaderBlobs.Clear();

    for (size_t i = 0; i < m_shader->moduleNames.Size(); i++)
    {
        const ByteBuffer& buffer = m_shader->shaderBlobs[i];

        if (buffer.Empty())
            continue;

        ShaderModuleType smt = m_shader->moduleTypes[i];

        D3D12_SHADER_BYTECODE bytecode {};
        bytecode.pShaderBytecode = buffer.Data();
        bytecode.BytecodeLength = buffer.Size();

        m_shaderBlobs.EmplaceBack(smt, bytecode);
    }

#ifdef HYP_DEBUG_MODE
    if (Name debugName = GetDebugName())
        SetDebugName(debugName);
#endif

    return {};
}

#ifdef HYP_DEBUG_MODE
void DX12ShaderInstance::SetDebugName(Name name)
{
    // @TODO
}
#endif

} // namespace Hyperion
