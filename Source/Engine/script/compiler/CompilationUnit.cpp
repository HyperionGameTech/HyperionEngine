#include <script/compiler/CompilationUnit.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/emit/StaticObject.hpp>
#include <script/compiler/emit/NamesPair.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <Core/memory/allocator/SlabAllocator.hpp>

#include <Core/containers/Array.hpp>

#include <Core/utilities/DeferredScope.hpp>

#include <Core/debug/Debug.hpp>

namespace Hyperion {

#pragma region SymbolTypeCache

class SymbolTypeCache
{
public:
    SymbolTypeCache()
        : allocator(sizeof(SymbolTypeRegistration), alignof(SymbolTypeRegistration))
    {
        ptrs.Reserve(1024);
    }

    SymbolTypeCache(const SymbolTypeCache& other) = delete;
    SymbolTypeCache& operator=(const SymbolTypeCache& other) = delete;

    SymbolTypeCache(SymbolTypeCache&& other) noexcept = delete;
    SymbolTypeCache& operator=(SymbolTypeCache&& other) noexcept = delete;

    ~SymbolTypeCache()
    {
        for (size_t i = ptrs.Size(); i > 0; i--)
        {
            ptrs[i - 1]->~SymbolTypeRegistration();
            allocator.Free(ptrs[i - 1]);
        }

        ptrs.Clear();
    }

    SymbolTypeRegistration* Register(SymbolType* symbolType)
    {
        Assert(symbolType != nullptr);
        Assert(!symbolType->IsRegistered());

        SymbolTypeRegistration* registration = (SymbolTypeRegistration*)allocator.Allocate();
        new (registration) SymbolTypeRegistration(symbolType);

        ptrs.PushBack(registration);

        return registration;
    }

    SlabAllocator allocator;
    Array<SymbolTypeRegistration*> ptrs;
};

#pragma endregion SymbolTypeCache

CompilationUnit::CompilationUnit()
    : m_globalModule(new Module(ScriptConfig::GlobalModuleName, SourceLocation::Eof())),
      m_symbolTypeCache(MakePimpl<SymbolTypeCache>())
{
    m_globalModule->SetImportTreeLink(moduleTree.TopNode());

    Scope& top = m_globalModule->scopeTree.Top();
    moduleTree.TopNode()->Get() = m_globalModule;

    ownedModules.PushBack(m_globalModule);
}

CompilationUnit::~CompilationUnit()
{
    // Modules need to be destructed before our SymbolTypeCache is
    // (otherwise will have assertions fail as SymbolTypes are deleted while in cache)
    // @NOTE: m_globalModule is in ownedModules, so will be deleted in the loop below
    for (Module* module : ownedModules)
    {
        delete module;
    }

    ownedModules.Clear();
    m_globalModule = nullptr;

    importedModules.Clear();

    m_symbolTypeCache.Reset();

#if defined(HYP_SYMBOL_TYPE_UNFREED_PTR_DEBUG) && HYP_SYMBOL_TYPE_UNFREED_PTR_DEBUG
    CheckUnfreedSymbolTypes();
#endif
}

Module* CompilationUnit::LookupModule(const String& name)
{
    TreeNode<Module*>* top = moduleTree.TopNode();

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

    static thread_local Array<SymbolType*> s_visited;

    if (s_visited.Find(symbolType) != s_visited.End())
    {
        // already visited this type, avoid infinite recursion
        return;
    }

    s_visited.PushBack(symbolType);
    HYP_DEFER({
        s_visited.PopBack();
    });

    if (const SymbolType* baseType = symbolType->GetBaseType())
    {
        Assert(baseType->IsRegistered());
    }

    if (symbolType->IsRegistered())
    {
        return;
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

    if (symbolType->IsGenericInstanceType())
    {
        for (GenericInstanceTypeInfo::Arg& arg : symbolType->GetGenericInstanceInfo().m_genericArgs)
        {
            RegisterType(const_cast<SymbolType*>(arg.m_type));
        }
    }

    if (symbolType->IsAlias())
    {
        RegisterType(const_cast<SymbolType*>(symbolType->GetAliasInfo().m_aliasee));
    }
}

} // namespace Hyperion
