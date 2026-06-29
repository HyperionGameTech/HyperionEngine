/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <DX12Pch.hpp>

#include <Rendering/DX12/DX12ShaderInstance.hpp>

#include <Rendering/Util/ShaderCompiler.hpp>

#include <Rendering/Shader.hpp>

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
        SetDebugName(shader->baseName);
    }
#endif
}

DX12ShaderInstance::~DX12ShaderInstance()
{
    for (ShaderBlob& blob : m_shaderBlobs)
    {
        if (blob.ownedBytecode)
        {
            g_dx12Pool->Free(const_cast<void*>(blob.ownedBytecode));
        }
    }
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

    auto resGuard = m_shader->GetReadScope();

    for (size_t index = 0; index < m_shader->moduleTypes.Size(); index++)
    {
        ShaderModuleType moduleType;
        String moduleName;
        String entryPointName;
        ConstByteView blob;

        if (!m_shader->GetShaderModuleInfo(index, moduleType, moduleName, entryPointName, blob))
        {
            continue;
        }

        if (blob.Size() == 0)
        {
            continue;
        }

        ubyte* ownedData = (ubyte*)g_dx12Pool->Allocate(blob.Size());
        Assert(ownedData != nullptr);

        Memory::Copy(ownedData, blob.Data(), blob.Size());

        D3D12_SHADER_BYTECODE bytecode {};
        bytecode.pShaderBytecode = ownedData;
        bytecode.BytecodeLength = blob.Size();

        ShaderBlob& shaderBlob = m_shaderBlobs.EmplaceBack();
        shaderBlob.type = moduleType;
        shaderBlob.bytecode = bytecode;
        shaderBlob.ownedBytecode = ownedData;
    }

#ifdef HYP_DEBUG_MODE
    if (Name debugName = GetDebugName())
    {
        SetDebugName(debugName);
    }
#endif

    return {};
}

#ifdef HYP_DEBUG_MODE
void DX12ShaderInstance::SetDebugName(Name name)
{
    ShaderInstanceBase::SetDebugName(name);
}
#endif

} // namespace Hyperion
