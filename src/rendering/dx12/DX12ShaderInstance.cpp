/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12ShaderInstance.hpp>

#include <rendering/util/ShaderCompiler.hpp>

#include <DX12ShaderInstance.generated.inl>

namespace Hyperion {

DX12ShaderInstance::DX12ShaderInstance()
    : ShaderInstanceBase()
{
}

DX12ShaderInstance::DX12ShaderInstance(const RC<CompiledShader>& compiledShader)
    : ShaderInstanceBase(compiledShader)
{
#ifdef HYP_DEBUG_MODE
    if (compiledShader != nullptr)
    {
        SetDebugName(compiledShader->GetName());
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
void DX12ShaderInstance::SetDebugName(Name name)
{
    // @TODO
}
#endif

} // namespace Hyperion
