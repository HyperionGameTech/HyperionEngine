#include <script/compiler/CompilationUnit.hpp>
#include <script/compiler/Configuration.hpp>

#include <script/compiler/emit/StaticObject.hpp>
#include <script/compiler/emit/NamesPair.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <core/debug/Debug.hpp>

namespace hyperion {

CompilationUnit::CompilationUnit()
    : m_globalModule(new Module(
          hyperion::Config::globalModuleName,
          SourceLocation::eof))
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

} // namespace hyperion
