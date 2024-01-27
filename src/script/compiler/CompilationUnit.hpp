#ifndef COMPILATION_UNIT_HPP
#define COMPILATION_UNIT_HPP

#include <script/compiler/Module.hpp>
#include <script/compiler/ErrorList.hpp>
#include <script/compiler/builtins/Builtins.hpp>
#include <script/compiler/emit/InstructionStream.hpp>
#include <script/compiler/ast/AstNodeBuilder.hpp>
#include <script/compiler/type-system/SymbolType.hpp>
#include <script/compiler/Tree.hpp>

#include <core/lib/String.hpp>
#include <core/lib/HashMap.hpp>
#include <core/lib/RefCountedPtr.hpp>
#include <core/lib/UniquePtr.hpp>

#include <memory>

namespace hyperion::compiler {

class CompilationUnit
{
public:
    CompilationUnit();
    CompilationUnit(const CompilationUnit &other) = delete;
    ~CompilationUnit();

    Module *GetGlobalModule()
        { return m_global_module.Get(); }

    const Module *GetGlobalModule() const
        { return m_global_module.Get(); }

    Module *GetCurrentModule()
        { return m_module_tree.Top(); }

    const Module *GetCurrentModule() const
        { return m_module_tree.Top(); }

    ErrorList &GetErrorList() { return m_error_list; }
    const ErrorList &GetErrorList() const { return m_error_list; }

    InstructionStream &GetInstructionStream()
        { return m_instruction_stream; }

    const InstructionStream &GetInstructionStream() const
        { return m_instruction_stream; }

    AstNodeBuilder &GetAstNodeBuilder()
        { return m_ast_node_builder; }

    const AstNodeBuilder &GetAstNodeBuilder() const
        { return m_ast_node_builder; }

    const Array<SymbolTypePtr_t> &GetRegisteredTypes() const
        { return m_registered_types; }

    Builtins &GetBuiltins()
        { return m_builtins; }

    const Builtins &GetBuiltins() const
        { return m_builtins; }

    /**
        Allows a non-builtin type to be used
    */
    void RegisterType(const SymbolTypePtr_t &type_ptr);

    /** Looks up the module with the name, taking scope into account.
        Modules with the name that are in the current module or any module
        above the current one will be considered.
    */
    Module *LookupModule(const String &name);

    /** Maps filepath to a vector of modules, so that no module has to be parsed
        and analyze more than once.
    */
    HashMap<String, Array<RC<Module>>>  m_imported_modules;
    Tree<Module*>                       m_module_tree;

private:
    String                  m_exec_path;

    ErrorList               m_error_list;
    InstructionStream       m_instruction_stream;
    AstNodeBuilder          m_ast_node_builder;
    Array<SymbolTypePtr_t>  m_registered_types;
    Builtins                m_builtins;

    // the global module
    RC<Module>              m_global_module;
};

} // namespace hyperion::compiler

#endif
