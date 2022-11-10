#include <script/compiler/Module.hpp>

#include <system/Debug.hpp>

namespace hyperion::compiler {

Module::Module(const std::string &name,
    const SourceLocation &location)
    : m_name(name),
      m_location(location),
      m_tree_link(nullptr)
{
}

std::unordered_set<std::string> Module::GenerateAllScanPaths() const
{
    std::unordered_set<std::string> scan_paths;

    scan_paths.insert(
        m_scan_paths.begin(),
        m_scan_paths.end()
    );

    TreeNode<Module*> *top = m_tree_link;
    
    while (top != nullptr) {
        AssertThrow(top->m_value != nullptr);

        scan_paths.insert(
            top->m_value->GetScanPaths().begin(),
            top->m_value->GetScanPaths().end()
        );

        top = top->m_parent;
    }

    return scan_paths;
}

std::string Module::GenerateFullModuleName() const
{
    TreeNode<Module*> *top = m_tree_link;
    
    if (top != nullptr) {
        std::vector<std::string> parts;
        size_t length = 0;

        do {
            AssertThrow(top->m_value != nullptr);

            parts.push_back(top->m_value->GetName());
            length += top->m_value->GetName().length();

            top = top->m_parent;
        } while (top != nullptr);

        std::string res;
        res.reserve(length + (2 * parts.size()));

        for (int i = parts.size() - 1; i >= 0; i--) {
            res.append(parts[i]);
            if (i != 0) {
                res.append("::");
            }
        }

        return res;
    }

    return m_name;
}

bool Module::IsInGlobalScope() const
{
    return m_scopes.TopNode()->m_parent == nullptr;
}

bool Module::IsInScopeOfType(ScopeType scope_type) const
{
    const TreeNode<Scope> *top = m_scopes.TopNode();

    while (top != nullptr) {
        if (top->m_value.GetScopeType() == scope_type) {
            return true;
        }
        
        top = top->m_parent;
    }

    return false;
}

bool Module::IsInScopeOfType(ScopeType scope_type, UInt scope_flags) const
{
    const TreeNode<Scope> *top = m_scopes.TopNode();

    while (top != nullptr) {
        if (top->m_value.GetScopeType() == scope_type && bool(UInt(top->m_value.GetScopeFlags()) & scope_flags)) {
            return true;
        }
        
        top = top->m_parent;
    }

    return false;
}

Module *Module::LookupNestedModule(const std::string &name)
{
    AssertThrow(m_tree_link != nullptr);

    // search siblings of the current module,
    // rather than global lookup.
    for (auto *sibling : m_tree_link->m_siblings) {
        AssertThrow(sibling != nullptr);
        AssertThrow(sibling->m_value != nullptr);
        
        if (sibling->m_value->GetName() == name) {
            return sibling->m_value;
        }
    }

    return nullptr;
}

Identifier *Module::LookUpIdentifier(const std::string &name, bool this_scope_only, bool outside_modules)
{
    TreeNode<Scope> *top = m_scopes.TopNode();

    while (top != nullptr) {
        if (Identifier *result = top->m_value.GetIdentifierTable().LookUpIdentifier(name)) {
            // a result was found
            return result;
        }

        if (this_scope_only) {
            return nullptr;
        }

        top = top->m_parent;
    }

    if (outside_modules) {
        if (m_tree_link != nullptr && m_tree_link->m_parent != nullptr) {
            if (Module *other = m_tree_link->m_parent->m_value) {
                if (other->GetLocation().GetFileName() == m_location.GetFileName()) {
                    return other->LookUpIdentifier(name, false);
                } else {
                    // we are outside of file scope, so loop until root/global module found
                    const TreeNode<Module*> *mod_link = m_tree_link->m_parent;

                    while (mod_link->m_parent != nullptr) {
                        mod_link = mod_link->m_parent;
                    }

                    AssertThrow(mod_link->m_value != nullptr);
                    AssertThrow(mod_link->m_value->GetName() == Config::global_module_name);

                    return mod_link->m_value->LookUpIdentifier(name, false);
                }
            }
        }
    }

    return nullptr;
}

Identifier *Module::LookUpIdentifierDepth(const std::string &name, int depth_level)
{
    TreeNode<Scope> *top = m_scopes.TopNode();

    for (int i = 0; top != nullptr && i < depth_level; i++) {
        Identifier *result = top->m_value.GetIdentifierTable().LookUpIdentifier(name);

        if (result != nullptr) {
            return result;
        }

        top = top->m_parent;
    }

    return nullptr;
}

SymbolTypePtr_t Module::LookupSymbolType(const std::string &name)
{
    return PerformLookup(
        [&name](TreeNode<Scope> *top) {
            return top->m_value.GetIdentifierTable().LookupSymbolType(name);
        },
        [&name](Module *mod) {
            return mod->LookupSymbolType(name);
        }
    );
}

SymbolTypePtr_t Module::LookupGenericInstance(
    const SymbolTypePtr_t &base,
    const std::vector<GenericInstanceTypeInfo::Arg> &params)
{
    return PerformLookup(
        [&base, &params](TreeNode<Scope> *top) {
            return top->m_value.GetIdentifierTable().LookupGenericInstance(base, params);
        },
        [&base, &params](Module *mod) {
            return mod->LookupGenericInstance(base, params);
        }
    );
}

SymbolTypePtr_t Module::PerformLookup(
    std::function<SymbolTypePtr_t(TreeNode<Scope>*)> pred1,
    std::function<SymbolTypePtr_t(Module *mod)> pred2
)
{
    TreeNode<Scope> *top = m_scopes.TopNode();

    while (top) {
        if (SymbolTypePtr_t result = pred1(top)) {
            // a result was found
            return result;
        }
        top = top->m_parent;
    }

    if (m_tree_link && m_tree_link->m_parent) {
        if (Module *other = m_tree_link->m_parent->m_value) {
            if (other->GetLocation().GetFileName() == m_location.GetFileName()) {
                return pred2(other);
            } else {
                // we are outside of file scope, so loop until root/global module found
                auto *link = m_tree_link->m_parent;

                while (link->m_parent) {
                    link = link->m_parent;
                }

                AssertThrow(link->m_value != nullptr);
                AssertThrow(link->m_value->GetName() == Config::global_module_name);

                return pred2(link->m_value);
            }
        }
    }

    return nullptr;
}

} // namespace hyperion::compiler
