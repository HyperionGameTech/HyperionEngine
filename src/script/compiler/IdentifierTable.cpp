#include <script/compiler/IdentifierTable.hpp>
#include <script/compiler/Configuration.hpp>

#include <core/containers/HashSet.hpp>

#include <core/debug/Debug.hpp>

#include <algorithm>

namespace hyperion::compiler {

IdentifierTable::IdentifierTable()
    : m_identifierIndex(0)
{
}

IdentifierTable::IdentifierTable(const IdentifierTable& other)
    : m_identifierIndex(other.m_identifierIndex),
      m_identifiers(other.m_identifiers)
{
}

int IdentifierTable::CountUsedVariables() const
{
    HashSet<int> usedVariables;

    for (auto& ident : m_identifiers)
    {
        if (!Config::cullUnusedObjects || ident->GetUseCount() > 0)
        {
            if (usedVariables.Find(ident->GetIndex()) == usedVariables.End())
            {
                usedVariables.Insert(ident->GetIndex());
            }
        }
    }

    return usedVariables.Size();
}

RC<Identifier> IdentifierTable::AddAlias(const String& name, Identifier* aliasee)
{
    Assert(aliasee != nullptr);

    m_identifiers.PushBack(RC<Identifier>(new Identifier(
        name,
        aliasee->GetIndex(),
        aliasee->GetFlags() | FLAG_ALIAS,
        aliasee)));

    return m_identifiers.Back();
}

RC<Identifier> IdentifierTable::AddIdentifier(
    const String& name,
    int flags,
    RC<AstExpression> currentValue,
    SymbolTypeRef symbolType)
{
    RC<Identifier> ident(new Identifier(
        name,
        m_identifierIndex++,
        flags));

    if (currentValue != nullptr)
    {
        ident->SetCurrentValue(currentValue);

        if (symbolType == nullptr)
        {
            ident->SetSymbolType(symbolType);
        }
    }

    if (symbolType != nullptr)
    {
        ident->SetSymbolType(symbolType);
    }

    m_identifiers.PushBack(ident);

    return m_identifiers.Back();
}

bool IdentifierTable::AddIdentifier(const RC<Identifier>& identifier)
{
    if (identifier == nullptr)
    {
        return false;
    }

    if (auto alreadyExistingIdentifier = LookUpIdentifier(identifier->GetName()))
    {
        return false;
    }

    m_identifiers.PushBack(identifier);

    return true;
}

RC<Identifier> IdentifierTable::LookUpIdentifier(const String& name)
{
    for (auto& ident : m_identifiers)
    {
        if (ident != nullptr)
        {
            if (ident->GetName() == name)
            {
                return ident;
            }
        }
    }

    return nullptr;
}

SymbolTypeRef IdentifierTable::LookupSymbolType(const String& name) const
{
    for (auto& type : m_symbolTypes)
    {
        if (type != nullptr && type->GetName() == name)
        {
            return type;
        }
    }

    return nullptr;
}

void IdentifierTable::AddSymbolType(const SymbolTypeRef& type)
{
    m_symbolTypes.PushBack(type);
}

} // namespace hyperion::compiler
