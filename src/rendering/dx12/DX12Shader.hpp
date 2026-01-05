/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/Shader.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class DX12Shader final : public ShaderBase
{
    HYP_OBJECT_BODY(DX12Shader);

public:
    DX12Shader();
    explicit DX12Shader(const RC<CompiledShader>& compiledShader);
    ~DX12Shader() override;

    bool IsCreated() const override;

    RendererResult Create() override;

#ifdef HYP_DEBUG_MODE
    void SetDebugName(Name name) override;
#endif

private:
    // @TODO
};

} // namespace Hyperion
