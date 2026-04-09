/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/ShaderInstance.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <rendering/dx12/DX12Shared.hpp>

namespace Hyperion {

enum class ShaderModuleType : uint8;

HYP_CLASS(NoScriptBindings)
class DX12ShaderInstance final : public ShaderInstanceBase
{
    HYP_OBJECT_BODY(DX12ShaderInstance);
    
    struct ShaderBlob
    {
        ShaderModuleType type;
        D3D12_SHADER_BYTECODE bytecode;
    };

public:
    DX12ShaderInstance();
    explicit DX12ShaderInstance(const Shader* shader);
    ~DX12ShaderInstance() override;

    ShaderBlob* GetShaderBlob(ShaderModuleType type)
    {
        for (ShaderBlob& sb : m_shaderBlobs)
        {
            if (sb.type == type)
                return &sb;
        }

        return nullptr;
    }

    D3D12_SHADER_BYTECODE GetShaderBytecode(ShaderModuleType type)
    {
        ShaderBlob* blob = GetShaderBlob(type);
        if (!blob)
            return {};

        return blob->bytecode;
    }

    bool IsCreated() const override;

    RendererResult Create() override;

#ifdef HYP_DEBUG_MODE
    void SetDebugName(Name name) override;
#endif

private:
    Array<ShaderBlob> m_shaderBlobs;
};

} // namespace Hyperion
