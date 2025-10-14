/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BUILDTOOL_MODULE_HPP
#define HYPERION_BUILDTOOL_MODULE_HPP

#include <analyzer/Definitions.hpp>

#include <core/containers/HashMap.hpp>
#include <core/containers/Array.hpp>

#include <core/filesystem/FilePath.hpp>

#include <core/utilities/Result.hpp>

#include <core/threading/Mutex.hpp>

#include <core/Defines.hpp>

namespace hyperion {
namespace buildtool {

class Module
{
public:
    using HypClassDefinitionMap = HashMap<String, HypClassDefinition, DynamicNodeAllocator>;

    explicit Module(const FilePath& path);

    Module(const Module&) = delete;
    Module& operator=(const Module&) = delete;

    Module(Module&&) = delete;
    Module& operator=(Module&&) = delete;

    ~Module() = default;

    HYP_FORCE_INLINE const FilePath& GetPath() const
    {
        return m_path;
    }

    HYP_FORCE_INLINE HypClassDefinitionMap& GetHypClasses()
    {
        return m_hypClasses;
    }

    HYP_FORCE_INLINE const HypClassDefinitionMap& GetHypClasses() const
    {
        return m_hypClasses;
    }

    Result AddHypClassDefinition(HypClassDefinition&& hypClassDefinition);

    const HypClassDefinition* FindHypClassDefinition(UTF8StringView className) const;

    void AddDependencyModule(Module* dep)
    {
        if (!dep)
        {
            return;
        }

        Mutex::Guard guard(m_mutex);

        m_deps.PushBack(dep);
    }

    HYP_FORCE_INLINE const Array<Module*>& GetDependencyModules() const
    {
        return m_deps;
    }

private:
    FilePath m_path;
    HypClassDefinitionMap m_hypClasses;
    Array<Module*> m_deps;

    mutable Mutex m_mutex;
};

} // namespace buildtool
} // namespace hyperion

#endif
