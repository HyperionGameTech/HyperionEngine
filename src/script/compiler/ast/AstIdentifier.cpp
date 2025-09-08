#include <script/compiler/ast/AstIdentifier.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Module.hpp>
#include <script/compiler/Scope.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <core/debug/Debug.hpp>

#include <iostream>

namespace hyperion {

AstIdentifier::AstIdentifier(const String& name, const SourceLocation& location)
    : AstExpression(location, ACCESS_MODE_LOAD | ACCESS_MODE_STORE),
      m_name(name)
{
}

void AstIdentifier::PerformLookup(AstVisitor* visitor, Module* mod)
{
    if (auto identifierOrSymbolType = mod->LookUpIdentifierOrSymbolType(m_name))
    {
        if (identifierOrSymbolType.Is<RC<Identifier>>())
        {
            m_properties.m_identifier = identifierOrSymbolType.Get<RC<Identifier>>();
            m_properties.SetIdentifierType(IDENTIFIER_TYPE_VARIABLE);
        }
        else if (identifierOrSymbolType.Is<SymbolTypeRef>())
        {
            m_properties.m_foundType = identifierOrSymbolType.Get<SymbolTypeRef>();
            m_properties.SetIdentifierType(IDENTIFIER_TYPE_TYPE);
        }

        return;
    }

    if ((m_properties.m_identifier = visitor->GetCompilationUnit()->GetGlobalModule()->LookUpIdentifier(m_name, false)))
    {
        // if the identifier was not found,
        // look in the global module to see if it is a global function.
        m_properties.SetIdentifierType(IDENTIFIER_TYPE_VARIABLE);
    }
    else if (mod->LookupNestedModule(m_name) != nullptr)
    {
        m_properties.SetIdentifierType(IDENTIFIER_TYPE_MODULE);
    }
    else
    {
        // nothing was found
        m_properties.SetIdentifierType(IDENTIFIER_TYPE_NOT_FOUND);
    }
}

void AstIdentifier::CheckInFunction(AstVisitor* visitor, Module* mod)
{
    m_properties.m_depth = 0;
    TreeNode<Scope>* top = mod->scopeTree.TopNode();

    while (top != nullptr)
    {
        m_properties.m_depth++;

        if (top->Get().scopeType == SCOPE_TYPE_FUNCTION)
        {
            m_properties.m_functionScope = &top->Get();
            m_properties.m_isInFunction = true;

            break;
        }

        top = top->m_parent;
    }
}

void AstIdentifier::Visit(AstVisitor* visitor, Module* mod)
{
    if (m_properties.GetIdentifierType() == IDENTIFIER_TYPE_UNKNOWN)
    {
        PerformLookup(visitor, mod);
    }

    CheckInFunction(visitor, mod);
}

int AstIdentifier::GetStackOffset(int stackSize) const
{
    Assert(m_properties.GetIdentifier() != nullptr);
    return stackSize - m_properties.GetIdentifier()->GetStackLocation();
}

const AstExpression* AstIdentifier::GetValueOf() const
{
    if (const RC<Identifier>& ident = m_properties.GetIdentifier())
    {
        if (((ident->GetFlags() & IdentifierFlags::FLAG_CONST) || (ident->GetFlags() & IdentifierFlags::FLAG_GENERIC))
            && !(ident->GetFlags() & IdentifierFlags::FLAG_ARGUMENT))
        {
            if (const auto currentValue = ident->GetCurrentValue())
            {
                if (currentValue.Get() == this)
                {
                    return this;
                }

                return currentValue->GetValueOf();
            }
        }
    }

    return AstExpression::GetValueOf();
}

const AstExpression* AstIdentifier::GetDeepValueOf() const
{
    if (const RC<Identifier>& ident = m_properties.GetIdentifier())
    {
        if (((ident->GetFlags() & IdentifierFlags::FLAG_CONST) || (ident->GetFlags() & IdentifierFlags::FLAG_GENERIC))
            && !(ident->GetFlags() & IdentifierFlags::FLAG_ARGUMENT))
        {
            if (const auto currentValue = ident->GetCurrentValue())
            {
                if (currentValue.Get() == this)
                {
                    return this;
                }

                return currentValue->GetDeepValueOf();
            }
        }
    }

    return AstExpression::GetDeepValueOf();
}

const String& AstIdentifier::GetName() const
{
    return m_name;
}

SymbolTypeRef AstIdentifier::GetHeldType() const
{
    if (m_properties.GetIdentifierType() == IDENTIFIER_TYPE_TYPE)
    {
        return m_properties.m_foundType;
    }

    return AstExpression::GetHeldType();
}

} // namespace hyperion
