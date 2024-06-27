/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_SHADER_HPP
#define HYPERION_SHADER_HPP

#include <core/Base.hpp>
#include <core/Name.hpp>
#include <core/Handle.hpp>
#include <core/threading/Mutex.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <util/shader_compiler/ShaderCompiler.hpp>

#include <HashCode.hpp>

namespace hyperion {

using renderer::ShaderObject;
using renderer::ShaderModule;
using renderer::ShaderModuleType;

class Engine;

struct SubShader
{
    ShaderModuleType    type;
    ShaderObject        spirv;

    HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(type);
        hc.Add(spirv);

        return hc;
    }
};

class HYP_API ShaderManagerSystem
{
public:
    static ShaderManagerSystem *GetInstance();

    ShaderRef GetOrCreate(
        const ShaderDefinition &definition
    );

    ShaderRef GetOrCreate(
        Name name,
        const ShaderProperties &props = { }
    );

private:
    HashMap<ShaderDefinition, ShaderWeakRef>    m_map;
    Mutex                                       m_mutex;
};

} // namespace hyperion

#endif // !HYPERION_SHADER_HPP

