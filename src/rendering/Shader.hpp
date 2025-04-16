/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_SHADER_HPP
#define HYPERION_SHADER_HPP

#include <core/Base.hpp>
#include <core/Name.hpp>
#include <core/Handle.hpp>
#include <core/threading/Mutex.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <rendering/shader_compiler/ShaderCompiler.hpp>

#include <HashCode.hpp>

namespace hyperion {

struct SubShader
{
};

class HYP_API ShaderManager
{
public:
    static ShaderManager *GetInstance();

    ShaderRef GetOrCreate(const ShaderDefinition &definition);
    ShaderRef GetOrCreate(Name name, const ShaderProperties &props = { });

private:
    struct ShaderMapEntry
    {
        enum class State : uint8
        {
            UNLOADED    = 0,
            LOADING     = 1,
            LOADED      = 2
        };

        ShaderWeakRef       shader;
        AtomicVar<State>    state = State::UNLOADED;
        ThreadID            loading_thread_id;
    };

    HashMap<ShaderDefinition, RC<ShaderMapEntry>>   m_map;
    Mutex                                           m_mutex;
};

} // namespace hyperion

#endif // !HYPERION_SHADER_HPP

