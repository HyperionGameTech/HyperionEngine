#ifndef HYPERION_COMPILER_MODULE_HPP
#define HYPERION_COMPILER_MODULE_HPP

#include <script/compiler/Scope.hpp>
#include <script/SourceLocation.hpp>
#include <script/compiler/Tree.hpp>
#include <script/compiler/Configuration.hpp>
#include <script/compiler/type-system/SymbolType.hpp>
#include <core/lib/String.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/FlatSet.hpp>
#include <core/lib/Optional.hpp>
#include <core/lib/Variant.hpp>
#include <core/lib/Proc.hpp>
#include <Types.hpp>

namespace hyperion::compiler {

class Module
{
public:
    Module(
        const String &name,
        const SourceLocation &location
    );

    Module(const Module &other) = delete;

    const String &GetName() const
        { return m_name; }

    const SourceLocation &GetLocation() const
        { return m_location; }
    
    const FlatSet<String> &GetScanPaths() const
        { return m_scan_paths; }

    void AddScanPath(const String &path)
        { m_scan_paths.Insert(path); }

    TreeNode<Module*> *GetImportTreeLink()
        { return m_tree_link; }

    const TreeNode<Module*> *GetImportTreeLink() const
        { return m_tree_link; }

    void SetImportTreeLink(TreeNode<Module*> *tree_link)
        { m_tree_link = tree_link; }

    FlatSet<String> GenerateAllScanPaths() const;
    /** Create a string of the module name (including parent module names)
        relative to the global scope */
    String GenerateFullModuleName() const;

    bool IsInGlobalScope() const;

    /** Reverse iterate the scopes starting from the currently opened scope,
        checking if the scope is nested within a scope of the given type. */
    bool IsInScopeOfType(ScopeType scope_type) const;

    /** Reverse iterate the scopes starting from the currently opened scope,
        checking if the scope is nested within a scope of the given type.
        Returns true only if the scope_type matches and the scope_flags match.*/
    bool IsInScopeOfType(ScopeType scope_type, UInt scope_flags) const;

    /** Look up a child module of this module */
    Module *LookupNestedModule(const String &name);

    Array<Module *> CollectNestedModules() const;

    /** Check to see if the identifier exists in multiple scopes, starting
        from the currently opened scope.
        If this_scope_only is set to true, only the current scope will be
        searched.
    */
    RC<Identifier> LookUpIdentifier(const String &name, bool this_scope_only, bool outside_modules=HYP_SCRIPT_ALLOW_IDENTIFIERS_OTHER_MODULES);
    
    /** Check to see if the identifier exists in this scope or above this one.
        Will only search the number of depth levels it is given.
        Pass `1` for this scope only.
    */
    RC<Identifier> LookUpIdentifierDepth(const String &name, int depth_level);

    /** Look up a symbol in this module by name */
    SymbolTypePtr_t LookupSymbolType(const String &name);

    Variant<RC<Identifier>, SymbolTypePtr_t> LookUpIdentifierOrSymbolType(const String &name);

    Optional<GenericInstanceCache::CachedObject> LookupGenericInstance(const GenericInstanceCache::Key &key);

    Tree<Scope> m_scopes;

private:
    template <class T>
    T PerformLookup(
        Proc<T, TreeNode<Scope> *> &&pred1,
        Proc<T, Module *> &&pred2
    )
    {
        TreeNode<Scope> *top = m_scopes.TopNode();

        while (top) {
            if (auto result = pred1(top)) {
                // a result was found
                return result;
            }

            top = top->m_parent;
        }

        if (m_tree_link && m_tree_link->m_parent) {
            if (Module *other = m_tree_link->m_parent->Get()) {
                if (other->GetLocation().GetFileName() == m_location.GetFileName()) {
                    return pred2(other);
                } else {
                    // we are outside of file scope, so loop until root/global module found
                    auto *link = m_tree_link->m_parent;

                    while (link->m_parent) {
                        link = link->m_parent;
                    }

                    AssertThrow(link->Get() != nullptr);
                    AssertThrow(link->Get()->GetName() == Config::global_module_name);

                    return pred2(link->Get());
                }
            }
        }

        return T { };
    }

    String              m_name;
    SourceLocation      m_location;

    // module scan paths
    FlatSet<String>     m_scan_paths;

    /** A link to where this module exists in the import tree */
    TreeNode<Module*>   *m_tree_link;
};

struct ScopeGuard : TreeNodeGuard<Scope>
{
    ScopeGuard(Module *mod, ScopeType scope_type, int scope_flags)
        : TreeNodeGuard(&mod->m_scopes, scope_type, scope_flags)
    {
    }

    ScopeGuard(const ScopeGuard &other)                 = delete;
    ScopeGuard &operator=(const ScopeGuard &other)      = delete;
    ScopeGuard(ScopeGuard &&other) noexcept             = delete;
    ScopeGuard &operator=(ScopeGuard &&other) noexcept  = delete;
    ~ScopeGuard()                                       = default;
};

} // namespace hyperion::compiler

#endif
