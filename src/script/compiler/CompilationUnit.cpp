#include <script/compiler/CompilationUnit.hpp>
#include <script/compiler/Configuration.hpp>
#include <script/compiler/emit/StaticObject.hpp>
#include <script/compiler/emit/NamesPair.hpp>
#include <script/compiler/Configuration.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <system/Debug.hpp>

#include <iostream>

namespace hyperion::compiler {

CompilationUnit::CompilationUnit()
    : m_module_index(0),
      m_global_module(new Module(
          hyperion::compiler::Config::global_module_name,
          SourceLocation::eof
      ))
{
    m_global_module->SetImportTreeLink(m_module_tree.TopNode());

    Scope &top = m_global_module->m_scopes.Top();

    top.GetIdentifierTable().BindTypeToIdentifier("Object", BuiltinTypes::OBJECT);
    top.GetIdentifierTable().BindTypeToIdentifier("Any", BuiltinTypes::ANY);
    top.GetIdentifierTable().BindTypeToIdentifier("Maybe", BuiltinTypes::MAYBE);
    top.GetIdentifierTable().BindTypeToIdentifier("Const", BuiltinTypes::CONST_TYPE);
    top.GetIdentifierTable().BindTypeToIdentifier("Block", BuiltinTypes::BLOCK_TYPE);
    top.GetIdentifierTable().BindTypeToIdentifier("Closure", BuiltinTypes::CLOSURE_TYPE);

    m_module_tree.TopNode()->m_value = m_global_module.Get();
}

CompilationUnit::~CompilationUnit()
{
}

void CompilationUnit::RegisterType(SymbolTypePtr_t &type_ptr)
{
    std::vector<NamesPair_t> names;

    for (auto &mem : type_ptr->GetMembers()) {
        std::string mem_name = std::get<0>(mem);

        names.push_back({
            mem_name.size(),
            std::vector<uint8_t>(mem_name.begin(), mem_name.end())
        });
    }

    // mangle the type name
    const SizeType len = type_ptr->GetName().length();

    AssertThrow(type_ptr->GetMembers().size() < hyperion::compiler::Config::max_data_members);

    // create static object
    StaticTypeInfo st;
    st.m_size = type_ptr->GetMembers().size();
    st.m_names = names;
    st.m_name = type_ptr->GetName().c_str();

    int id;

    StaticObject so(st);
    int found_id = m_instruction_stream.FindStaticObject(so);
    if (found_id == -1) {
        so.m_id = m_instruction_stream.NewStaticId();
        m_instruction_stream.AddStaticObject(so);
        id = so.m_id;
    } else {
        id = found_id;
    }

    type_ptr->SetId(id);
}

Module *CompilationUnit::LookupModule(const std::string &name)
{
    TreeNode<Module*> *top = m_module_tree.TopNode();

    while (top != nullptr) {
        if (top->m_value != nullptr && top->m_value->GetName() == name) {
            return top->m_value;
        }

        // look up module names in the top module's siblings
        for (auto &sibling : top->m_siblings) {
            if (sibling != nullptr && sibling->m_value != nullptr) {
                if (sibling->m_value->GetName() == name) {
                    return sibling->m_value;
                }
            }
        }

        top = top->m_parent;
    }

    return nullptr;
}

} // namespace hyperion::compiler
