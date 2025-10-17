/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Name.hpp>
#include <core/reflection/Handle.hpp>

#include <core/threading/Mutex.hpp>

#include <rendering/RenderObject.hpp>

#include <rendering/shader_compiler/ShaderCompiler.hpp>

#include <core/HashCode.hpp>

namespace hyperion {

struct SubShader
{
};

class HYP_API ShaderManager
{
public:
    static ShaderManager* GetInstance();

    ShaderRef GetOrCreate(const ShaderDefinition& definition);
    ShaderRef GetOrCreate(Name name, const ShaderProperties& props = {});

    SizeType CalculateMemoryUsage() const;

private:
    struct ShaderMapEntry
    {
        enum class State : uint8
        {
            UNLOADED = 0,
            LOADING = 1,
            LOADED = 2
        };

        ShaderWeakRef shader;
        RC<CompiledShader> compiledShader;
        AtomicVar<State> state = State::UNLOADED;
        ThreadId loadingThreadId;
    };

    HashMap<ShaderDefinition, RC<ShaderMapEntry>> m_map;
    mutable Mutex m_mutex;
};

} // namespace hyperion
