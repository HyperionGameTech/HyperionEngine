#include <script/compiler/IdentifierTable.hpp>
#include <script/compiler/Configuration.hpp>

#include <Core/containers/HashSet.hpp>

#include <Core/debug/Debug.hpp>

namespace Hyperion {

int IdentifierTable::CountUsedVariables() const
{
    HashSet<int> usedVariables;

    for (auto& ident : identifiers)
    {
        if (!ScriptConfig::CullUnusedObjects || ident->GetUseCount() > 0)
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
        aliasee->GetFlags() | IdentifierFlags::ALIAS,
        aliasee)));
}

const RC<Identifier>& IdentifierTable::AddIdentifier(
    const String& name,
    int flags,
    const RC<AstExpression>& currentValue,
    const SymbolType* symbolType)
{
    RC<Identifier> identifier(new Identifier(
        name,
        identifierIndex++,
        flags));

    if (currentValue != nullptr)
    {
        identifier->SetCurrentValue(currentValue);

        if (symbolType == nullptr)
        {
            identifier->SetSymbolType(symbolType);
        }
    }

    if (symbolType != nullptr)
    {
        Assert(symbolType->IsRegistered());

        identifier->SetSymbolType(symbolType);
    }

    if (scope != nullptr && identifier->GetDeclScope() == nullptr)
    {
        identifier->SetDeclScope(scope);
    }

    return identifiers.PushBack(identifier);
}

bool IdentifierTable::AddIdentifier(const RC<Identifier>& identifier)
{
    if (!identifier)
    {
        return false;
    }

    if (LookUpIdentifier(identifier->GetName()) != nullptr)
    {
        // already exists
        return false;
    }

    if (scope != nullptr && identifier->GetDeclScope() == nullptr)
    {
        identifier->SetDeclScope(scope);
    }

    identifiers.PushBack(identifier);

    return true;
}

RC<Identifier> IdentifierTable::LookUpIdentifier(const String& name)
{
    for (const RC<Identifier>& identifier : identifiers)
    {
        if (identifier != nullptr)
        {
            if (identifier->GetName() == name)
            {
                return identifier;
            }
        }
    }

    return nullptr;
}

const SymbolType* IdentifierTable::LookupSymbolType(const String& name, bool includePlaceholderTypes) const
{
    for (const SymbolType* symbolType : symbolTypes)
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

void IdentifierTable::AddSymbolType(SymbolType* symbolType)
{
    if (!symbolType)
    {
        return;
    }

    Assert(symbolType->IsRegistered());

    if (!symbolType->m_declScope)
    {
        symbolType->m_declScope = scope;
    }

    auto it = symbolTypes.FindIf([&](const SymbolType* other)
        {
            return other->GetName() == symbolType->GetName();
        });

    if (it != symbolTypes.End())
    {
        // already exists, update
        *it = symbolType;

        return;
    }

    // add to list of types
    symbolTypes.PushBack(symbolType);
}

} // namespace Hyperion
