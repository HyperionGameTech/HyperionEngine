#include <script/compiler/ast/AstIdentifier.hpp>
#include <script/compiler/ast/AstTypeObject.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Scope.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <system/Debug.hpp>

#include <iostream>

namespace hyperion::compiler {

AstIdentifier::AstIdentifier(const String &name, const SourceLocation &location)
    : AstExpression(location, ACCESS_MODE_LOAD | ACCESS_MODE_STORE),
      m_name(name)
{
}

void AstIdentifier::PerformLookup(AstVisitor *visitor, Module *mod)
{
    if (auto identifier_or_symbol_type = mod->LookUpIdentifierOrSymbolType(m_name)) {
        if (identifier_or_symbol_type.Is<RC<Identifier>>()) {
            m_properties.m_identifier = identifier_or_symbol_type.Get<RC<Identifier>>();
            m_properties.SetIdentifierType(IDENTIFIER_TYPE_VARIABLE);
        } else if (identifier_or_symbol_type.Is<SymbolTypePtr_t>()) {
            m_properties.m_found_type = identifier_or_symbol_type.Get<SymbolTypePtr_t>();
            m_properties.SetIdentifierType(IDENTIFIER_TYPE_TYPE);
        }

        return;
    }

    if ((m_properties.m_identifier = visitor->GetCompilationUnit()->GetGlobalModule()->LookUpIdentifier(m_name, false))) {
        // if the identifier was not found,
        // look in the global module to see if it is a global function.
        m_properties.SetIdentifierType(IDENTIFIER_TYPE_VARIABLE);
    } else if (mod->LookupNestedModule(m_name) != nullptr) {
        m_properties.SetIdentifierType(IDENTIFIER_TYPE_MODULE);
    } else {
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

        if (top->Get().GetScopeType() == SCOPE_TYPE_FUNCTION) {
            m_properties.m_function_scope = &top->Get();
            m_properties.m_is_in_function = true;

            if (top->Get().GetScopeFlags() & ScopeFunctionFlags::PURE_FUNCTION_FLAG) {
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
    if (const RC<Identifier> &ident = m_properties.GetIdentifier()) {
        if (((ident->GetFlags() & IdentifierFlags::FLAG_CONST) || (ident->GetFlags() & IdentifierFlags::FLAG_GENERIC))
            && !(ident->GetFlags() & IdentifierFlags::FLAG_ARGUMENT)) {
            if (const auto current_value = ident->GetCurrentValue()) {
                if (current_value.Get() == this) {
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
    if (const RC<Identifier> &ident = m_properties.GetIdentifier()) {
        if (((ident->GetFlags() & IdentifierFlags::FLAG_CONST) || (ident->GetFlags() & IdentifierFlags::FLAG_GENERIC))
            && !(ident->GetFlags() & IdentifierFlags::FLAG_ARGUMENT)) {
            if (const auto current_value = ident->GetCurrentValue()) {
                if (current_value.Get() == this) {
                    return this;
                }

                return current_value->GetDeepValueOf();
            }
        }
    }

    return AstExpression::GetDeepValueOf();
}

const String &AstIdentifier::GetName() const
{
    return m_name;
}

SymbolTypePtr_t AstIdentifier::GetHeldType() const
{
    if (m_properties.GetIdentifierType() == IDENTIFIER_TYPE_TYPE) {
        return m_properties.m_found_type;
    }

    return AstExpression::GetHeldType();
}

} // namespace hyperion::compiler
