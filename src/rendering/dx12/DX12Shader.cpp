/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12Shader.hpp>

#include <DX12Shader.generated.inl>

namespace Hyperion {

DX12Shader::DX12Shader()
    : ShaderBase()
{
}

DX12Shader::DX12Shader(const RC<CompiledShader>& compiledShader)
    : ShaderBase(compiledShader)
{
}

DX12Shader::~DX12Shader()
{
    // @TODO
}

bool DX12Shader::IsCreated() const
{
    // @TODO
    return false;
}

RendererResult DX12Shader::Create()
{
    // @TODO
    HYPERION_RETURN_OK;
}

} // namespace Hyperion
