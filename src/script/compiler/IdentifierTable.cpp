#include <script/compiler/IdentifierTable.hpp>
#include <script/compiler/Configuration.hpp>

#include <core/containers/HashSet.hpp>

#include <core/debug/Debug.hpp>

namespace hyperion {

IdentifierTable::IdentifierTable(Scope* scope)
    : identifierIndex(0),
      scope(scope)
{
}

int IdentifierTable::CountUsedVariables() const
{
    HashSet<int> usedVariables;

    for (auto& ident : identifiers)
    {
        if (!Config::cullUnusedObjects || ident->GetUseCount() > 0)
        {
            if (usedVariables.Find(ident->GetIndex()) == usedVariables.End())
            {
                usedVariables.Insert(ident->GetIndex());
            }
        }
    }

    return (int)usedVariables.Size();
}

const RC<Identifier>& IdentifierTable::AddAlias(const String& name, Identifier* aliasee)
{
    Assert(aliasee != nullptr);

    return identifiers.PushBack(RC<Identifier>(new Identifier(
        name,
        aliasee->GetIndex(),
        aliasee->GetFlags() | FLAG_ALIAS,
        aliasee)));
}

const RC<Identifier>& IdentifierTable::AddIdentifier(
    const String& name,
    int flags,
    RC<AstExpression> currentValue,
    SymbolTypeRef symbolType)
{
    RC<Identifier> ident(new Identifier(
        name,
        identifierIndex++,
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

    return identifiers.PushBack(ident);
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

    identifiers.PushBack(identifier);

    return true;
}

RC<Identifier> IdentifierTable::LookUpIdentifier(const String& name)
{
    for (auto& ident : identifiers)
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

SymbolTypeRef IdentifierTable::LookupSymbolType(const String& name, bool includePlaceholderTypes) const
{
    for (const SymbolTypeRef& symbolType : symbolTypes)
    {
        if (symbolType != nullptr && symbolType->GetName() == name)
        {
            if (!includePlaceholderTypes && symbolType->IsPlaceholderType())
            {
                continue;
            }

            return symbolType;
        }
    }

    return nullptr;
}

void IdentifierTable::AddSymbolType(const SymbolTypeRef& symbolType)
{
    if (!symbolType)
    {
        return;
    }

    if (!symbolType->m_declScope)
    {
        symbolType->m_declScope = scope;
    }

    symbolTypes.PushBack(symbolType);
}

} // namespace hyperion
