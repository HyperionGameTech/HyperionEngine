#include <script/compiler/CompilationUnit.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/emit/StaticObject.hpp>
#include <script/compiler/emit/NamesPair.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <core/memory/pool/Pool.hpp>

#include <core/debug/Debug.hpp>

namespace hyperion {

#pragma region SymbolTypeCache

class SymbolTypeCache
{
public:
    SymbolTypeCache()
    {
        ptrs.Reserve(1024);
    }

    ~SymbolTypeCache()
    {
        for (SizeType i = ptrs.Size(); i > 0; i--)
        {
            ptrs[i - 1]->~SymbolTypeRegistration();
        }

        // pool frees itself
    }

    SymbolTypeRegistration* Register(SymbolType* symbolType)
    {
        Assert(symbolType != nullptr);

        SymbolTypeRegistration* pRegistration = (SymbolTypeRegistration*)pool.Allocate(sizeof(SymbolTypeRegistration), alignof(SymbolTypeRegistration));
        new (pRegistration) SymbolTypeRegistration(symbolType);

        ptrs.PushBack(pRegistration);

        return pRegistration;
    }

    Pool pool;
    Array<SymbolTypeRegistration*> ptrs;
};

#pragma endregion SymbolTypeCache

CompilationUnit::CompilationUnit()
    : m_globalModule(new Module(hyperion::Config::globalModuleName, SourceLocation::eof)),
      m_symbolTypeCache(MakePimpl<SymbolTypeCache>())
{
    m_globalModule->SetImportTreeLink(m_moduleTree.TopNode());

    Scope& top = m_globalModule->scopeTree.Top();

    m_moduleTree.TopNode()->Get() = m_globalModule.Get();
}

CompilationUnit::~CompilationUnit() = default;

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

void CompilationUnit::RegisterType(SymbolType* symbolType)
{
    if (!symbolType)
    {
        return;
    }

    m_symbolTypeCache->Register(symbolType);
}

} // namespace hyperion
