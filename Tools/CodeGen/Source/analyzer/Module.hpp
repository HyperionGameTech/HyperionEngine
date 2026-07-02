/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#ifndef HYPERION_CODEGEN_MODULE_HPP
#define HYPERION_CODEGEN_MODULE_HPP

#include <analyzer/Definitions.hpp>

#include <Core/Containers/Map.hpp>
#include <Core/Containers/Array.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Core/Utilities/Result.hpp>

#include <Core/Threading/Mutex.hpp>

#include <Core/Defines.hpp>

namespace Hyperion {
namespace CodeGen {

class Module
{
public:
    using ClassDefinitionMap = Map<String, ClassDefinition, DynamicAllocator, containers::HashTablePolicy::NotPooled>;

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

    HYP_FORCE_INLINE ClassDefinitionMap& GetClasses()
    {
        return m_classes;
    }

    HYP_FORCE_INLINE const ClassDefinitionMap& GetClasses() const
    {
        return m_classes;
    }

    Result AddClassDefinition(ClassDefinition&& classDefinition);

    const ClassDefinition* FindClassDefinition(UTF8StringView className) const;

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
    ClassDefinitionMap m_classes;
    Array<Module*> m_deps;

    mutable Mutex m_mutex;
};

} // namespace CodeGen
} // namespace Hyperion

#endif
