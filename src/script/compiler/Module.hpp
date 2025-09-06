#pragma once

#include <script/compiler/Scope.hpp>
#include <script/SourceLocation.hpp>
#include <script/compiler/Tree.hpp>
#include <script/compiler/Configuration.hpp>
#include <script/compiler/type-system/SymbolType.hpp>
#include <core/containers/String.hpp>
#include <core/containers/Array.hpp>
#include <core/containers/FlatSet.hpp>
#include <core/utilities/Optional.hpp>
#include <core/utilities/Variant.hpp>
#include <core/functional/Proc.hpp>
#include <core/Types.hpp>

namespace hyperion {

class Module
{
public:
    Module(
        const String& name,
        const SourceLocation& location);

    Module(const Module& other) = delete;

    const String& GetName() const
    {
        return m_name;
    }

    const SourceLocation& GetLocation() const
    {
        return m_location;
    }

    const FlatSet<String>& GetScanPaths() const
    {
        return m_scanPaths;
    }

    void AddScanPath(const String& path)
    {
        m_scanPaths.Insert(path);
    }

    TreeNode<Module*>* GetImportTreeLink()
    {
        return m_treeLink;
    }

    const TreeNode<Module*>* GetImportTreeLink() const
    {
        return m_treeLink;
    }

    void SetImportTreeLink(TreeNode<Module*>* treeLink)
    {
        m_treeLink = treeLink;
    }

    FlatSet<String> GenerateAllScanPaths() const;
    /** Create a string of the module name (including parent module names)
        relative to the global scope */
    String GenerateFullModuleName() const;

    bool IsInGlobalScope() const;

    /** Reverse iterate the scopes starting from the currently opened scope,
        checking if the scope is nested within a scope of the given type. */
    bool IsInScopeOfType(ScopeType scopeType) const;

    /** Reverse iterate the scopes starting from the currently opened scope,
        checking if the scope is nested within a scope of the given type.
        Returns true only if the scopeType matches and the scopeFlags match.*/
    bool IsInScopeOfType(ScopeType scopeType, uint32 scopeFlags) const;

    /** Look up a child module of this module */
    Module* LookupNestedModule(const String& name);

    Array<Module*> CollectNestedModules() const;

    /** Check to see if the identifier exists in multiple scopes, starting
        from the currently opened scope.
        If thisScopeOnly is set to true, only the current scope will be
        searched.
    */
    RC<Identifier> LookUpIdentifier(const String& name, bool thisScopeOnly, bool outsideModules = HYP_SCRIPT_ALLOW_IDENTIFIERS_OTHER_MODULES);

    /** Check to see if the identifier exists in this scope or above this one.
        Will only search the number of depth levels it is given.
        Pass `1` for this scope only.
    */
    RC<Identifier> LookUpIdentifierDepth(const String& name, int depthLevel);

    /** Look up a symbol in this module by name */
    SymbolTypeRef LookupSymbolType(const String& name, bool includePlaceholderTypes = true);

    Variant<RC<Identifier>, SymbolTypeRef> LookUpIdentifierOrSymbolType(const String& name, bool includePlaceholderTypes = true);

    SymbolTypeRef LookupGenericInstance(const TypeInstanceCache::Key& cacheKey);
    void CacheTypeInstance(const TypeInstanceCache::Key& cacheKey, const SymbolTypeRef& type);

    Tree<Scope> m_scopes;

    template <class T>
    T PerformLookup(
        Proc<T(TreeNode<Scope>*)>&& pred1,
        Proc<T(Module*)>&& pred2)
    {
        return PerformLookup<T>(m_scopes.TopNode(), std::move(pred1), std::move(pred2));
    }

    template <class T>
    T PerformLookup(
        TreeNode<Scope>* top,
        Proc<T(TreeNode<Scope>*)>&& pred1,
        Proc<T(Module*)>&& pred2)
    {
        while (top)
        {
            if (auto result = pred1(top))
            {
                // a result was found
                return result;
            }

            top = top->m_parent;
        }

        if (m_treeLink && m_treeLink->m_parent)
        {
            if (Module* other = m_treeLink->m_parent->Get())
            {
                if (other->GetLocation().GetFileName() == m_location.GetFileName())
                {
                    return pred2(other);
                }
                else
                {
                    // we are outside of file scope, so loop until root/global module found
                    auto* link = m_treeLink->m_parent;

                    while (link->m_parent)
                    {
                        link = link->m_parent;
                    }

                    Assert(link->Get() != nullptr);
                    Assert(link->Get()->GetName() == Config::globalModuleName);

                    return pred2(link->Get());
                }
            }
        }

        return T {};
    }

    String m_name;
    SourceLocation m_location;

    // module scan paths
    FlatSet<String> m_scanPaths;

    /** A link to where this module exists in the import tree */
    TreeNode<Module*>* m_treeLink;
};

struct ScopeGuard : TreeNodeGuard<Scope>
{
    ScopeGuard(Module* mod, ScopeType scopeType, int scopeFlags)
        : TreeNodeGuard(&mod->m_scopes, scopeType, scopeFlags)
    {
    }

    ScopeGuard(const ScopeGuard& other) = delete;
    ScopeGuard& operator=(const ScopeGuard& other) = delete;
    ScopeGuard(ScopeGuard&& other) noexcept = delete;
    ScopeGuard& operator=(ScopeGuard&& other) noexcept = delete;
    ~ScopeGuard() = default;
};

} // namespace hyperion
