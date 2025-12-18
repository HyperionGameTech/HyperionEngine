/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Name.hpp>

#include <core/memory/Pimpl.hpp>

#include <rendering/RenderObject.hpp>

#include <rendering/shader_compiler/ShaderCompiler.hpp>

namespace hyperion {

struct ShaderDefinition;
class ShaderProperties;

class ShaderManager
{
public:
    static ShaderManager* GetInstance();

    ShaderManager();

    ShaderRef GetOrCreate(const ShaderDefinition& definition);
    ShaderRef GetOrCreate(Name name, const ShaderProperties& props = {});

    SizeType CalculateMemoryUsage() const;

private:
    Pimpl<class ShaderManagerImpl> m_impl;
};

} // namespace hyperion
