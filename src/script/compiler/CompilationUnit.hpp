#pragma once

#include <script/compiler/Module.hpp>
#include <script/compiler/ErrorList.hpp>
#include <script/compiler/builtins/Builtins.hpp>
#include <script/compiler/emit/InstructionStream.hpp>
#include <script/compiler/ast/AstNodeBuilder.hpp>
#include <script/compiler/type-system/SymbolType.hpp>
#include <script/compiler/Tree.hpp>

#include <core/containers/String.hpp>
#include <core/containers/HashMap.hpp>
#include <core/memory/RefCountedPtr.hpp>
#include <core/memory/UniquePtr.hpp>

#include <memory>

namespace hyperion::compiler {

class CompilationUnit
{
public:
    CompilationUnit();
    CompilationUnit(const CompilationUnit& other) = delete;
    ~CompilationUnit();

    Module* GetGlobalModule()
    {
        return m_globalModule.Get();
    }

    const Module* GetGlobalModule() const
    {
        return m_globalModule.Get();
    }

    Module* GetCurrentModule()
    {
        return m_moduleTree.Top();
    }

    const Module* GetCurrentModule() const
    {
        return m_moduleTree.Top();
    }

    ErrorList& GetErrorList()
    {
        return m_errorList;
    }
    const ErrorList& GetErrorList() const
    {
        return m_errorList;
    }

    InstructionStream& GetInstructionStream()
    {
        return m_instructionStream;
    }

    const InstructionStream& GetInstructionStream() const
    {
        return m_instructionStream;
    }

    AstNodeBuilder& GetAstNodeBuilder()
    {
        return m_astNodeBuilder;
    }

    const AstNodeBuilder& GetAstNodeBuilder() const
    {
        return m_astNodeBuilder;
    }

    const Array<SymbolTypeRef>& GetRegisteredTypes() const
    {
        return m_registeredTypes;
    }

    Builtins& GetBuiltins()
    {
        return m_builtins;
    }

    const Builtins& GetBuiltins() const
    {
        return m_builtins;
    }

    /** Looks up the module with the name, taking scope into account.
        Modules with the name that are in the current module or any module
        above the current one will be considered.
    */
    Module* LookupModule(const String& name);

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
    Array<SymbolTypeRef> m_registeredTypes;
    Builtins m_builtins;

    // the global module
    RC<Module> m_globalModule;
};

} // namespace hyperion::compiler
