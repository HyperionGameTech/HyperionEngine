#include <script/compiler/Module.hpp>

#include <core/debug/Debug.hpp>

namespace hyperion {

Module::Module(
    const String& name,
    const SourceLocation& location)
    : m_name(name),
      m_location(location),
      importTreeNode(nullptr)
{
}

FlatSet<String> Module::GenerateAllScanPaths() const
{
    FlatSet<String> allScanPaths = m_scanPaths;

    TreeNode<Module*>* top = importTreeNode;

    while (top != nullptr)
    {
        Assert(top->Get() != nullptr);

        for (const auto& otherScanPath : top->Get()->GetScanPaths())
        {
            allScanPaths.Insert(otherScanPath);
        }

        top = top->m_parent;
    }

    return allScanPaths;
}

String Module::GenerateFullModuleName() const
{
    TreeNode<Module*>* top = importTreeNode;

    if (top != nullptr)
    {
        Array<String> parts;
        SizeType size = 0;

        do
        {
            Assert(top->Get() != nullptr);

            parts.PushBack(top->Get()->GetName());
            size += top->Get()->GetName().Size();

            top = top->m_parent;
        }
        while (top != nullptr);

        String res;
        res.Reserve(size + (2 * parts.Size()));

        for (int i = int(parts.Size()) - 1; i >= 0; i--)
        {
            res += parts[i];

            if (i != 0)
            {
                res += "::";
            }
        }

        return res;
    }

    return m_name;
}

bool Module::IsInGlobalScope() const
{
    return scopeTree.TopNode()->m_parent == nullptr;
}

bool Module::IsInScopeOfType(ScopeType scopeType, bool thisScopeOnly) const
{
    const TreeNode<Scope>* top = scopeTree.TopNode();

    while (top != nullptr)
    {
        if (top->Get().scopeType == scopeType)
        {
            return true;
        }

        if (thisScopeOnly)
        {
            return false;
        }

        top = top->m_parent;
    }

    return false;
}

bool Module::IsInScopeOfType(ScopeType scopeType, uint32 scopeFlags, bool thisScopeOnly) const
{
    const TreeNode<Scope>* top = scopeTree.TopNode();

    while (top != nullptr)
    {
        if (top->Get().scopeType == scopeType && (uint32(top->Get().scopeFlags) & scopeFlags) == scopeFlags)
        {
            return true;
        }

        if (thisScopeOnly)
        {
            return false;
        }

        top = top->m_parent;
    }

    return false;
}

Module* Module::LookupNestedModule(const String& name)
{
    Assert(importTreeNode != nullptr);

    // search siblings of the current module,
    // rather than global lookup.
    for (auto* sibling : importTreeNode->m_siblings)
    {
        Assert(sibling != nullptr);
        Assert(sibling->Get() != nullptr);

        if (sibling->Get()->GetName() == name)
        {
            return sibling->Get();
        }
    }

    return nullptr;
}

Array<Module*> Module::CollectNestedModules() const
{
    Assert(importTreeNode != nullptr);

    Array<Module*> nestedModules;

    // search siblings of the current module,
    // rather than global lookup.
    for (auto* sibling : importTreeNode->m_siblings)
    {
        Assert(sibling != nullptr);
        Assert(sibling->Get() != nullptr);

        nestedModules.PushBack(sibling->Get());
    }

    return nestedModules;
}

RC<Identifier> Module::LookUpIdentifier(const String& name, bool thisScopeOnly, bool outsideModules)
{
    TreeNode<Scope>* top = scopeTree.TopNode();

    while (top != nullptr)
    {
        if (RC<Identifier> result = top->Get().identifierTable.LookUpIdentifier(name))
        {
            // a result was found
            return result;
        }

        if (thisScopeOnly)
        {
            return nullptr;
        }

        top = top->m_parent;
    }

    if (outsideModules)
    {
        if (importTreeNode != nullptr && importTreeNode->m_parent != nullptr)
        {
            if (Module* other = importTreeNode->m_parent->Get())
            {
                if (other->GetLocation().GetFileName() == m_location.GetFileName())
                {
                    return other->LookUpIdentifier(name, false);
                }
                else
                {
                    // we are outside of file scope, so loop until root/global module found
                    const TreeNode<Module*>* modLink = importTreeNode->m_parent;

                    while (modLink->m_parent != nullptr)
                    {
                        modLink = modLink->m_parent;
                    }

                    Assert(modLink->Get() != nullptr);
                    Assert(modLink->Get()->GetName() == Config::globalModuleName);

                    return modLink->Get()->LookUpIdentifier(name, false);
                }
            }
        }
    }

    return nullptr;
}

RC<Identifier> Module::LookUpIdentifierDepth(const String& name, int depthLevel)
{
    TreeNode<Scope>* top = scopeTree.TopNode();

    for (int i = 0; top != nullptr && i < depthLevel; i++)
    {
        if (RC<Identifier> result = top->Get().identifierTable.LookUpIdentifier(name))
        {
            return result;
        }

        top = top->m_parent;
    }

    return nullptr;
}

SymbolTypeRef Module::LookupSymbolType(const String& name, bool includePlaceholderTypes)
{
    return PerformLookup<SymbolTypeRef>(
        [&name, includePlaceholderTypes](TreeNode<Scope>* top)
        {
            return top->Get().identifierTable.LookupSymbolType(name, includePlaceholderTypes);
        },
        [&name](Module* mod)
        {
            return mod->LookupSymbolType(name);
        });
}

Variant<RC<Identifier>, SymbolTypeRef> Module::LookUpIdentifierOrSymbolType(const String& name, bool includePlaceholderTypes)
{
    return PerformLookup<Variant<RC<Identifier>, SymbolTypeRef>>(
        [&name, includePlaceholderTypes](TreeNode<Scope>* top) -> Variant<RC<Identifier>, SymbolTypeRef>
        {
            if (RC<Identifier> result = top->Get().identifierTable.LookUpIdentifier(name))
            {
                return Variant<RC<Identifier>, SymbolTypeRef>(result);
            }

            if (SymbolTypeRef symbolType = top->Get().identifierTable.LookupSymbolType(name, includePlaceholderTypes))
            {
                return Variant<RC<Identifier>, SymbolTypeRef>(symbolType);
            }

            return Variant<RC<Identifier>, SymbolTypeRef>();
        },
        [&name, includePlaceholderTypes](Module* mod) -> Variant<RC<Identifier>, SymbolTypeRef>
        {
            return mod->LookUpIdentifierOrSymbolType(name, includePlaceholderTypes);
        });
}

SymbolTypeRef Module::LookupTypeInstance(const TypeInstanceCache::Key& cacheKey, bool deep)
{
    if (deep)
    {
        return PerformLookup<SymbolTypeRef>(
            [&cacheKey](TreeNode<Scope>* top)
            {
                return top->Get().typeInstanceCache.Lookup(cacheKey);
            },
            [&cacheKey](Module* mod)
            {
                return mod->LookupTypeInstance(cacheKey);
            });
    }

    TreeNode<Scope>* top = scopeTree.TopNode();

    if (top != nullptr)
    {
        return top->Get().typeInstanceCache.Lookup(cacheKey);
    }

    return nullptr;
}

void Module::CacheTypeInstance(const TypeInstanceCache::Key& cacheKey, const SymbolTypeRef& type)
{
    // cache in this module at topmost scope
    TreeNode<Scope>* top = scopeTree.TopNode();

    if (top != nullptr)
    {
        top->Get().typeInstanceCache.Put(cacheKey, type);
    }
}

} // namespace hyperion
