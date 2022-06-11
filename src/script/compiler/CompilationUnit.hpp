#ifndef COMPILATION_UNIT_HPP
#define COMPILATION_UNIT_HPP

#include <script/compiler/Module.hpp>
#include <script/compiler/ErrorList.hpp>
#include <script/compiler/emit/InstructionStream.hpp>
#include <script/compiler/ast/AstNodeBuilder.hpp>
#include <script/compiler/type-system/SymbolType.hpp>
#include <script/compiler/Tree.hpp>

#include <map>
#include <vector>
#include <memory>
#include <string>

namespace hyperion::compiler {

class CompilationUnit {
public:
    CompilationUnit();
    CompilationUnit(const CompilationUnit &other) = delete;
    ~CompilationUnit();

    inline Module *GetGlobalModule() { return m_global_module.get(); }
    inline const Module *GetGlobalModule() const { return m_global_module.get(); }

    inline Module *GetCurrentModule() { return m_module_tree.Top(); }
    inline const Module *GetCurrentModule() const { return m_module_tree.Top(); }

    inline ErrorList &GetErrorList() { return m_error_list; }
    inline const ErrorList &GetErrorList() const { return m_error_list; }

    inline InstructionStream &GetInstructionStream() { return m_instruction_stream; }
    inline const InstructionStream &GetInstructionStream() const { return m_instruction_stream; }

    inline AstNodeBuilder &GetAstNodeBuilder() { return m_ast_node_builder; }
    inline const AstNodeBuilder &GetAstNodeBuilder() const { return m_ast_node_builder; }

    void BindDefaultTypes();

    /**
        Allows a non-builtin type to be used
    */
    void RegisterType(SymbolTypePtr_t &type_ptr);

    /** Looks up the module with the name, taking scope into account.
        Modules with the name that are in the current module or any module
        above the current one will be considered.
    */
    Module *LookupModule(const std::string &name);

    /** Maps filepath to a vector of modules, so that no module has to be parsed
        and analyze more than once.
    */
    std::map<std::string, std::vector<std::shared_ptr<Module>>> m_imported_modules;
    Tree<Module*> m_module_tree;

    /** all modules contained in the compilation unit */
    //std::vector<std::unique_ptr<Module>> m_modules;
    /** the index of the current, active module in m_modules */
    int m_module_index;

private:
    std::string m_exec_path;

    ErrorList m_error_list;
    InstructionStream m_instruction_stream;
    AstNodeBuilder m_ast_node_builder;
    // the global module
    std::shared_ptr<Module> m_global_module;
};

} // namespace hyperion::compiler

#endif
