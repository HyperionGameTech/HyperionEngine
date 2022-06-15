#include <script/compiler/ast/AstIdentifier.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Scope.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <system/debug.h>

#include <iostream>

namespace hyperion::compiler {

AstIdentifier::AstIdentifier(const std::string &name, const SourceLocation &location)
    : AstExpression(location, ACCESS_MODE_LOAD | ACCESS_MODE_STORE),
      m_name(name)
{
}

void AstIdentifier::PerformLookup(AstVisitor *visitor, Module *mod)
{
    // the variable must exist in the active scope or a parent scope
    if ((m_properties.m_identifier = mod->LookUpIdentifier(m_name, false))) {
        m_properties.SetIdentifierType(IDENTIFIER_TYPE_VARIABLE);
    } else if ((m_properties.m_identifier = visitor->GetCompilationUnit()->GetGlobalModule()->LookUpIdentifier(m_name, false))) {
        // if the identifier was not found,
        // look in the global module to see if it is a global function.
        m_properties.SetIdentifierType(IDENTIFIER_TYPE_VARIABLE);
    } else if (mod->LookupNestedModule(m_name) != nullptr) {
        m_properties.SetIdentifierType(IDENTIFIER_TYPE_MODULE);
    }/* else if ((m_properties.m_found_type = mod->LookupSymbolType(m_name))) {
        m_properties.SetIdentifierType(IDENTIFIER_TYPE_TYPE);
    } */
    else {
        // nothing was found
        m_properties.SetIdentifierType(IDENTIFIER_TYPE_NOT_FOUND);
    }
}

void AstIdentifier::CheckInFunction(AstVisitor *visitor, Module *mod)
{
    m_properties.m_depth = 0;
    TreeNode<Scope> *top = mod->m_scopes.TopNode();
    
    while (top != nullptr) {
        m_properties.m_depth++;

        if (top->m_value.GetScopeType() == SCOPE_TYPE_FUNCTION) {
            m_properties.m_function_scope = &top->m_value;
            m_properties.m_is_in_function = true;

            if (top->m_value.GetScopeFlags() & ScopeFunctionFlags::PURE_FUNCTION_FLAG) {
                m_properties.m_is_in_pure_function = true;
            }
            
            break;
        }

        top = top->m_parent;
    }
}

void AstIdentifier::Visit(AstVisitor *visitor, Module *mod)
{
    if (m_properties.GetIdentifierType() == IDENTIFIER_TYPE_UNKNOWN) {
        PerformLookup(visitor, mod);
    }

    CheckInFunction(visitor, mod);
}

int AstIdentifier::GetStackOffset(int stack_size) const
{
    AssertThrow(m_properties.GetIdentifier() != nullptr);
    return stack_size - m_properties.GetIdentifier()->GetStackLocation();
}

const AstExpression *AstIdentifier::GetValueOf() const
{
    if (const Identifier *ident = m_properties.GetIdentifier()) {
        if (ident->GetFlags() & IdentifierFlags::FLAG_CONST || ident->GetFlags() & IdentifierFlags::FLAG_GENERIC) {
            if (const auto current_value = ident->GetCurrentValue()) {
                if (current_value.get() == this) {
                    return this;
                }

                return current_value->GetValueOf();
            }
        }
    }

    return AstExpression::GetValueOf();
}

const AstExpression *AstIdentifier::GetDeepValueOf() const
{
    if (const Identifier *ident = m_properties.GetIdentifier()) {
        if (ident->GetFlags() & IdentifierFlags::FLAG_CONST || ident->GetFlags() & IdentifierFlags::FLAG_GENERIC) {
            if (const auto current_value = ident->GetCurrentValue()) {
                if (current_value.get() == this) {
                    return this;
                }

                return current_value->GetDeepValueOf();
            }
        }
    }

    return AstExpression::GetDeepValueOf();
}

const AstTypeObject *AstIdentifier::ExtractTypeObject() const
{
    if (const Identifier *ident = m_properties.GetIdentifier()) {
        if (const auto current_value = ident->GetCurrentValue()) {
            if (auto *nested_identifier = dynamic_cast<const AstIdentifier *>(current_value.get())) {
                if (nested_identifier == this) {
                    return nullptr;
                }

                return nested_identifier->ExtractTypeObject();
            } else if (auto *type_object = dynamic_cast<const AstTypeObject *>(current_value.get())) {
                return type_object;
            }
        }
    }

    return nullptr;
}

} // namespace hyperion::compiler
