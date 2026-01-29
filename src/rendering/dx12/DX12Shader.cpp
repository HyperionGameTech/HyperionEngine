/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12Shader.hpp>

#include <rendering/util/ShaderCompiler.hpp>

#include <DX12Shader.generated.inl>

namespace Hyperion {

DX12Shader::DX12Shader()
    : ShaderBase()
{
}

DX12Shader::DX12Shader(const RC<CompiledShader>& compiledShader)
    : ShaderBase(compiledShader)
{
#ifdef HYP_DEBUG_MODE
    if (compiledShader != nullptr)
    {
        SetDebugName(compiledShader->GetName());
    }
#endif
}

DX12Shader::~DX12Shader()
{
    // @TODO
}

bool DX12Shader::IsCreated() const
{
    return m_shaderBlobs.Any();
}

RendererResult DX12Shader::Create()
{
    if (IsCreated())
        return {};

    if (!m_compiledShader || !m_compiledShader->IsValid())
        return HYP_MAKE_ERROR(RendererError, "Invalid CompiledShader, cannot create Shader instance!");

    m_shaderBlobs.Clear();

    const Array<ByteBuffer>& modules = m_compiledShader->modules;

    for (SizeType i = 0; i < modules.Size(); i++)
    {
        const ByteBuffer& buffer = modules[i];

        if (buffer.Empty())
            continue;

        ShaderModuleType smt = ShaderModuleType(i);

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
void DX12Shader::SetDebugName(Name name)
{
    // @TODO
}
#endif

} // namespace Hyperion
