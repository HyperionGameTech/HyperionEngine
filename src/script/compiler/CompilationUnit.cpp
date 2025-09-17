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
        : pool(new Pool())
    {
        ptrs.Reserve(1024);
    }

    SymbolTypeCache(const SymbolTypeCache& other) = delete;
    SymbolTypeCache& operator=(const SymbolTypeCache& other) = delete;

    SymbolTypeCache(SymbolTypeCache&& other) noexcept = delete;
    SymbolTypeCache& operator=(SymbolTypeCache&& other) noexcept = delete;

    ~SymbolTypeCache()
    {
        for (SizeType i = ptrs.Size(); i > 0; i--)
        {
            ptrs[i - 1]->~SymbolTypeRegistration();
            pool->Free(ptrs[i - 1]);
        }

        ptrs.Clear();

        // pool frees itself
        delete pool;
        pool = nullptr;

        HYP_BREAKPOINT;
    }

    SymbolTypeRegistration* Register(SymbolType* symbolType)
    {
        Assert(symbolType != nullptr);

        SymbolTypeRegistration* pRegistration = (SymbolTypeRegistration*)pool->Allocate(sizeof(SymbolTypeRegistration), alignof(SymbolTypeRegistration));
        new (pRegistration) SymbolTypeRegistration(symbolType);

        ptrs.PushBack(pRegistration);

        return pRegistration;
    }

    Pool* pool;
    Array<SymbolTypeRegistration*> ptrs;
};

#pragma endregion SymbolTypeCache

CompilationUnit::CompilationUnit()
    : m_globalModule(MakeRefCountedPtr<Module>(hyperion::Config::globalModuleName, SourceLocation::eof)),
      m_symbolTypeCache(MakePimpl<SymbolTypeCache>())
{
    m_globalModule->SetImportTreeLink(m_moduleTree.TopNode());

    Scope& top = m_globalModule->scopeTree.Top();

    m_moduleTree.TopNode()->Get() = m_globalModule.Get();
}

CompilationUnit::~CompilationUnit()
{
    m_symbolTypeCache.Reset();
    m_globalModule.Reset();

#if defined(HYP_SYMBOL_TYPE_DANGLING_PTR_DEBUG) && HYP_SYMBOL_TYPE_DANGLING_PTR_DEBUG
    CheckDanglingSymbolTypes();

    HYP_BREAKPOINT;
#endif
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

void CompilationUnit::RegisterType(SymbolType* symbolType)
{
    if (!symbolType)
    {
        return;
    }

    if (symbolType->IsRegistered())
    {
        return;
    }

    if (const SymbolType* baseType = symbolType->GetBaseType())
    {
        Assert(baseType->IsRegistered());
    }

    m_symbolTypeCache->Register(symbolType);

    for (SymbolTypeMember& member : symbolType->GetMembers())
    {
        RegisterType(member.GetType());
    }

    for (SymbolTypeMember& staticMember : symbolType->GetStaticMembers())
    {
        RegisterType(staticMember.GetType());
    }

    if (symbolType->IsAlias())
    {
        RegisterType(const_cast<SymbolType*>(symbolType->GetAliasInfo().m_aliasee));
    }

    if (symbolType->IsGenericInstanceType())
    {
        for (GenericInstanceTypeInfo::Arg& arg : symbolType->GetGenericInstanceInfo().m_genericArgs)
        {
            RegisterType(const_cast<SymbolType*>(arg.m_type));
        }
    }
}

} // namespace hyperion
