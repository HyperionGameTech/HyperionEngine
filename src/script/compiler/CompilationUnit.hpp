#pragma once

#include <script/compiler/Module.hpp>
#include <script/compiler/ErrorList.hpp>
#include <script/compiler/emit/InstructionStream.hpp>
#include <script/compiler/ast/AstNodeBuilder.hpp>
#include <script/compiler/Tree.hpp>

#include <core/containers/String.hpp>
#include <core/containers/HashMap.hpp>

#include <core/memory/RefCountedPtr.hpp>
#include <core/memory/UniquePtr.hpp>
#include <core/memory/Pimpl.hpp>

#include <core/utilities/IdGenerator.hpp>

namespace hyperion {

class SymbolType;

class CompilationUnit
{
public:
    CompilationUnit();
    CompilationUnit(const CompilationUnit& other) = delete;
    ~CompilationUnit();

    HYP_FORCE_INLINE Module* GetGlobalModule() const
    {
        return m_globalModule.Get();
    }

    HYP_FORCE_INLINE Module* GetCurrentModule()
    {
        return m_moduleTree.Top();
    }

    HYP_FORCE_INLINE const Module* GetCurrentModule() const
    {
        return m_moduleTree.Top();
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
        return String("@AnonClass") + String::ToString(m_anonClassIdGenerator.Next());
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
    HashMap<String, Array<RC<Module>>> m_importedModules;
    Tree<Module*> m_moduleTree;

private:
    String m_execPath;

    ErrorList m_errorList;
    InstructionStream m_instructionStream;
    AstNodeBuilder m_astNodeBuilder;
    Pimpl<class SymbolTypeCache> m_symbolTypeCache;

    IdGenerator m_anonClassIdGenerator;

    // the global module
    RC<Module> m_globalModule;
};

} // namespace hyperion
