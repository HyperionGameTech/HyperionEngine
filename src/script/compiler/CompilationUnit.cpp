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

    m_module_tree.TopNode()->Get() = m_global_module.Get();
}

CompilationUnit::~CompilationUnit() = default;

void CompilationUnit::RegisterType(SymbolTypePtr_t &type_ptr)
{
    Array<NamesPair_t> names;

    for (auto &mem : type_ptr->GetMembers()) {
        String mem_name = std::get<0>(mem);

        Array<UInt8> array_elements;
        array_elements.Resize(mem_name.Size());
        Memory::MemCpy(array_elements.Data(), mem_name.Data(), mem_name.Size());

        names.PushBack(NamesPair_t {
            mem_name.Size(),
            std::move(array_elements)
        });
    }

    AssertThrow(type_ptr->GetMembers().Size() < hyperion::compiler::Config::max_data_members);

    // create static object
    StaticTypeInfo st;
    st.m_size = type_ptr->GetMembers().Size();
    st.m_names = names;
    st.m_name = type_ptr->GetName();

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

Module *CompilationUnit::LookupModule(const String &name)
{
    TreeNode<Module *> *top = m_module_tree.TopNode();

    while (top != nullptr) {
        if (top->Get() != nullptr && top->Get()->GetName() == name) {
            return top->Get();
        }

        // look up module names in the top module's siblings
        for (auto &sibling : top->m_siblings) {
            if (sibling != nullptr && sibling->Get() != nullptr) {
                if (sibling->Get()->GetName() == name) {
                    return sibling->Get();
                }
            }
        }

        top = top->m_parent;
    }

    return nullptr;
}

} // namespace hyperion::compiler
