#pragma once

#include <script/compiler/Scope.hpp>
#include <script/SourceLocation.hpp>
#include <script/compiler/Tree.hpp>
#include <script/compiler/Configuration.hpp>
#include <script/compiler/type-system/SymbolType.hpp>
#include <Core/containers/String.hpp>
#include <Core/containers/Array.hpp>
#include <Core/containers/FlatSet.hpp>
#include <Core/utilities/Optional.hpp>
#include <Core/utilities/Variant.hpp>
#include <Core/functional/Proc.hpp>
#include <Core/Types.hpp>

namespace Hyperion {

class Module
{
public:
    Module(
        const String& name,
        const SourceLocation& location);

    Module(const Module& other) = delete;
    Module& operator=(const Module& other) = delete;
    Module(Module&& other) noexcept = delete;
    Module& operator=(Module&& other) noexcept = delete;

    ~Module();

    HYP_FORCE_INLINE const String& GetName() const
    {
        return m_name;
    }

    HYP_FORCE_INLINE const SourceLocation& GetLocation() const
    {
        return m_location;
    }

    HYP_FORCE_INLINE const FlatSet<String>& GetScanPaths() const
    {
        return m_scanPaths;
    }

    HYP_FORCE_INLINE void AddScanPath(const String& path)
    {
        m_scanPaths.Insert(path);
    }

    HYP_FORCE_INLINE TreeNode<Module*>* GetImportTreeLink() const
    {
        return importTreeNode;
    }

    HYP_FORCE_INLINE void SetImportTreeLink(TreeNode<Module*>* treeLink)
    {
        importTreeNode = treeLink;
    }

    FlatSet<String> GenerateAllScanPaths() const;
    /** Create a string of the module name (including parent module names)
        relative to the global scope */
    String GenerateFullModuleName() const;

    bool IsInGlobalScope() const;

    /** Reverse iterate the scopes starting from the currently opened scope,
        checking if the scope is nested within a scope of the given type. */
    bool IsInScopeOfType(ScopeType scopeType, bool thisScopeOnly) const;

    /** Reverse iterate the scopes starting from the currently opened scope,
        checking if the scope is nested within a scope of the given type.
        Returns true only if the scopeType matches and the scopeFlags match.*/
    bool IsInScopeOfType(ScopeType scopeType, uint32 scopeFlags, bool thisScopeOnly) const;

    /** Look up a child module of this module */
    Module* LookupNestedModule(const String& name);

    Array<Module*> CollectNestedModules() const;

    /** Check to see if the identifier exists in multiple scopes, starting
        from the currently opened scope.
        If thisScopeOnly is set to true, only the current scope will be
        searched.
    */
    RC<Identifier> LookUpIdentifier(
        const String& name,
        bool thisScopeOnly,
        bool outsideModules = HYP_SCRIPT_ALLOW_IDENTIFIERS_OTHER_MODULES);

    /** Check to see if the identifier exists in this scope or above this one.
        Will only search the number of depth levels it is given.
        Pass `1` for this scope only.
    */
    RC<Identifier> LookUpIdentifierDepth(const String& name, int depthLevel);

    /** Look up a symbol in this module by name */
    const SymbolType* LookupSymbolType(
        const String& name,
        bool includePlaceholderTypes = true,
        bool thisScopeOnly = false);

    Variant<RC<Identifier>, const SymbolType*> LookUpIdentifierOrSymbolType(
        const String& name,
        bool includePlaceholderTypes = true,
        bool thisScopeOnly = false,
        bool outsideModules = HYP_SCRIPT_ALLOW_IDENTIFIERS_OTHER_MODULES);

    SymbolType* LookupTypeInstance(const TypeInstanceCache::Key& cacheKey, bool deep = true);
    void CacheTypeInstance(const TypeInstanceCache::Key& cacheKey, SymbolType* type);

    Tree<Scope> scopeTree;
    TreeNode<Module*>* importTreeNode;

    template <class T>
    T PerformLookup(
        Proc<T(TreeNode<Scope>*)>&& pred1,
        Proc<T(Module*)>&& pred2)
    {
        return PerformLookup<T>(scopeTree.TopNode(), std::move(pred1), std::move(pred2));
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

        if (importTreeNode && importTreeNode->m_parent)
        {
            if (Module* other = importTreeNode->m_parent->Get())
            {
                if (other->GetLocation().GetFileName() == m_location.GetFileName())
                {
                    return pred2(other);
                }
                else
                {
                    // we are outside of file scope, so loop until root/global module found
                    auto* link = importTreeNode->m_parent;

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

private:
    String m_name;
    SourceLocation m_location;
    FlatSet<String> m_scanPaths;
};

struct ScopeGuard : TreeNodeGuard<Scope>
{
    ScopeGuard(Module* mod, ScopeType scopeType, int scopeFlags = 0)
        : TreeNodeGuard(&mod->scopeTree, scopeType, scopeFlags)
    {
    }

    ScopeGuard(const ScopeGuard& other) = delete;
    ScopeGuard& operator=(const ScopeGuard& other) = delete;
    ScopeGuard(ScopeGuard&& other) noexcept = delete;
    ScopeGuard& operator=(ScopeGuard&& other) noexcept = delete;
    ~ScopeGuard() = default;
};

} // namespace Hyperion
