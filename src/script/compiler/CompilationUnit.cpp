#include <script/compiler/CompilationUnit.hpp>
#include <script/compiler/Configuration.hpp>
#include <script/compiler/emit/StaticObject.hpp>
#include <script/compiler/emit/NamesPair.hpp>
#include <script/compiler/builtins/Builtins.hpp>
#include <script/compiler/Configuration.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <system/Debug.hpp>

namespace hyperion::compiler {

CompilationUnit::CompilationUnit()
    : m_global_module(new Module(
          hyperion::compiler::Config::global_module_name,
          SourceLocation::eof
      )),
      m_builtins(this)
{
    m_global_module->SetImportTreeLink(m_module_tree.TopNode());

    Scope &top = m_global_module->m_scopes.Top();

    m_module_tree.TopNode()->Get() = m_global_module.Get();
}

CompilationUnit::~CompilationUnit() = default;

void CompilationUnit::RegisterType(const SymbolTypePtr_t &type_ptr)
{
    AssertThrow(type_ptr != nullptr);

    AssertThrowMsg(
        type_ptr->GetTypeObject().Lock() != nullptr,
        "Type object must be assigned to SymbolType before RegisterType() is called. SymbolType name: %s",
        type_ptr->GetName().Data()
    );

    // @TODO Use a more efficient data structure
    // for registered_types like an IntrusiveHashSet or something

    const HashCode type_ptr_hash_code = type_ptr->GetHashCode();

    auto registered_types_it = m_registered_types.FindIf([type_ptr_hash_code](const SymbolTypePtr_t &type)
    {
        return type->GetHashCode() == type_ptr_hash_code;
    });

    if (registered_types_it != m_registered_types.End()) {
        AssertThrow((*registered_types_it)->GetId() != -1);

        // re-use the id because the type is already registered
        type_ptr->SetId((*registered_types_it)->GetId());

        return;
    }

    Array<NamesPair_t> names;

    for (const SymbolTypeMember &member : type_ptr->GetMembers()) {
        const String &member_name = member.name;

        Array<UInt8> array_elements;
        array_elements.Resize(member_name.Size());
        Memory::MemCpy(array_elements.Data(), member_name.Data(), member_name.Size());

        names.PushBack(NamesPair_t {
            member_name.Size(),
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

    m_registered_types.PushBack(type_ptr);
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
