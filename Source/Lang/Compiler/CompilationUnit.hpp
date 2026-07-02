#pragma once

#include <Lang/Compiler/Module.hpp>
#include <Lang/Compiler/ErrorList.hpp>
#include <Lang/Compiler/Emit/InstructionStream.hpp>
#include <Lang/Compiler/Ast/AstNodeBuilder.hpp>
#include <Lang/Compiler/Tree.hpp>

#include <Core/Containers/String.hpp>
#include <Core/Containers/Map.hpp>

#include <Core/Memory/SharedPtr.hpp>
#include <Core/Memory/UniquePtr.hpp>
#include <Core/Memory/Pimpl.hpp>

#include <Core/Utilities/IndexAllocator.hpp>

namespace Hyperion {

class SymbolType;

class CompilationUnit
{
public:
    CompilationUnit();
    CompilationUnit(const CompilationUnit& other) = delete;
    CompilationUnit& operator=(const CompilationUnit& other) = delete;
    CompilationUnit(CompilationUnit&& other) noexcept = delete;
    CompilationUnit& operator=(CompilationUnit&& other) noexcept = delete;
    ~CompilationUnit();

    HYP_FORCE_INLINE Module* GetGlobalModule() const
    {
        return m_globalModule;
    }

    HYP_FORCE_INLINE Module* GetCurrentModule() const
    {
        return moduleTree.Top();
    }

    HYP_FORCE_INLINE ErrorList& GetErrorList()
    {
        return m_errorList;
    }

    HYP_FORCE_INLINE const ErrorList& GetErrorList() const
    {
        return m_errorList;
    }

    HYP_FORCE_INLINE InstructionStream& GetInstructionStream()
    {
        return m_instructionStream;
    }

    HYP_FORCE_INLINE const InstructionStream& GetInstructionStream() const
    {
        return m_instructionStream;
    }

    HYP_FORCE_INLINE AstNodeBuilder& GetAstNodeBuilder()
    {
        return m_astNodeBuilder;
    }

    HYP_FORCE_INLINE const AstNodeBuilder& GetAstNodeBuilder() const
    {
        return m_astNodeBuilder;
    }

    HYP_FORCE_INLINE String GetAnonClassName()
    {
        return String("@AnonClass") + String::ToString(m_anonClassIndexAllocator.Allocate() + 1);
    }

    /** Looks up the module with the name, taking scope into account.
        Modules with the name that are in the current module or any module
        above the current one will be considered.
    */
    Module* LookupModule(const String& name);

    void RegisterType(SymbolType* symbolType);

    /** Maps filepath to a vector of modules, so that no module has to be parsed
        and analyze more than once.
    */
    Map<String, Array<Module*>> importedModules;
    Array<Module*> ownedModules;
    Tree<Module*> moduleTree;

private:
    String m_execPath;

    ErrorList m_errorList;
    InstructionStream m_instructionStream;
    AstNodeBuilder m_astNodeBuilder;
    Pimpl<class SymbolTypeCache> m_symbolTypeCache;

    IndexAllocator m_anonClassIndexAllocator;

    // the global module
    Module* m_globalModule;
};

} // namespace Hyperion
