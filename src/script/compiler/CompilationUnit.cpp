#include <script/compiler/CompilationUnit.hpp>
#include <script/compiler/Configuration.hpp>
#include <script/compiler/emit/StaticObject.hpp>
#include <script/compiler/emit/NamesPair.hpp>
#include <script/compiler/builtins/Builtins.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <core/debug/Debug.hpp>

namespace hyperion::compiler {

CompilationUnit::CompilationUnit()
    : m_globalModule(new Module(
          hyperion::compiler::Config::globalModuleName,
          SourceLocation::eof)),
      m_builtins(this)
{
    m_globalModule->SetImportTreeLink(m_moduleTree.TopNode());

    Scope& top = m_globalModule->m_scopes.Top();

    m_moduleTree.TopNode()->Get() = m_globalModule.Get();
}

CompilationUnit::~CompilationUnit() = default;

void CompilationUnit::RegisterType(const SymbolTypeRef& typePtr)
{
    Assert(typePtr != nullptr);

    // Assert(
    //     typePtr->GetTypeObject().Lock() != nullptr,
    //     "Type object must be assigned to SymbolType before RegisterType() is called. SymbolType name: %s",
    //     typePtr->GetName().Data());

    // @TODO Use a more efficient data structure
    // for registeredTypes like an IntrusiveHashSet or something

    const HashCode typePtrHashCode = typePtr->GetHashCode();

    auto registeredTypesIt = m_registeredTypes.FindIf([typePtrHashCode](const SymbolTypeRef& type)
        {
            return type->GetHashCode() == typePtrHashCode;
        });

    if (registeredTypesIt != m_registeredTypes.End())
    {
        Assert((*registeredTypesIt)->GetId() != -1);

        // re-use the id because the type is already registered
        typePtr->SetId((*registeredTypesIt)->GetId());

        return;
    }

    Array<NamesPair_t> names;

    for (const SymbolTypeMember& member : typePtr->GetMembers())
    {
        const String& memberName = member.name;

        Array<uint8> arrayElements;
        arrayElements.Resize(memberName.Size());
        Memory::MemCpy(arrayElements.Data(), memberName.Data(), memberName.Size());

        names.PushBack(NamesPair_t {
            memberName.Size(),
            std::move(arrayElements) });
    }

    Assert(typePtr->GetMembers().Size() < hyperion::compiler::Config::maxDataMembers);

    // create static object
    StaticTypeInfo st;
    st.m_size = typePtr->GetMembers().Size();
    st.m_names = names;
    st.m_name = typePtr->GetName();

    int id;

    StaticObject so(st);
    int foundId = m_instructionStream.FindStaticObject(so);
    if (foundId == -1)
    {
        so.m_id = m_instructionStream.NewStaticId();
        m_instructionStream.AddStaticObject(so);
        id = so.m_id;
    }
    else
    {
        id = foundId;
    }

    typePtr->SetId(id);

    m_registeredTypes.PushBack(typePtr);
}

Module* CompilationUnit::LookupModule(const String& name)
{
    TreeNode<Module*>* top = m_moduleTree.TopNode();

    while (top != nullptr)
    {
        if (top->Get() != nullptr && top->Get()->GetName() == name)
        {
            return top->Get();
        }

        // look up module names in the top module's siblings
        for (auto& sibling : top->m_siblings)
        {
            if (sibling != nullptr && sibling->Get() != nullptr)
            {
                if (sibling->Get()->GetName() == name)
                {
                    return sibling->Get();
                }
            }
        }

        top = top->m_parent;
    }

    return nullptr;
}

} // namespace hyperion::compiler
